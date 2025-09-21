#ifndef MESH_H
#define MESH_H

#include "math3d.h"
#include "cvector.h"

#define MAPW 128
#define _MAPW 127
#define MAPSHIFT 7

#define TILESHIFT 4 // 16x16 tiles
#define IX(x, z) (((x) & _MAPW) + (((z) & _MAPW) << (MAPSHIFT)))

typedef struct Mesh
{
    cvector_vector_type(V3D) verts;
    cvector_vector_type(TRI) faces;
    cvector_vector_type(V3D) verts_transformed;
} Mesh;

extern Mesh g_Mesh;

void GenerateTerrain(void);
void DeAllocateTerrain(void);
fix GetHeight(V3D *eyePos);

#endif // MESH_H

