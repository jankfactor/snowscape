#ifndef MESH_H
#define MESH_H

#include "math3d.h"
#include "terrain.h"

#define MAPW 128
#define TILESHIFT 4
#define CELL_SIZE_FIX (1 << 20)
#define CELL_QUARTER_FIX (CELL_SIZE_FIX / 4)
#define IX(x, z) (((x) & (MAPW - 1)) + (((z) & (MAPW - 1)) * MAPW))

typedef struct Mesh
{
    V3D verts[MAPW * MAPW];
    TRI faces[MAPW * MAPW * 2];
    V3D verts_transformed[MAPW * MAPW];
} Mesh;

extern Mesh g_Mesh;

/** Subdivide, shade, and triangulate the selected Midwinter terrain patch.
 * @param source Loaded pristine terrain seed.
 * @param path Selected map zoom path; centered levels are appended as needed.
 * @return Zero on success; non-zero if terrain preparation fails.
 */
int GenerateTerrain(const TerrainSource *source, const TerrainZoomPath *path);
/** Place the camera over the map cursor selected for entry into 3D.
 * @param path Completed zoom path and final-grid player coordinate.
 * @param eyePos Destination camera position, including terrain clearance.
 */
void SetTerrainPlayerStart(const TerrainZoomPath *path, V3D *eyePos);
/** Interpolate the terrain surface beneath a world-space position.
 * @param eyePos Position whose X/Z coordinates select the terrain triangle.
 * @return Surface height plus camera clearance in 16.16 fixed point.
 */
fix GetHeight(V3D *eyePos);

#endif // MESH_H
