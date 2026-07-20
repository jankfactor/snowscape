#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mesh.h"

Mesh g_Mesh;

#define SNOWLEVEL ((7400 * 64) << WORLD_SCALE_SHIFT)
#define SANDLEVEL 0
#define ZBUFFER_SIZE 10000
#define SEED_SIZE 50
#define GENERATED_SIZE 100
#define TERRAIN_MARGIN ((MAPW - GENERATED_SIZE) / 2)
#define HEIGHT_TO_FIX_SHIFT (8 + WORLD_SCALE_SHIFT)
#define HEIGHT_TO_FIX (1 << HEIGHT_TO_FIX_SHIFT)
#define TERRAIN_SHADE_SHIFT (16 - WORLD_SCALE_REDUCTION)

/* Height is a signed 8.8 source word scaled by HEIGHT_TO_FIX. Multiplying the
   source word by the full 16-bit barycentric weight preserves all available
   interpolation precision and cannot overflow a signed 32-bit ARM register. */
#define INTERPOLATE_HEIGHT(height, weight) \
    ((((height) >> HEIGHT_TO_FIX_SHIFT) * (weight)) >> \
     (16 - HEIGHT_TO_FIX_SHIFT))

typedef struct HeightCell
{
    unsigned short height;
    unsigned short detail;
} HeightCell;

/** Interpret an unsigned terrain word with the original signed 16-bit semantics.
 * @param value Raw terrain word.
 * @return The same bit pattern interpreted as a signed short.
 */
static short SignedWord(unsigned short value)
{
    return (short)value;
}

/** Classify a terrain face using its sun-facing slope and deterministic hash.
 * @param riseX Height rise toward the light along X.
 * @param riseZ Height rise toward the light along Z.
 * @param hash Midwinter terrain detail/hash word.
 * @param secondTriangle Non-zero to use the hash byte for the second face.
 * @param base Neutral palette-ramp class.
 * @param shift Fixed-point slope scaling shift.
 * @param minimum Lowest permitted class.
 * @param maximum Highest permitted class.
 * @return Clamped palette-ramp class.
 *
 * Midwinter classifies a face from its slope towards the sun, then uses the
 * terrain hash to break up otherwise solid bands of equal-coloured triangles.
 * The recovered notes do not yet identify the final 3D palette-byte table, so
 * keep Snowscape's palette ramps and reproduce the confirmed classification
 * inputs here. Each half of a cell uses a different byte of the hash word.
 */
static int TerrainShadeClass(fix riseX, fix riseZ, unsigned short hash,
                             int secondTriangle, int base, int shift,
                             int minimum, int maximum)
{
    static const signed char hashVariationTable[4] = {-1, 0, 0, 1};
    unsigned char hashByte;
    int hashVariation;

    hashByte = secondTriangle ? (unsigned char)hash :
                                (unsigned char)(hash >> 8);
    hashVariation = hashVariationTable[(hashByte >> 6) & 3];

    return clamp(base + ((riseX + riseZ) >> shift) + hashVariation,
                 minimum, maximum);
}

/** Generate one deterministic Midwinter midpoint sample from four corners.
 * @param a First corner and hash source.
 * @param b Second corner.
 * @param c Third corner.
 * @param d Fourth corner.
 * @param shift Roughness shift for the current subdivision level.
 * @return Generated height/detail cell.
 */
static HeightCell Midpoint(HeightCell a, HeightCell b, HeightCell c, HeightCell d, int shift)
{
    unsigned int sum;
    unsigned short sumLow;
    short average;
    short base;
    short multiplier;
    int product;
    HeightCell result;

    sum = ((unsigned int)a.height << 16) | a.detail;
    sum += ((unsigned int)b.height << 16) | b.detail;
    sum += ((unsigned int)c.height << 16) | c.detail;
    sum += ((unsigned int)d.height << 16) | d.detail;

    sumLow = (unsigned short)sum;
    average = SignedWord((unsigned short)(sum >> 16)) >> 2;
    base = SignedWord(sumLow) >> shift;
    multiplier = SignedWord((unsigned short)(average * 8 + 0x1388));
    product = (int)base * (int)multiplier;

    result.height = (unsigned short)(average + (product >> 16));
    result.detail = (unsigned short)((sumLow ^ a.detail) & 0x3fff);
    return result;
}

/** Expand a 50x50 terrain window into the game's 100x100 working grid.
 * @param source Source cells in row-major order.
 * @param output Destination buffer for 100x100 cells.
 * @param shift Roughness shift for generated midpoint displacement.
 */
static void SubdivideTerrain(const HeightCell *source, HeightCell *output, int shift)
{
    HeightCell centers[SEED_SIZE];
    HeightCell previousCenters[SEED_SIZE];
    HeightCell horizontal[SEED_SIZE];
    HeightCell vertical[SEED_SIZE];
    HeightCell zero;
    int x, y;

    zero.height = 0;
    zero.detail = 0;
    for (x = 0; x < SEED_SIZE; ++x)
        previousCenters[x] = zero;

    for (y = 0; y < SEED_SIZE; ++y)
    {
        for (x = 0; x < SEED_SIZE - 1; ++x)
        {
            if (y < SEED_SIZE - 1)
            {
                centers[x] = Midpoint(source[y * SEED_SIZE + x],
                                      source[y * SEED_SIZE + x + 1],
                                      source[(y + 1) * SEED_SIZE + x],
                                      source[(y + 1) * SEED_SIZE + x + 1], shift);
            }
            else
            {
                centers[x] = previousCenters[x];
            }
        }
        centers[SEED_SIZE - 1] = centers[SEED_SIZE - 2];

        /* The original's first previous-center row is stale scratch data. Its
           influence is confined to an outer row that no centered zoom keeps. */
        if (y == 0)
            for (x = 0; x < SEED_SIZE; ++x)
                previousCenters[x] = centers[x];

        for (x = 0; x < SEED_SIZE - 1; ++x)
        {
            horizontal[x] = Midpoint(centers[x], previousCenters[x],
                                     source[y * SEED_SIZE + x],
                                     source[y * SEED_SIZE + x + 1], shift);
        }
        horizontal[SEED_SIZE - 1] = horizontal[SEED_SIZE - 2];

        for (x = 0; x < SEED_SIZE - 2; ++x)
        {
            if (y < SEED_SIZE - 1)
            {
                vertical[x] = Midpoint(centers[x], centers[x + 1],
                                       source[y * SEED_SIZE + x + 1],
                                       source[(y + 1) * SEED_SIZE + x + 1], shift);
            }
            else
            {
                vertical[x] = centers[x];
            }
        }
        vertical[SEED_SIZE - 2] = vertical[SEED_SIZE - 3];
        vertical[SEED_SIZE - 1] = vertical[SEED_SIZE - 2];

        for (x = 0; x < SEED_SIZE; ++x)
        {
            output[(y * 2) * GENERATED_SIZE + x * 2] = source[y * SEED_SIZE + x];
            output[(y * 2) * GENERATED_SIZE + x * 2 + 1] = horizontal[x];
            output[(y * 2 + 1) * GENERATED_SIZE + x * 2] =
                (x == 0) ? centers[0] : vertical[x - 1];
            output[(y * 2 + 1) * GENERATED_SIZE + x * 2 + 1] = centers[x];
        }

        for (x = 0; x < SEED_SIZE; ++x)
            previousCenters[x] = centers[x];
    }
}

/** Load ZBUFFER and generate the selected fully zoomed terrain patch.
 * @param baseDirectoryPath RISC OS application path containing ZBUFFER.
 * @param terrain Destination buffer for 100x100 cells.
 * @return Zero on success; non-zero on file or allocation failure.
 */
static int LoadCenteredTerrain(const char *baseDirectoryPath, HeightCell *terrain)
{
    unsigned char bytes[ZBUFFER_SIZE];
    HeightCell *window;
    FILE *file;
    char filename[256];
    int level, shift, x, y, px, py;

    srand((unsigned int)time(NULL));

    if (baseDirectoryPath == NULL)
    {
        printf("Game$Dir is not set.\n");
        return 1;
    }

    sprintf(filename, "%s.assets.ZBUFFER", baseDirectoryPath);
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Failed to open file: %s\n", filename);
        return 1;
    }
    if (fread(bytes, 1, ZBUFFER_SIZE, file) != ZBUFFER_SIZE)
    {
        printf("Failed to read %d bytes from: %s\n", ZBUFFER_SIZE, filename);
        fclose(file);
        return 1;
    }
    fclose(file);

    window = (HeightCell *)malloc(SEED_SIZE * SEED_SIZE * sizeof(HeightCell));
    if (window == NULL)
    {
        printf("Failed to allocate terrain generation window.\n");
        return 1;
    }

    /* ZBUFFER.BIN stores each 16-bit word in big-endian order. */
    for (y = 0; y < SEED_SIZE; ++y)
        for (x = 0; x < SEED_SIZE; ++x)
        {
            int offset = (y * SEED_SIZE + x) * 4;
            window[y * SEED_SIZE + x].height =
                (unsigned short)((bytes[offset] << 8) | bytes[offset + 1]);
            window[y * SEED_SIZE + x].detail =
                (unsigned short)((bytes[offset + 2] << 8) | bytes[offset + 3]);
        }

    /* Generate the main map, then select its centered 50x50 window four
       times: the maximum zoom supported by Midwinter's map pipeline. */
    px = 8 + rand() % (SEED_SIZE - 8);
    py = 8 + rand() % (SEED_SIZE - 8);
    for (level = 0; level <= 4; ++level)
    {
        shift = (level < 2) ? 4 : level + 3;
        SubdivideTerrain(window, terrain, shift);
        if (level < 4)
            for (y = 0; y < SEED_SIZE; ++y)
                for (x = 0; x < SEED_SIZE; ++x)
                    window[y * SEED_SIZE + x] =
                        terrain[(py + y) * GENERATED_SIZE + px + x];
    }

    free(window);
    return 0;
}

/** Construct world vertices and shaded checkerboard faces from ZBUFFER terrain.
 * @param baseDirectoryPath RISC OS application path containing terrain assets.
 * @return Zero on success; non-zero if terrain loading or allocation fails.
 */
int GenerateTerrain(const char *baseDirectoryPath)
{
    int i, j, k;
    int terrainX, terrainZ;
    unsigned short terrainHash;
    HeightCell *terrain;
    TRI face;
    int facecounter = 0;
    fix tl, tr, bl, br;

#ifdef PAL_256
    unsigned char range[48] = {49, 49, 49, 49, 89, 89, 89, 89, 90, 90, 91, 91, 92, 92, 118, 118,
                               32, 32, 32, 32, 35, 35, 40, 40, 68, 68, 68, 112, 112, 115, 115, 113,
                               160, 160, 160, 160, 161, 161, 161, 161, 194, 194, 194, 194, 247, 247, 255, 255};
#endif // PAL_256

    terrain = (HeightCell *)malloc(GENERATED_SIZE * GENERATED_SIZE * sizeof(HeightCell));
    if (terrain == NULL)
    {
        printf("Failed to allocate generated terrain.\n");
        return 1;
    }
    if (LoadCenteredTerrain(baseDirectoryPath, terrain) != 0)
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
            terrainX = clamp(i - TERRAIN_MARGIN, 0, GENERATED_SIZE - 1);
            terrainZ = clamp(j - TERRAIN_MARGIN, 0, GENERATED_SIZE - 1);
            /* Preserve Midwinter's signed 8.8 height, then apply the uniform
               internal world scale used by X and Z. */
            g_Mesh.verts[IX(i, j)].y = ((fix)SignedWord(
                terrain[terrainZ * GENERATED_SIZE + terrainX].height)) *
                HEIGHT_TO_FIX;
        }
    for (j = 0; j < MAPW; ++j)
        for (i = 0; i < MAPW; ++i)
        {
            tl = g_Mesh.verts[IX(i, j)].y;
            tr = g_Mesh.verts[IX(i + 1, j)].y;
            bl = g_Mesh.verts[IX(i, j + 1)].y;
            br = g_Mesh.verts[IX(i + 1, j + 1)].y;
            terrainX = clamp(i - TERRAIN_MARGIN, 0, GENERATED_SIZE - 1);
            terrainZ = clamp(j - TERRAIN_MARGIN, 0, GENERATED_SIZE - 1);
            terrainHash = terrain[terrainZ * GENERATED_SIZE + terrainX].detail;

            // Midwinter used a |/|\| pattern for the terrain.
            //                  |\|/|
            if ((i + j) & 1)
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
#ifdef PAL_256
                k = TerrainShadeClass(tl - tr, tl - bl, terrainHash, 0,
                                      12, TERRAIN_SHADE_SHIFT, 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = TerrainShadeClass(tl - tr, tl - bl, terrainHash, 0,
                                      6, TERRAIN_SHADE_SHIFT + 2, 2, 10);
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

#ifdef PAL_256
                k = TerrainShadeClass(bl - br, tr - br, terrainHash, 1,
                                      12, TERRAIN_SHADE_SHIFT, 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = TerrainShadeClass(bl - br, tr - br, terrainHash, 1,
                                      6, TERRAIN_SHADE_SHIFT + 2, 2, 10);
                face.flags = k;
#endif // PAL_256

                g_Mesh.faces[facecounter++] = face;
            }
            else
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

#ifdef PAL_256
                k = TerrainShadeClass(tl - tr, tr - br, terrainHash, 0,
                                      12, TERRAIN_SHADE_SHIFT, 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = TerrainShadeClass(tl - tr, tr - br, terrainHash, 0,
                                      6, TERRAIN_SHADE_SHIFT + 1, 2, 10);
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

#ifdef PAL_256
                k = TerrainShadeClass(bl - br, tl - bl, terrainHash, 1,
                                      12, TERRAIN_SHADE_SHIFT, 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = TerrainShadeClass(bl - br, tl - bl, terrainHash, 1,
                                      6, TERRAIN_SHADE_SHIFT + 1, 2, 10);
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
    else // Top Right / Bottom Left
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

    return A + B + C + (CELL_QUARTER_FIX >> 1);
}
