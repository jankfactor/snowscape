#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <kernel.h>
#include <swis.h>

#include "mesh.h"
#include "map.h"
#include "palette.h"
#include "render.h"

// ASM Routines
/** Select the configured graphics mode and disable the text cursor. */
extern void VDUSetup(void);
/** Publish the current draw-buffer address to the assembly rasterizer.
 * @param screenStart Start address returned by OS_ReadVduVariables.
 * @param screenMax End/size value retained for interface compatibility.
 */
extern void UpdateMemAddress(int screenStart, int screenMax);
/** Reserve enough RISC OS screen memory for double buffering. */
extern void ReserveScreenBanks(void);
/** Make the draw bank visible and select the other bank for drawing. */
extern void SwitchScreenBank(void);
/** Fill all or part of the current draw bank.
 * @param color Packed pixel pattern written to the screen.
 * @param fullclear Non-zero to clear the full screen; zero for the partial area.
 */
extern void ClearScreen(int color, int fullclear);
/** Poll one RISC OS internal key code.
 * @param keyCode Internal key number.
 * @return Non-zero while the key is pressed.
 */
extern int KeyPress(int keyCode);
/** Rasterize one projected triangle using the fog/colour lookup.
 * @param triList Address of three consecutive projected vertices.
 * @param color Packed base-colour and fog-table offset.
 */
extern void FillEdgeLists(int triList, int color);

// SWI access
_kernel_oserror *err;
_kernel_swi_regs rin, rout;

char *gBaseDirectoryPath = NULL;
int *gEdgeList = NULL;
extern unsigned int EdgeList;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define KEY_F 67
#define KEY_F1 113

static const char *CompassDirection(int heading)
{
    /* Heading zero faces mesh +X (map east); map north is mesh -Z. */
    static const char *const directions[] = {
        "E", "NE", "N", "NW", "W", "SW", "S", "SE"
    };
    int normalizedHeading = heading & _SINETABLE_SIZE;
    int sector = ((normalizedHeading + SINETABLE_SIZE / 16) &
                  _SINETABLE_SIZE) / (SINETABLE_SIZE / 8);

    return directions[sector];
}

/** Initialize Snowscape, run the interactive terrain view, and release resources.
 * @param argc Process argument count; currently unused.
 * @param argv Process argument vector; currently unused.
 * @return Zero after a normal exit; non-zero when required assets cannot load.
 */
int main(int argc, char *argv[])
{
    int i, swi_data[10], isRunning = 1, exitCode = 0;
    int terrainGenerated = 0;
#ifndef PAL_256
    int fogKeyWasPressed = 0;
#endif
    int heading = 498, pitch = -53, angle = 0;
    V3D eyePos, direction;
    V3D verts[3];
    MAT43 mat;
    int mouseX, mouseY;
    unsigned char block[9];
    TerrainSource terrainSource;
    TerrainZoomPath mapSelection = {0};
    TerrainZoomPath terrainSelection = {0};

    gBaseDirectoryPath = getenv("Game$Dir");

    SetupMathsGlobals(1);
    for (i = 0; i < 1024; ++i)
    {
        g_SineTable[i] = float2fix(sinf((i * M_PI * 2.f) / 1024.f));
        g_oneOver[i] = (i == 0) ? float2fix(1.f) : float2fix(1.f / i);
    }

    cvector_reserve(gEdgeList, 256);
    EdgeList = (unsigned int)(gEdgeList); // For ASM access

    SetupPaletteLookup(1);
    SetupRender();
    if (TerrainLoad(&terrainSource, gBaseDirectoryPath) != 0)
    {
        printf("ERROR: Failed to load terrain.\n");
        return 1;
    }

    if (LoadFogLookup() != 0)
    {
        printf("ERROR: Failed to load fog lookup table.\n");
        return 1;
    }

    // (void)getchar(); // Uncomment to pause here and read data output

    // Disable the default escape handler
    rin.r[0] = 229;
    rin.r[1] = 0xFFFFFFFF;
    err = _kernel_swi(OS_Byte, &rin, &rout);

    VDUSetup();
    ReserveScreenBanks();
    SwitchScreenBank();
#ifndef PAL_256
    SetPalette(); // Set the sixteen configurable logical colours used by the renderer
#endif // PAL_256
    Save256(); // Uncomment to save the VIDC generated palette to a file

    // Obtain details about the current screen mode
    swi_data[0] = (int)148;         // screen base address
    swi_data[1] = (int)-1;          // terminate query
    rin.r[0] = (int)(&swi_data[0]); // Start of query
    rin.r[1] = (int)(&swi_data[3]); // Results
    err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
    UpdateMemAddress(swi_data[3], swi_data[4]);

    for (i = 0; i < 2; ++i)
    {
        SwitchScreenBank();             // Swap draw buffer with display buffer
        rin.r[0] = (int)(&swi_data[0]); // Get the new screen start address
        rin.r[1] = (int)(&swi_data[3]); // Results
        err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
        UpdateMemAddress(swi_data[3], swi_data[4]); // Pass these to the ASM side
        ClearScreen(0, 1);                          // Clear the new draw buffer
    }

    TerrainSelectPlayer(&terrainSelection,
                        TERRAIN_DISPLAY_SIZE / 2,
                        TERRAIN_DISPLAY_SIZE / 2);

map_screen:
    i = RunMapScreen(&terrainSource, &mapSelection, &terrainSelection);
    if (i != 0)
    {
        if (i < 0)
        {
            printf("ERROR: Failed to prepare terrain map.\n");
            exitCode = 1;
        }
        goto exit_graphics;
    }

    if (GenerateTerrain(&terrainSource, &terrainSelection) != 0)
    {
        printf("ERROR: Failed to generate selected terrain.\n");
        exitCode = 1;
        goto exit_graphics;
    }
    terrainGenerated = 1;
    {
        int worldX, worldY;
        TerrainPlayerWorldPosition(&terrainSelection, &worldX, &worldY);
        printf("Loading player at world position %d, %d\n",
               worldX, worldY);
    }

#ifdef PAL_256
    /* Keep Mode 13's default palette for the map, then install the renderer's
       configurable colours only when entering the 3D view. */
    SetPalette();
#endif // PAL_256

    /* Remove the map from both banks before entering the 3D view. */
    for (i = 0; i < 2; ++i)
    {
        SwitchScreenBank();
        rin.r[0] = (int)(&swi_data[0]);
        rin.r[1] = (int)(&swi_data[3]);
        err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
        UpdateMemAddress(swi_data[3], swi_data[4]);
        ClearScreen(0, 1);
    }

    SetTerrainPlayerStart(&terrainSelection, &eyePos);

#ifdef TIMING_LOG
    gTimerLog.biggestVertex = 0;
#endif // TIMING_LOG

    // Set an infinite screen box so the mouse doesn't stop at the screen edge.
    block[0] = 0x01; // Reason code
    block[1] = 0x00; // Left LSB
    block[2] = 0x80; // Left MSB
    block[3] = 0x00; // Bottom LSB
    block[4] = 0x80; // Bottom MSB
    block[5] = 0xFF; // Right LSB
    block[6] = 0x7F; // Right MSB
    block[7] = 0xFF; // Top LSB
    block[8] = 0x7F; // Top MSB
    rin.r[0] = 21;
    rin.r[1] = (int)(&block[0]);
    err = _kernel_swi(OS_Word, &rin, &rout);

    // Get initial mouse position
    err = _kernel_swi(OS_Mouse, &rin, &rout);
    mouseX = rout.r[0];
    mouseY = rout.r[1];
    eyePos.y = GetHeight(&eyePos); // Start at the correct height

    // Triangle mainly in the center of the 320x256 screen
    verts[0].x = (164);
    verts[0].y = (10);
    verts[0].z = (0);
    verts[1].x = (150);
    verts[1].y = (200);
    verts[1].z = (0);
    verts[2].x = (170);
    verts[2].y = (200);
    verts[2].z = (0);

    if (err == NULL)
    {
        while (isRunning)
        {
            err = _kernel_swi(OS_Mouse, &rin, &rout); // Get the mouse position
            heading -= clamp((rout.r[0] - mouseX) >> 7, -32, 32);
            pitch += clamp((rout.r[1] - mouseY) >> 7, -32, 32);
            pitch = clamp(pitch, -100, 100);

            if (KeyPress(112)) // Escape
                isRunning = 0;
            if (isRunning && KeyPress(KEY_F1))
                goto map_screen;

#ifndef PAL_256
            {
                int fogKeyPressed = KeyPress(KEY_F);

                if (fogKeyPressed && !fogKeyWasPressed)
                    ToggleFogLookupBlend();
                fogKeyWasPressed = fogKeyPressed;
            }
#endif // PAL_256

            if (rout.r[2] & 4) // Left mouse button - Walk forward
            {
                // --angle;
                /* One sixteenth of a terrain cell per frame. */
                eyePos.x += fixcos(heading);
                eyePos.z -= fixsin(heading);
                eyePos.y = GetHeight(&eyePos);
            }
            if (rout.r[2] & 1) // Right mouse button - Walk backward
            {
                // ++angle;
                /* One thirty-second of a terrain cell per frame. */
                eyePos.x -= fixcos(heading) >> 1;
                eyePos.z += fixsin(heading) >> 1;
                eyePos.y = GetHeight(&eyePos);
            }

            SwitchScreenBank();             // Swap draw buffer with display buffer
            rin.r[0] = (int)(&swi_data[0]); // Get the new screen start address
            rin.r[1] = (int)(&swi_data[3]); // Results
            err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
            UpdateMemAddress(swi_data[3], swi_data[4]); // Pass these to the ASM side

            direction.x = fixcos(heading);
            direction.y = fixsin(pitch);
            direction.z = -fixsin(heading);

            LookAt(&eyePos, &direction, &mat); // TODO - SLOWWWWWW - 2 cross products in here

#ifdef PAL_256
            ClearScreen(0xC6C6C6C6, 1); // Clear the new draw buffer
#else
            ClearScreen(0x22222222, 1); // Clear the new draw buffer
#endif // PAL_256

            RenderModel(&mat, &eyePos, heading); // Main render

            // rin.r[0] = 30;
            // err = _kernel_swi(OS_WriteC, &rin, &rout);

            // printf("HEADING: %s", CompassDirection(heading));

#ifdef TIMING_LOG
            {
                rin.r[0] = 30;
                err = _kernel_swi(OS_WriteC, &rin, &rout);

                // printf("DISt     :  %d", dist);
                printf("\nTRANSFORM TILES : %d", gTimerLog.transformTiles);
                printf("\nSUBMIT TRIANGLES: %d", gTimerLog.submitRenderTriangles);
                printf("\nCLIPPING QUEUE  : %d", gTimerLog.clippingQueue);
                printf("\n3D PROJECTION   : %d", gTimerLog.project3D);
                printf("\nSCENE RENDER    : %d", gTimerLog.sceneRender);
                printf("\nBIGGEST VERTEX  : %d", gTimerLog.biggestVertex);
                printf("\nCLIPPED COUNT   : %d", gTimerLog.clippedCount);
            }
#endif // TIMING_LOG
        }
    }
    else
    {
        printf("ERROR: %s", err->errmess);
    }

exit_graphics:
    // Return to text mode
    rin.r[0] = 22;
    err = _kernel_swi(OS_WriteC, &rin, &rout);
    rin.r[0] = 0;
    err = _kernel_swi(OS_WriteC, &rin, &rout);

    // Re-enable the default escape handler
    rin.r[0] = 229;
    rin.r[1] = 0xFFFFFFFF;
    err = _kernel_swi(OS_Byte, &rin, &rout);

    // Free up memory that was allocated
    cvector_free(gEdgeList);
    if (terrainGenerated)
        DeAllocateTerrain();
    SetupMathsGlobals(0);
    SetupPaletteLookup(0);
    if (terrainGenerated)
    {
        printf("Heading: %d, Pitch: %d\n", heading, pitch);
        printf("Eyepos: %d, %d, %d\n", eyePos.x, eyePos.y, eyePos.z);
    }

    // (void)getchar(); // Uncomment to pause here and read data output

    return exitCode;
}
