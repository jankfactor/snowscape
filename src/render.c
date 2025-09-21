#include <kernel.h>

#include "render.h"
#include "math3d.h"
#include "mesh.h"
#include "palette.h"

extern void SortVertices(int triList);
extern void DrawTriangle(int triList, int color);
extern void FillEdgeLists(int triList, int color);
extern void ProjectVertex(int vertexPtr);

TRI *g_RenderQueue[MAXDEPTH];

int gDebug = 0;

#define MAX_EXTRA_VERTS 192
#define MAX_EXTRA_TRIS 64

TRI *clippedQueue1 = 0;
V3D clippedNearVerts[MAX_EXTRA_VERTS];
int clippedNearVertIndex = 0;
TRI clippedNearTris[MAX_EXTRA_TRIS];
int clippedNearTrisIndex = 0;

#ifdef TIMING_LOG
TimerLog gTimerLog;

int GetRenderDelta(void)
{
    _kernel_oserror *err;
    _kernel_swi_regs rin, rout;
    int deltaTime = 0;

    err = _kernel_swi(SWI_Timer_Stop, &rin, &rout);
    deltaTime = rout.r[1];
    err = _kernel_swi(SWI_Timer_Start, &rin, &rout);

    return deltaTime;
}
#endif // TIMING_LOG

void SetupRender(void)
{
    memset(g_RenderQueue, 0, MAXDEPTH * sizeof(TRI *));
}

#define interpolate(a, b)                                         \
    do                                                            \
    {                                                             \
        t = ((a)->z - (b)->z) >> 8;                               \
        if (t != 0)                                               \
        {                                                         \
            t = ((a)->z << 8) / t;                                \
            tmp.x = (((b)->x - (a)->x) >> 8) * (t >> 8) + (a)->x; \
            tmp.y = (((b)->y - (a)->y) >> 8) * (t >> 8) + (a)->y; \
            tmp.z = 0;                                            \
            inside.verts[inside.numVerts++] = tmp;                \
        }                                                         \
        else                                                      \
        {                                                         \
            (a)->z = 0;                                           \
        }                                                         \
    } while (0);

void ClipPolyonListToNearPlane(POLYGON *p)
{
    static POLYGON inside;
    static V3D *previous, *current;
    static V3D tmp;
    register int i, t;

    inside.numVerts = 0;
    previous = &p->verts[p->numVerts - 1];

    for (i = 0; i < p->numVerts; ++i)
    {
        current = &p->verts[i];

        // Have we crossed into near plane?
        if ((previous->z ^ current->z) & 0x80000000)
        {
            if (previous->z < 0)
            {
                interpolate(current, previous);
            }
            else
            {
                interpolate(previous, current);
            }
        }

        if (current->z >= 0)
            inside.verts[inside.numVerts++] = *current;

        previous = current;
    }

    *p = inside;
}

void RenderTriangle(TRI *tri)
{
    int i, j, k;
    V3D vscreen[3];
    static int clipflags;

    vscreen[0] = g_Mesh.verts_transformed[tri->a];
    vscreen[1] = g_Mesh.verts_transformed[tri->b];
    vscreen[2] = g_Mesh.verts_transformed[tri->c];

    i = vscreen[0].z;
    j = vscreen[1].z;
    k = vscreen[2].z;

    // Clipping of triangles outside the view frustum

    if ((vscreen[0].x < -i) &&
        (vscreen[1].x < -j) &&
        (vscreen[2].x < -k))
        return;

    if ((vscreen[0].x > i) &&
        (vscreen[1].x > j) &&
        (vscreen[2].x > k))
        return;

    // Clipping of triangles based on Z depth
    clipflags = (i < 0) + (j < 0) + (k < 0);

    // All verts are behind the camera, skip
    if (clipflags == 3)
        return;

    // Turn the average depth into an index that fits into our render queue range
    k += (j + k);
    k >>= 18;
    k += 4;

    // Triangles that cross the near plane need to be clipped
    if (clipflags && (clippedNearTrisIndex < MAX_EXTRA_TRIS))
    {
        tri->depth = max(k, 0);
        tri->next = clippedQueue1;
        clippedQueue1 = tri;
    }
    else if (k < MAXDEPTH)
    {
        // All verts are in front of the camera, no need to clip
        tri->depth = k;
        tri->next = g_RenderQueue[k];
        g_RenderQueue[k] = tri;
    }
}

void RenderModel(MAT43 *mv, V3D *eyePos, int yaw)
{
    int i, j, k, x, z, mx, mz;

#ifdef TIMING_LOG
    int largest;
#endif // TIMING_LOG

    TRI **tri;
    V3D _verts[3];
    static POLYGON poly;

    clippedNearTrisIndex = clippedNearVertIndex = 0;

    // Get the fixed normalized location
    mx = eyePos->x >> 20;
    mz = eyePos->z >> 20;

    // mx += (fixcos(yaw) >> 13);
    // mz -= (fixsin(yaw) >> 13);

    mx += ((fixcos(yaw) >> 8) * (SUBRANGE)) >> 16;
    mz -= ((fixsin(yaw) >> 8) * (SUBRANGE)) >> 16;

    // We have a limit of SCANRANGE tiles to render, so if mx and mz get
    // too lose to the limit of the map (MAPW) then move the player back
    // to the center of the map.
    if (mx <= SCANRANGE || mx >= MAPW - SCANRANGE || mz <= SCANRANGE || mz >= MAPW - SCANRANGE)
    {
        eyePos->x = int2fix(MAPW << TILESHIFT) / 2;
        eyePos->z = int2fix(MAPW << TILESHIFT) / 2;
    }

#ifdef TIMING_LOG
    (void)GetRenderDelta();
#endif // TIMING_LOG

    for (x = mx - SCANRANGE; x <= mx + SCANRANGE; ++x)
    {
        for (z = mz - SCANRANGE; z <= mz + SCANRANGE; ++z)
        {
            k = x + (z << MAPSHIFT);
            MultV3DMat(&g_Mesh.verts[k], &g_Mesh.verts_transformed[k], mv);
        }
    }

#ifdef TIMING_LOG
    gTimerLog.transformTiles = GetRenderDelta();
#endif // TIMING_LOG

    for (x = mx - SCANRANGE; x < mx + SCANRANGE; ++x)
    {
        for (z = mz - SCANRANGE; z < mz + SCANRANGE; ++z)
        {
            k = x + (z << MAPSHIFT);
            k <<= 1;
            RenderTriangle(&g_Mesh.faces[k]);
            ++k;
            RenderTriangle(&g_Mesh.faces[k]);
        }
    }

#ifdef TIMING_LOG
    gTimerLog.submitRenderTriangles = GetRenderDelta();
#endif // TIMING_LOG

    // Clip triangles against near plane
    while (clippedQueue1)
    {
        k = clippedQueue1->depth;

        poly.numVerts = 3;
        poly.verts[0] = g_Mesh.verts_transformed[clippedQueue1->a];
        poly.verts[1] = g_Mesh.verts_transformed[clippedQueue1->b];
        poly.verts[2] = g_Mesh.verts_transformed[clippedQueue1->c];

        // Clip against near plane
        ClipPolyonListToNearPlane(&poly);

        for (i = 0; i < poly.numVerts; ++i)
        {
            ProjectVertex((int)(&poly.verts[i]));
        }

        // Render clipped triangles
        for (i = 0; i < poly.numVerts - 2; ++i)
        {
            if (clippedNearTrisIndex >= MAX_EXTRA_TRIS)
                break;

            clippedNearVerts[clippedNearVertIndex] = poly.verts[0];
            clippedNearTris[clippedNearTrisIndex].a = clippedNearVertIndex++;
            clippedNearVerts[clippedNearVertIndex] = poly.verts[i + 1];
            clippedNearTris[clippedNearTrisIndex].b = clippedNearVertIndex++;
            clippedNearVerts[clippedNearVertIndex] = poly.verts[i + 2];
            clippedNearTris[clippedNearTrisIndex].c = clippedNearVertIndex++;

            clippedNearTris[clippedNearTrisIndex].flags = clippedQueue1->flags | TRI_CLIPPED_BIT;

            // Stick these at the front of the render queue
            clippedNearTris[clippedNearTrisIndex].depth = clippedQueue1->depth;
            clippedNearTris[clippedNearTrisIndex].next = g_RenderQueue[k];
            g_RenderQueue[k] = &clippedNearTris[clippedNearTrisIndex];

            ++clippedNearTrisIndex;
        }

        clippedQueue1 = clippedQueue1->next;
    }

#ifdef TIMING_LOG
    gTimerLog.clippingQueue = GetRenderDelta();
#endif // TIMING_LOG

    for (x = mx - SCANRANGE; x <= mx + SCANRANGE; ++x)
    {
        for (z = mz - SCANRANGE; z <= mz + SCANRANGE; ++z)
        {
            i = x + (z << MAPSHIFT);
            // MultV3DProj(&g_Mesh.verts_transformed[i], &k);
            ProjectVertex((int)&g_Mesh.verts_transformed[i]);
        }
    }

#ifdef TIMING_LOG
    gTimerLog.project3D = GetRenderDelta();
    largest = 0;
#endif // TIMING_LOG

    // Painter's algorithm. Proceed from furthest to nearest.
    for (i = MAXDEPTH - 1; i >= 0; i--)
    {
        tri = &g_RenderQueue[i];
        while (*tri)
        { // Render faces with current depth

            if ((*tri)->flags & TRI_CLIPPED_BIT)
            {
                _verts[0] = clippedNearVerts[(*tri)->a];
                _verts[1] = clippedNearVerts[(*tri)->b];
                _verts[2] = clippedNearVerts[(*tri)->c];
                j = 0;
            }
            else
            {
                _verts[0] = g_Mesh.verts_transformed[(*tri)->a];
                _verts[1] = g_Mesh.verts_transformed[(*tri)->b];
                _verts[2] = g_Mesh.verts_transformed[(*tri)->c];

                // The following is a cheap approximation to a hypotenuse
                // If we use the projected depth value then it causes the dithering
                // to change as you rotate the camera.
                j = abs((*tri)->centerpoint.x - eyePos->x);
                k = abs((*tri)->centerpoint.y - eyePos->z);
                j = (j > k) ? (j + (k >> 1)) : (k + (j >> 1));

#ifdef PAL_256
#ifdef A5000
                j >>= 18;
                j = max(0, min(j - 16, 63));
#else
                j >>= 17;
                j = max(0, min(j - 16, 63));
#endif // A5000
#else  // 16 COLOR
#ifdef A5000
                j >>= 20;
                j = max(0, j - 4);
#else
                j >>= 19;
                j = max(0, j - 6);
#endif // A5000
#endif // PAL_256
            }

#ifdef PAL_256
            k = (((*tri)->flags & 255) << 7) + j;
            if (orient2dint(_verts[0], _verts[1], _verts[2]) < 0)
                FillEdgeLists((int)&_verts[0], k);
#else
            k = (((*tri)->flags & 31) << 5) + j;
            if (j < 16 && orient2dint(_verts[0], _verts[1], _verts[2]) < 0)
                FillEdgeLists((int)&_verts[0], k);
#endif // PAL_256

#ifdef TIMING_LOG
            largest = max(largest, abs(_verts[0].x));
            largest = max(largest, abs(_verts[1].x));
            largest = max(largest, abs(_verts[2].x));
            largest = max(largest, abs(_verts[0].y));
            largest = max(largest, abs(_verts[1].y));
            largest = max(largest, abs(_verts[2].y));
#endif // TIMING_LOG

            *tri = (*tri)->next; // Next face
        }
    }

#ifdef TIMING_LOG
    gTimerLog.sceneRender = GetRenderDelta();
    gTimerLog.biggestVertex = max(largest, gTimerLog.biggestVertex);
    gTimerLog.clippedCount = clippedNearTrisIndex;
#endif // TIMING_LOG
}
