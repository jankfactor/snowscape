#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mesh.h"

#define rand32(max) (((rand() << 16) | rand()) % (max))
#define rand32balanced(max) ((((rand() << 16) | rand()) % (max)) - ((max) >> 1))

Mesh g_Mesh;

#define SNOWLEVEL -655360
#define SANDLEVEL -3276800

void GenerateTerrain()
{
    int i, j, k, bsize, csize, rnd;
    V3D vertex, light;
    V3D _verts[4];
    V3D *ptrV3D = 0;
    TRI face;
    int facecounter = 0;
    fix tl, tr, bl, br;

#ifdef PAL_256
    unsigned char range[48] = {49, 49, 49, 49, 89, 89, 89, 89, 90, 90, 91, 91, 92, 92, 118, 118,
                               32, 32, 32, 32, 35, 35, 40, 40, 68, 68, 68, 112, 112, 115, 115, 113,
                               160, 160, 160, 160, 161, 161, 161, 161, 194, 194, 194, 194, 247, 247, 255, 255};
#endif // PAL_256

    // srand(time(NULL));
    srand(55555);

    printf("Reserving %d bytes for Terrain Vertices...\n", MAPW * MAPW * sizeof(V3D));
    cvector_reserve(g_Mesh.verts, MAPW * MAPW);
    printf("Reserving %d bytes for Terrain Faces...\n", (MAPW * MAPW) * 2 * sizeof(TRI));
    cvector_reserve(g_Mesh.faces, (MAPW * MAPW) * 2);
    printf("Reserving %d bytes for transformed Terrain Vertices...\n", MAPW * MAPW * sizeof(V3D));
    cvector_reserve(g_Mesh.verts_transformed, MAPW * MAPW);

    for (j = 0; j < MAPW; ++j)
        for (i = 0; i < MAPW; ++i)
        {
            g_Mesh.verts[IX(i, j)].x = int2fix(i << TILESHIFT);
            g_Mesh.verts[IX(i, j)].z = int2fix(j << TILESHIFT);
            g_Mesh.verts[IX(i, j)].y = 0;
        }

    bsize = MAPW / 4;
    csize = bsize / 2;
    rnd = int2fix(MAPW);

    while (bsize > 1)
    {
        // Edges
        for (i = 0; i < MAPW; i += bsize)
            for (j = 0; j < MAPW; j += bsize)
            {
                k = g_Mesh.verts[IX(i, j)].y + g_Mesh.verts[IX(i + bsize, j)].y;
                k >>= 1;
                k += rand32balanced(rnd);

                g_Mesh.verts[IX(i + csize, j)].y = k;

                k = g_Mesh.verts[IX(i, j)].y + g_Mesh.verts[IX(i, j + bsize)].y;
                k >>= 1;
                k += rand32balanced(rnd);

                g_Mesh.verts[IX(i, j + csize)].y = k;
            }

        // Center
        for (i = 0; i < MAPW; i += bsize)
            for (j = 0; j < MAPW; j += bsize)
            {
                k = g_Mesh.verts[IX(i + csize, j)].y +
                    g_Mesh.verts[IX(i, j + csize)].y +
                    g_Mesh.verts[IX(i + bsize, j + csize)].y +
                    g_Mesh.verts[IX(i + csize, j + bsize)].y;
                k >>= 2;
                k += rand32balanced(rnd);

                g_Mesh.verts[IX(i + csize, j + csize)].y = k;
            }

        rnd >>= 1;

        bsize >>= 1;
        csize >>= 1;
    };

    for (i = 0; i < MAPW * MAPW; ++i)
    {
        if (g_Mesh.verts[i].y < 0)
            g_Mesh.verts[i].y >>= 1;
    }

    for (j = 0; j < MAPW; ++j)
        for (i = 0; i < MAPW; ++i)
        {
            tl = g_Mesh.verts[IX(i, j)].y;
            tr = g_Mesh.verts[IX(i + 1, j)].y;
            bl = g_Mesh.verts[IX(i, j + 1)].y;
            br = g_Mesh.verts[IX(i + 1, j + 1)].y;

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
                face.centerpoint.x = int2fix(i << TILESHIFT) + int2fix(4);
                face.centerpoint.y = int2fix(j << TILESHIFT) + int2fix(4);
#ifdef PAL_256
                k = clamp(12 + (((tl - tr) + (tl - bl)) >> 16), 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = clamp(6 + (((tl - tr) + (tl - bl)) >> 18), 2, 10);
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
                face.centerpoint.x = int2fix(i << TILESHIFT) + int2fix(12);
                face.centerpoint.y = int2fix(j << TILESHIFT) + int2fix(12);

#ifdef PAL_256
                k = clamp(12 + (((bl - br) + (tr - br)) >> 16), 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = clamp(6 + (((bl - br) + (tr - br)) >> 18), 2, 10);
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
                face.centerpoint.x = int2fix(i << TILESHIFT) + int2fix(12);
                face.centerpoint.y = int2fix(j << TILESHIFT) + int2fix(4);

#ifdef PAL_256
                k = clamp(12 + (((tl - tr) + (tr - br)) >> 16), 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = clamp(6 + (((tl - tr) + (tr - br)) >> 17), 2, 10);
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
                face.centerpoint.x = int2fix(i << TILESHIFT) + int2fix(4);
                face.centerpoint.y = int2fix(j << TILESHIFT) + int2fix(12);

#ifdef PAL_256
                k = clamp(12 + (((bl - br) + (tl - bl)) >> 16), 0, 15);
                if (tl > SNOWLEVEL)
                    k += 32;
                else if (tl > SANDLEVEL)
                    k += 16;
                face.flags = range[k];
#else
                k = clamp(6 + (((bl - br) + (tl - bl)) >> 17), 2, 10);
                face.flags = k;
#endif // PAL_256

                g_Mesh.faces[facecounter++] = face;
            }
        }

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
}

void DeAllocateTerrain(void)
{
    cvector_free(g_Mesh.verts);
    cvector_free(g_Mesh.faces);
    cvector_free(g_Mesh.verts_transformed);
}

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
            B = fixmultINTL(B, localX);
            C = fixmultINTL(C, localZ);
        }
        else
        {
            // Bottom Right Triangle
            // Flip the local coords
            localX = 65536 - localX;
            localZ = 65536 - localZ;
            A = g_Mesh.verts[IX(mapX + 1, mapZ + 1)].y;
            B = fixmultINTL(B, localZ);
            C = fixmultINTL(C, localX);
        }

        A = fixmultINTL(A, (65536 - localX - localZ));
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
            B = fixmultINTL(B, localX);
            C = fixmultINTL(C, localZ);
        }
        else
        {
            // Bottom Left Triangle
            // Flip the local coords
            localZ = 65536 - localZ;
            A = g_Mesh.verts[IX(mapX, mapZ + 1)].y;
            B = fixmultINTL(B, localZ);
            C = fixmultINTL(C, localX);
        }

        A = fixmultINTL(A, (65536 - localX - localZ));
    }

    return A + B + C + 262144;
}
