#ifndef MESH_H
#define MESH_H

#include "math3d.h"
#include "cvector.h"
#include "terrain.h"

#define MAPW 128
#define _MAPW 127
#define MAPSHIFT 7

/* Midwinter cells are logically 2x2 units. Scale the complete world by eight
   internally so the 16.16 transform and projection paths retain more precision. */
#define MIDWINTER_TILESHIFT 1
#define WORLD_SCALE_SHIFT 3
#define TILESHIFT (MIDWINTER_TILESHIFT + WORLD_SCALE_SHIFT)
#define CELL_TO_WORLD_SHIFT (16 + TILESHIFT)
#define CELL_SIZE_FIX (1 << CELL_TO_WORLD_SHIFT)
#define CELL_QUARTER_FIX (CELL_SIZE_FIX >> 2)
#define CELL_THREE_QUARTER_FIX (CELL_QUARTER_FIX * 3)

/* Renderer distance constants were originally tuned for 16-unit cells. */
#define LEGACY_TILESHIFT 4
#define WORLD_SCALE_REDUCTION (LEGACY_TILESHIFT - TILESHIFT)
#define IX(x, z) (((x) & _MAPW) + (((z) & _MAPW) << (MAPSHIFT)))

typedef struct Mesh
{
    cvector_vector_type(V3D) verts;
    cvector_vector_type(TRI) faces;
    cvector_vector_type(V3D) verts_transformed;
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
/** Release all dynamically allocated terrain mesh arrays. */
void DeAllocateTerrain(void);
/** Interpolate the terrain surface beneath a world-space position.
 * @param eyePos Position whose X/Z coordinates select the terrain triangle.
 * @return Surface height plus camera clearance in 16.16 fixed point.
 */
fix GetHeight(V3D *eyePos);

#endif // MESH_H
