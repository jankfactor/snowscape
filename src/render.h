#ifndef RENDER_H
#define RENDER_H

#define SCREEN_W 320
#define SCREEN_H 256
#define MAXDEPTH 256

#include "math3d.h"

/** Clear every depth bucket before rendering begins. */
void SetupRender(void);
/** Transform, clip, sort, project, and rasterize the visible terrain.
 * @param mv World-to-view transform.
 * @param eyePos Camera position, which may be recentered at a map boundary.
 * @param yaw Camera heading in sine-table units.
 */
void RenderModel(MAT43 *mv, V3D *eyePos, int yaw);
/** Project a transformed vertex and report clipping state.
 * @param v Vertex to project.
 * @param clipflags Receives implementation-defined clipping flags.
 */
void MultV3DProj(V3D *v, int *clipflags);

// How many tiles around the look center. i.e., double this for the max tiles ahead.
// The bigger the number, the more tiles will be rendered.

#ifdef A5000
    #define SCANRANGE 10
#else // A3000/A30X0
    #define SCANRANGE 8
#endif

#define SUBRANGE ((SCANRANGE - 1) << 8)

// Uncomment the following to enable the timing log.
// #define TIMING_LOG 1

#ifdef TIMING_LOG
typedef struct TimerLog
{
    int transformTiles;
    int submitRenderTriangles;
    int clippingQueue;
    int project3D;
    int sceneRender;
    int biggestVertex;
    int clippedCount;
} TimerLog;

extern TimerLog gTimerLog;

// The following SWIs are for David Ruck's TimerMod.
// Which can be found at https://armclub.org.uk/free/
#define SWI_Timer_Start 0x000490C0
#define SWI_Timer_Stop 0x000490C1
#define SWI_Timer_Value 0x000490C2

/** Stop and restart TimerMod around a render stage.
 * @return Elapsed TimerMod ticks.
 */
int GetRenderDelta(void);

#endif // TIMING_LOG

#endif // RENDER_H
