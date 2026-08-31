#include <stdio.h>
#include <stdlib.h>
#include "mesh.h"
#include "terrain.h"

Mesh g_Mesh;

#define SNOWLEVEL ((7400 * 64) << WORLD_SCALE_SHIFT)
#define SANDLEVEL 0
#define TERRAIN_MARGIN ((MAPW - TERRAIN_GRID_SIZE) / 2)
#define HEIGHT_TO_FIX_SHIFT (9 + WORLD_SCALE_SHIFT)
#define HEIGHT_TO_FIX (1 << HEIGHT_TO_FIX_SHIFT)

/* Height is a signed 8.8 source word scaled by HEIGHT_TO_FIX. Multiplying the
   source word by the full 16-bit barycentric weight preserves all available
   interpolation precision and cannot overflow a signed 32-bit ARM register. */
#define INTERPOLATE_HEIGHT(height, weight)             \
    ((((height) >> HEIGHT_TO_FIX_SHIFT) * (weight)) >> \
     (16 - HEIGHT_TO_FIX_SHIFT))

/** Interpret the low word of an arithmetic result as a signed 16-bit value. */
static int Signed16(int value)
{
    value &= 0xffff;
    if (value & 0x8000)
        value -= 0x10000;
    return value;
}

/** Return the integer square root of an unsigned 32-bit value. */
static unsigned int IntegerSquareRoot32(unsigned int value)
{
    unsigned int bit = 1u << 30;
    unsigned int root = 0;

    while (bit > value)
        bit >>= 2;

    while (bit != 0)
    {
        if (value >= root + bit)
        {
            value -= root + bit;
            root = (root >> 1) + bit;
        }
        else
        {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/** Calculate Midwinter's Lambert index for a terrain triangle.
 * @param triangleHeight Height of any vertex joined by the two slope terms.
 * @param slopeX Signed 16-bit terrain-normal X component.
 * @param slopeY Signed 16-bit terrain-normal Y component.
 * @return In 256-colour mode, an index from 0 through 15. In 16-colour mode,
 *         an index from 2 through 23, or 24 for flat sea.
 *
 * The unnormalised terrain normal is (slopeX, slopeY, 256), and the effective
 * light vector is (0, 3, 2). Midwinter performs the intermediate arithmetic
 * with signed 16-bit slope and dot-product values.
 */
static int TerrainLightIndex(int triangleHeight, int slopeX, int slopeY)
{
    unsigned int normalLength;
    unsigned int normalLengthSquared;
    unsigned int slopeXSquared;
    unsigned int slopeYSquared;
    int dotTerm;
    int numerator;
    int light;

    slopeX = Signed16(slopeX);
    slopeY = Signed16(slopeY);

#ifndef PAL_256
    if (triangleHeight == 0 && slopeX == 0 && slopeY == 0)
        return 24;
#else
    (void)triangleHeight;
#endif

    slopeXSquared = (unsigned int)(slopeX * slopeX);
    slopeYSquared = (unsigned int)(slopeY * slopeY);
    normalLengthSquared = slopeXSquared + slopeYSquared + 256u * 256u;
    normalLength = IntegerSquareRoot32(normalLengthSquared);

    dotTerm = Signed16(3 * slopeY + 2 * 256);
#ifdef PAL_256
    /* A scale of six maps a level normal to index 12, matching the original
       256-colour terrain bias while retaining the complete 0..15 range. */
    numerator = dotTerm * 6;
#else
    numerator = dotTerm * 8;
#endif // PAL_256
    if (numerator < 0)
        light = -((-numerator) / (int)normalLength);
    else
        light = numerator / (int)normalLength;

#ifdef PAL_256
    return min(max(light, 0), 15);
#else
    return min(max(light, 0) + 2, 23);
#endif // PAL_256
}

/** Construct world vertices and shaded checkerboard faces from selected terrain.
 * @param source Loaded pristine Midwinter terrain seed.
 * @param path Zoom-window path selected on the map screen.
 * @return Zero on success; non-zero if terrain loading or allocation fails.
 */
int GenerateTerrain(const TerrainSource *source, const TerrainZoomPath *path)
{
    int i, j, k;
    int terrainX, terrainZ;
    HeightCell *terrain;
    TRI face;
    int facecounter = 0;
    fix tl, tr, bl, br;
    int tlHeight, trHeight, blHeight, brHeight;

#ifdef PAL_256
    unsigned char range[48] = {49, 49, 49, 49, 89, 89, 89, 89, 90, 90, 91, 91, 92, 92, 118, 118,
                               32, 32, 32, 32, 35, 35, 40, 40, 68, 68, 68, 112, 112, 115, 115, 113,
                               160, 160, 160, 160, 161, 161, 161, 161, 194, 194, 194, 194, 247, 247, 255, 255};
#endif // PAL_256

    terrain = (HeightCell *)malloc(TERRAIN_GRID_SIZE * TERRAIN_GRID_SIZE *
                                   sizeof(HeightCell));
    if (terrain == NULL)
    {
        printf("Failed to allocate generated terrain.\n");
        return 1;
    }
    if (TerrainBuildGrid(source, path, 1, terrain) != 0)
    {
        free(terrain);
        return 1;
    }

    printf("Reserving %d bytes for Terrain Vertices...\n", MAPW * MAPW * sizeof(V3D));
    cvector_reserve(g_Mesh.verts, MAPW * MAPW);
    printf("Reserving %d bytes for Terrain Faces...\n", (MAPW * MAPW) * 2 * sizeof(TRI));
    cvector_reserve(g_Mesh.faces, (MAPW * MAPW) * 2);
    printf("Reserving %d bytes for transformed Terrain Vertices...\n", MAPW * MAPW * sizeof(V3D));
    cvector_reserve(g_Mesh.verts_transformed, MAPW * MAPW);

    for (j = 0; j < MAPW; ++j)
        for (i = 0; i < MAPW; ++i)
        {
            g_Mesh.verts[IX(i, j)].x = i * CELL_SIZE_FIX;
            g_Mesh.verts[IX(i, j)].z = j * CELL_SIZE_FIX;
            terrainX = clamp(i - TERRAIN_MARGIN, 0, TERRAIN_GRID_SIZE - 1);
            terrainZ = clamp(j - TERRAIN_MARGIN, 0, TERRAIN_GRID_SIZE - 1);
            /* Preserve Midwinter's signed 8.8 height, then apply the uniform
               internal world scale used by X and Z. */
            g_Mesh.verts[IX(i, j)].y = ((fix)TerrainSignedHeight(
                                           terrain[terrainZ * TERRAIN_GRID_SIZE + terrainX].height)) *
                                       HEIGHT_TO_FIX;
        }
    for (j = 0; j < MAPW; ++j)
        for (i = 0; i < MAPW; ++i)
        {
            tl = g_Mesh.verts[IX(i, j)].y;
            tr = g_Mesh.verts[IX(i + 1, j)].y;
            bl = g_Mesh.verts[IX(i, j + 1)].y;
            br = g_Mesh.verts[IX(i + 1, j + 1)].y;
            tlHeight = tl / HEIGHT_TO_FIX;
            trHeight = tr / HEIGHT_TO_FIX;
            blHeight = bl / HEIGHT_TO_FIX;
            brHeight = br / HEIGHT_TO_FIX;

            // Midwinter used a |\|/| pattern for the terrain.
            //                  |/|\|
            if ((i + j) & 1)
            {
                /* TR
                   ___
                   \  |
                    \ |
                     \|
                */

                face.a = i + j * (MAPW);
                face.b = (i + 1) + j * (MAPW);
                face.c = (i + 1) + (j + 1) * (MAPW);
                face.next = NULL;
                face.centerpoint.x = i * CELL_SIZE_FIX + CELL_THREE_QUARTER_FIX;
                face.centerpoint.y = j * CELL_SIZE_FIX + CELL_QUARTER_FIX;

                k = TerrainLightIndex(tlHeight, tlHeight - trHeight,
                                      trHeight - brHeight);
#ifdef PAL_256
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                face.flags = k;
#endif // PAL_256

                g_Mesh.faces[facecounter++] = face;

                /* BL

                   |\
                   | \
                   |__\
                */

                face.a = i + j * (MAPW);
                face.b = (i + 1) + (j + 1) * (MAPW);
                face.c = i + (j + 1) * (MAPW);
                face.next = NULL;
                face.centerpoint.x = i * CELL_SIZE_FIX + CELL_QUARTER_FIX;
                face.centerpoint.y = j * CELL_SIZE_FIX + CELL_THREE_QUARTER_FIX;

                k = TerrainLightIndex(blHeight, blHeight - brHeight,
                                      tlHeight - blHeight);
#ifdef PAL_256
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                face.flags = k;
#endif // PAL_256

                g_Mesh.faces[facecounter++] = face;
            }
            else
            {
                /* TL
                    ___
                   |  /
                   | /
                   |/
                */

                face.a = i + j * (MAPW);
                face.b = (i + 1) + j * (MAPW);
                face.c = i + (j + 1) * (MAPW);
                face.next = NULL;
                face.centerpoint.x = i * CELL_SIZE_FIX + CELL_QUARTER_FIX;
                face.centerpoint.y = j * CELL_SIZE_FIX + CELL_QUARTER_FIX;
                k = TerrainLightIndex(tlHeight, tlHeight - trHeight,
                                      tlHeight - blHeight);
#ifdef PAL_256
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                face.flags = k;
#endif // PAL_256

                g_Mesh.faces[facecounter++] = face;

                /* BR

                     /|
                    / |
                   /__|
                */

                face.a = (i + 1) + (j + 1) * (MAPW);
                face.b = i + (j + 1) * (MAPW);
                face.c = (i + 1) + j * (MAPW);
                face.next = NULL;
                face.centerpoint.x = i * CELL_SIZE_FIX + CELL_THREE_QUARTER_FIX;
                face.centerpoint.y = j * CELL_SIZE_FIX + CELL_THREE_QUARTER_FIX;

                k = TerrainLightIndex(brHeight, blHeight - brHeight,
                                      trHeight - brHeight);
#ifdef PAL_256
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                face.flags = k;
#endif // PAL_256

                g_Mesh.faces[facecounter++] = face;
            }
        }

    free(terrain);

    // printf("Preparing Lighting pass...\n");
    // light.x = int2fix(-120);
    // light.y = int2fix(420);
    // light.z = int2fix(120);
    // Normalize(&light);

    // for (i = 0; i < (_MAPW * _MAPW) * 2; ++i)
    // {
    //     // _verts[0] = g_Mesh.verts[g_Mesh.faces[i].a];
    //     // _verts[1] = g_Mesh.verts[g_Mesh.faces[i].b];
    //     // _verts[2] = g_Mesh.verts[g_Mesh.faces[i].c];
    //     // Normal(&_verts[0], &_verts[2], &_verts[1], &g_Mesh.faces[i].normal);
    //     // Normalize(&g_Mesh.faces[i].normal);

    //     g_Mesh.faces[i].flags = ; //  min(max(0, (int)(fix2float(DotProduct(&g_Mesh.faces[i].normal, &light)) * 16.f)),15);
    // }
    // printf("Done.\n");
    return 0;
}

/** Convert the selected final-grid cell to local mesh space for the camera.
 * @param path Completed zoom path and 16.16 final-grid player coordinate.
 * @param eyePos Destination position; Y is placed at standing terrain height.
 */
void SetTerrainPlayerStart(const TerrainZoomPath *path, V3D *eyePos)
{
    int gridX = 50 << 16;
    int gridZ = 50 << 16;

    if (path != NULL)
    {
        gridX = path->playerGridX;
        gridZ = path->playerGridY;
    }
    eyePos->x = ((TERRAIN_MARGIN << 16) + gridX) << TILESHIFT;
    eyePos->z = ((TERRAIN_MARGIN << 16) + gridZ) << TILESHIFT;
    eyePos->y = GetHeight(eyePos);
}

/** Release the global mesh's vertex, face, and transformed-vertex arrays. */
void DeAllocateTerrain(void)
{
    cvector_free(g_Mesh.verts);
    cvector_free(g_Mesh.faces);
    cvector_free(g_Mesh.verts_transformed);
}

/** Interpolate the triangle under a position and add standing clearance.
 * @param eyePos World position used for its X and Z coordinates.
 * @return Camera height in 16.16 fixed point.
 */
fix GetHeight(V3D *eyePos)
{
    fix A, B, C;
    fix mapX, mapZ;
    fix localX, localZ;

    // Get the fixed normalized location
    mapX = eyePos->x >> TILESHIFT;
    mapZ = eyePos->z >> TILESHIFT;

    // Obtain the fractional part.
    localX = mapX & 0xFFFF;
    localZ = mapZ & 0xFFFF;

    // Now get map coords as integer.
    mapX >>= 16;
    mapZ >>= 16;

    if ((mapX + mapZ) & 1) // Top Left / Bottom Right
    {

        //  ___
        // |\  |
        // | \ |
        // |__\|
        // 2 shared corners

        B = g_Mesh.verts[IX(mapX, mapZ)].y;
        C = g_Mesh.verts[IX(mapX + 1, mapZ + 1)].y;

        if (localX > localZ)
        {
            // Top Right Triangle
            localX = 65536 - localX;
            A = g_Mesh.verts[IX(mapX + 1, mapZ)].y;
            B = INTERPOLATE_HEIGHT(B, localX);
            C = INTERPOLATE_HEIGHT(C, localZ);
        }
        else
        {
            // Bottom Left Triangle
            // Flip the local coords
            localZ = 65536 - localZ;
            A = g_Mesh.verts[IX(mapX, mapZ + 1)].y;
            B = INTERPOLATE_HEIGHT(B, localZ);
            C = INTERPOLATE_HEIGHT(C, localX);
        }

        A = INTERPOLATE_HEIGHT(A, (65536 - localX - localZ));
    }
    else // Top Right / Bottom Left
    {
        //  ___
        // |  /|
        // | / |
        // |/__|

        // 2 shared corners
        B = g_Mesh.verts[IX(mapX + 1, mapZ)].y;
        C = g_Mesh.verts[IX(mapX, mapZ + 1)].y;

        if ((localX + localZ) < 65536)
        {
            // Top Left Triangle
            A = g_Mesh.verts[IX(mapX, mapZ)].y;
            B = INTERPOLATE_HEIGHT(B, localX);
            C = INTERPOLATE_HEIGHT(C, localZ);
        }
        else
        {
            // Bottom Right Triangle
            // Flip the local coords
            localX = 65536 - localX;
            localZ = 65536 - localZ;
            A = g_Mesh.verts[IX(mapX + 1, mapZ + 1)].y;
            B = INTERPOLATE_HEIGHT(B, localZ);
            C = INTERPOLATE_HEIGHT(C, localX);
        }

        A = INTERPOLATE_HEIGHT(A, (65536 - localX - localZ));
    }

    return A + B + C + (CELL_QUARTER_FIX >> 2);
}
