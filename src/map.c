#include <stdlib.h>
#include <string.h>

#include <kernel.h>
#include <swis.h>

#include "map.h"
#include "render.h"

#define MAP_LEFT 0
#define MAP_TOP 0
#define MAP_SEA_COLOR 203
#define MAP_FRAME_WIDTH 3

extern void UpdateMemAddress(int screenStart, int screenMax);
extern int KeyPress(int keyCode);

/* Mode 13 indices for its sixteen exact grey ramp entries. */
static const unsigned char reliefColors[16] = {
    0, 1, 2, 3, 44, 45, 46, 47,
    208, 209, 210, 211, 252, 253, 254, 255
};

/* Three exact Mode 13 greys forming the frame's outer-to-inner rings. */
static const unsigned char mapFrameColors[MAP_FRAME_WIDTH] = {
    0, 208, 255
};

/** Clamp an integer to an inclusive range.
 * @param value Value to constrain.
 * @param minimum Lowest permitted result.
 * @param maximum Highest permitted result.
 * @return value constrained to minimum through maximum.
 */
static int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

/** Test whether a screen position lies anywhere in the 200x200 map pane. */
static int IsMapPixel(int x, int y)
{
    return x >= MAP_LEFT && x < MAP_LEFT + TERRAIN_DISPLAY_SIZE &&
           y >= MAP_TOP && y < MAP_TOP + TERRAIN_DISPLAY_SIZE;
}

/** Test whether a screen position lies inside the frame's usable map area. */
static int IsMapInterior(int x, int y)
{
    return x >= MAP_LEFT + MAP_FRAME_WIDTH &&
           x < MAP_LEFT + TERRAIN_DISPLAY_SIZE - MAP_FRAME_WIDTH &&
           y >= MAP_TOP + MAP_FRAME_WIDTH &&
           y < MAP_TOP + TERRAIN_DISPLAY_SIZE - MAP_FRAME_WIDTH;
}

/** Test whether a screen position belongs to one of the map-frame rings. */
static int IsMapFramePixel(int x, int y)
{
    return IsMapPixel(x, y) && !IsMapInterior(x, y);
}

/** Select the bevel colour for a map-frame pixel. */
static unsigned char MapFrameColor(int x, int y)
{
    int localX = x - MAP_LEFT;
    int localY = y - MAP_TOP;
    int depth = localX;

    if (localY < depth) depth = localY;
    if (TERRAIN_DISPLAY_SIZE - 1 - localX < depth)
        depth = TERRAIN_DISPLAY_SIZE - 1 - localX;
    if (TERRAIN_DISPLAY_SIZE - 1 - localY < depth)
        depth = TERRAIN_DISPLAY_SIZE - 1 - localY;
    return mapFrameColors[ClampInt(depth, 0, MAP_FRAME_WIDTH - 1)];
}

/** Select the relief-map colour for one land sample and its eastern neighbor.
 * This follows Midwinter's VGA relief viewer: east-facing height change is
 * centered on a 64-shade ramp and becomes more sensitive at each zoom level.
 * Mode 13 provides sixteen exact greys, so the VGA ramp is reduced to those.
 * @param rawHeight Midwinter signed height stored in an unsigned word.
 * @param rawEastHeight Height of the sample immediately to the east.
 * @param zoomLevel Current map zoom level from zero through four.
 * @return Sea index 203 or one of the sixteen ordered grey indices.
 */
static unsigned char ColorForRelief(unsigned short rawHeight,
                                    unsigned short rawEastHeight,
                                    int zoomLevel)
{
    int height = TerrainSignedHeight(rawHeight);
    int eastHeight;
    int shade;

    if (height < 0)
        return MAP_SEA_COLOR;
    eastHeight = TerrainSignedHeight(rawEastHeight);
    shade = ((eastHeight - height) >> (6 - zoomLevel)) + 32;
    shade = ClampInt(shade, 0, 63);
    return reliefColors[shade >> 2];
}

/** Convert interpolated terrain cells into a cached 200x200 indexed bitmap.
 * @param bitmap Destination byte-per-pixel map buffer.
 * @param display Source 200x200 terrain display cells.
 * @param zoomLevel Current map zoom level from zero through four.
 */
static void RenderMapBitmap(unsigned char *bitmap, const HeightCell *display,
                            int zoomLevel)
{
    int x, y;

    for (y = 0; y < TERRAIN_DISPLAY_SIZE; ++y)
    {
        unsigned char *row = bitmap + y * TERRAIN_DISPLAY_SIZE;
        for (x = 0; x < TERRAIN_DISPLAY_SIZE; ++x)
        {
            int index = y * TERRAIN_DISPLAY_SIZE + x;
            unsigned short eastHeight = (x + 1 < TERRAIN_DISPLAY_SIZE)
                ? display[index + 1].height : 0xffff;
            row[x] = ColorForRelief(
                display[index].height, eastHeight, zoomLevel);
        }
    }
}

/** Draw the three-pixel bevel over the undefined terrain fringe. */
static void DrawMapFrame(unsigned char *screen)
{
    int inset, offset;

    for (inset = 0; inset < MAP_FRAME_WIDTH; ++inset)
    {
        int left = MAP_LEFT + inset;
        int right = MAP_LEFT + TERRAIN_DISPLAY_SIZE - 1 - inset;
        int top = MAP_TOP + inset;
        int bottom = MAP_TOP + TERRAIN_DISPLAY_SIZE - 1 - inset;
        unsigned char color = mapFrameColors[inset];

        for (offset = inset;
             offset < TERRAIN_DISPLAY_SIZE - inset; ++offset)
        {
            int x = MAP_LEFT + offset;
            int y = MAP_TOP + offset;
            screen[top * SCREEN_W + x] = color;
            screen[bottom * SCREEN_W + x] = color;
            screen[y * SCREEN_W + left] = color;
            screen[y * SCREEN_W + right] = color;
        }
    }
}

/** Copy the cached map bitmap into the map area of the hardware screen.
 * @param screen Start of the 320x256 8-bit screen bank.
 * @param bitmap Source 200x200 byte-per-pixel map buffer.
 */
static void BlitMap(unsigned char *screen, const unsigned char *bitmap)
{
    int y;

    for (y = 0; y < TERRAIN_DISPLAY_SIZE; ++y)
        memcpy(screen + (MAP_TOP + y) * SCREEN_W + MAP_LEFT,
               bitmap + y * TERRAIN_DISPLAY_SIZE,
               TERRAIN_DISPLAY_SIZE);
    DrawMapFrame(screen);
}

/** Clear the part of the hardware screen not occupied by the map.
 * @param screen Start of the 320x256 8-bit screen bank.
 */
static void ClearMapSurroundings(unsigned char *screen)
{
    int y;

    for (y = 0; y < TERRAIN_DISPLAY_SIZE; ++y)
        memset(screen + y * SCREEN_W + TERRAIN_DISPLAY_SIZE, 0,
               SCREEN_W - TERRAIN_DISPLAY_SIZE);
    memset(screen + TERRAIN_DISPLAY_SIZE * SCREEN_W, 0,
           (SCREEN_H - TERRAIN_DISPLAY_SIZE) * SCREEN_W);
}

/** Restore a damaged overlay rectangle from the map bitmap or background.
 * Interior pixels come from bitmap, frame pixels are recomposed, and pixels
 * outside the map become black.
 * @param screen Start of the 320x256 8-bit screen bank.
 * @param bitmap Cached 200x200 map pixels.
 * @param left Left screen coordinate of the damaged rectangle.
 * @param top Top screen coordinate of the damaged rectangle.
 * @param width Rectangle width in pixels.
 * @param height Rectangle height in pixels.
 */
static void RestoreRect(unsigned char *screen, const unsigned char *bitmap,
                        int left, int top, int width, int height)
{
    int x, y;
    int right = ClampInt(left + width, 0, SCREEN_W);
    int bottom = ClampInt(top + height, 0, SCREEN_H);

    left = ClampInt(left, 0, SCREEN_W);
    top = ClampInt(top, 0, SCREEN_H);
    for (y = top; y < bottom; ++y)
        for (x = left; x < right; ++x)
        {
            if (IsMapFramePixel(x, y))
                screen[y * SCREEN_W + x] = MapFrameColor(x, y);
            else if (IsMapPixel(x, y))
                screen[y * SCREEN_W + x] = bitmap[
                    (y - MAP_TOP) * TERRAIN_DISPLAY_SIZE + x - MAP_LEFT];
            else
                screen[y * SCREEN_W + x] = 0;
        }
}

/** Write one clipped overlay pixel without allowing it to damage the frame.
 * @param screen Start of the 320x256 8-bit screen bank.
 * @param x Horizontal screen coordinate.
 * @param y Vertical screen coordinate.
 * @param color Palette index to write.
 */
static void PutPixel(unsigned char *screen, int x, int y,
                     unsigned char color)
{
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H &&
        !IsMapFramePixel(x, y))
        screen[y * SCREEN_W + x] = color;
}

/** Resolve a cursor position to an original-valid 50x50 extraction origin.
 * Midwinter excludes the generated scratch fringe rather than clamping a
 * selection onto it.
 * @param mouseX Horizontal cursor coordinate in screen pixels.
 * @param mouseY Vertical cursor coordinate in screen pixels.
 * @param originX Destination for the 100x100 grid's horizontal origin.
 * @param originY Destination for the 100x100 grid's vertical origin.
 * @return Non-zero when the cursor describes a valid extraction window.
 */
static int MapZoomOrigin(int mouseX, int mouseY,
                         int *originX, int *originY)
{
    int mapX = mouseX - MAP_LEFT;
    int mapY = mouseY - MAP_TOP;

    *originX = mapX / 2 - TERRAIN_SEED_SIZE / 2;
    *originY = mapY / 2 - TERRAIN_SEED_SIZE / 2;
    return *originX >= TERRAIN_ZOOM_X_MIN &&
           *originX <= TERRAIN_ZOOM_X_MAX &&
           *originY >= TERRAIN_ZOOM_Y_MIN &&
           *originY <= TERRAIN_ZOOM_Y_MAX;
}

/** Calculate the 100x100 preview box selected by a valid cursor position.
 * @param mouseX Horizontal cursor coordinate in screen pixels.
 * @param mouseY Vertical cursor coordinate in screen pixels.
 * @param left Destination for the box's left screen coordinate.
 * @param top Destination for the box's top screen coordinate.
 * @return Non-zero when the cursor describes a valid extraction window.
 */
static int ZoomBoxOrigin(int mouseX, int mouseY, int *left, int *top)
{
    int originX, originY;

    if (!MapZoomOrigin(mouseX, mouseY, &originX, &originY))
        return 0;
    *left = MAP_LEFT + originX * 2;
    *top = MAP_TOP + originY * 2;
    return 1;
}

/** Draw the alternating-colour outline of the next 100x100 zoom window.
 * @param screen Start of the 320x256 8-bit screen bank.
 * @param left Left screen coordinate returned by ZoomBoxOrigin.
 * @param top Top screen coordinate returned by ZoomBoxOrigin.
 */
static void DrawZoomBox(unsigned char *screen, int left, int top)
{
    int offset;

    for (offset = 0; offset < 100; ++offset)
    {
        unsigned char color = (offset & 4) ? 0 : 255;
        PutPixel(screen, left + offset, top, color);
        PutPixel(screen, left + offset, top + 99, color);
        PutPixel(screen, left, top + offset, color);
        PutPixel(screen, left + 99, top + offset, color);
    }
}

/** Draw a black-outlined white crosshair at the current mouse position.
 * @param screen Start of the 320x256 8-bit screen bank.
 * @param mouseX Horizontal cursor coordinate in screen pixels.
 * @param mouseY Vertical cursor coordinate in screen pixels.
 */
static void DrawMouseCursor(unsigned char *screen, int mouseX, int mouseY)
{
    int offset;

    for (offset = -5; offset <= 5; ++offset)
    {
        PutPixel(screen, mouseX + offset, mouseY, 0);
        PutPixel(screen, mouseX, mouseY + offset, 0);
    }
    for (offset = -3; offset <= 3; ++offset)
    {
        PutPixel(screen, mouseX + offset, mouseY, 255);
        PutPixel(screen, mouseX, mouseY + offset, 255);
    }
}

/** Configure bank one as both draw and visible bank for the map phase.
 * @param screen Destination for the selected hardware screen address.
 * @return Zero on success; non-zero if a required RISC OS SWI fails.
 */
static int SelectMapScreenBank(unsigned char **screen)
{
    _kernel_swi_regs in, out;
    _kernel_oserror *error;
    int vduVariables[3];

    /* Use bank one as both the VDU and displayed bank during map selection. */
    in.r[0] = 112;
    in.r[1] = 1;
    error = _kernel_swi(OS_Byte, &in, &out);
    if (error != NULL) return 1;
    in.r[0] = 113;
    in.r[1] = 1;
    error = _kernel_swi(OS_Byte, &in, &out);
    if (error != NULL) return 1;

    vduVariables[0] = 148;
    vduVariables[1] = -1;
    in.r[0] = (int)&vduVariables[0];
    in.r[1] = (int)&vduVariables[2];
    error = _kernel_swi(OS_ReadVduVariables, &in, &out);
    if (error != NULL) return 1;

    *screen = (unsigned char *)vduVariables[2];
    UpdateMemAddress(vduVariables[2], 0);
    memset(*screen, 0, SCREEN_W * SCREEN_H);
    return 0;
}

/** Regenerate the terrain and display cells for the current zoom path.
 * @param source Loaded pristine Midwinter terrain seed.
 * @param selection Current ordered map extraction windows.
 * @param grid Reusable destination for the generated 100x100 terrain.
 * @param display Reusable destination for the interpolated 200x200 terrain.
 * @return Zero on success; non-zero if terrain generation fails.
 */
static int RebuildMap(const TerrainSource *source,
                      const TerrainZoomPath *selection,
                      HeightCell *grid, HeightCell *display)
{
    if (TerrainBuildGrid(source, selection, 0, grid) != 0)
        return 1;
    return TerrainBuildDisplay(grid, selection->count, display);
}

/** Poll buffered keyboard input for Space without waiting.
 * This avoids depending on a keyboard-layout-specific internal key number.
 * @return Non-zero if a Space character was read; zero otherwise.
 */
static int ReadSpace(void)
{
    _kernel_swi_regs in, out;
    _kernel_oserror *error;

    in.r[0] = 129;
    in.r[1] = 0;
    in.r[2] = 0;
    error = _kernel_swi(OS_Byte, &in, &out);
    return error == NULL && out.r[2] == 0 && out.r[1] == ' ';
}

/** Run single-buffered map selection until Space accepts or Escape cancels.
 * @param source Loaded pristine Midwinter terrain seed.
 * @param selection Destination zoom path and cursor-selected player position.
 * @return Zero when accepted, one when cancelled, or -1 on an error.
 */
int RunMapScreen(const TerrainSource *source, TerrainZoomPath *selection)
{
#ifdef PAL_256
    HeightCell *grid;
    HeightCell *display;
    unsigned char *bitmap;
    unsigned char *screen;
    _kernel_swi_regs in, out;
    _kernel_oserror *error;
    int previousButtons = 0;
    int mousePixelX = -1;
    int mousePixelY = -1;
    int drawnMouseX = -20;
    int drawnMouseY = -20;
    int drawnBoxLeft = 0;
    int drawnBoxTop = 0;
    int drawnBoxVisible = 0;
    int mapDirty = 1;
    int result = 1;

    if (source == NULL || selection == NULL)
        return -1;

    grid = (HeightCell *)malloc(TERRAIN_GRID_SIZE * TERRAIN_GRID_SIZE *
                                sizeof(HeightCell));
    display = (HeightCell *)malloc(TERRAIN_DISPLAY_SIZE * TERRAIN_DISPLAY_SIZE *
                                   sizeof(HeightCell));
    bitmap = (unsigned char *)malloc(TERRAIN_DISPLAY_SIZE *
                                     TERRAIN_DISPLAY_SIZE);
    if (grid == NULL || display == NULL || bitmap == NULL)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }

    memset(selection, 0, sizeof(*selection));
    if (RebuildMap(source, selection, grid, display) != 0)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }
    RenderMapBitmap(bitmap, display, selection->count);

    if (SelectMapScreenBank(&screen) != 0)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }

    for (;;)
    {
        int buttons;
        int pressed;
        int zoomOriginX, zoomOriginY;
        int validZoomOrigin;

        error = _kernel_swi(OS_Mouse, &in, &out);
        if (error != NULL)
            break;
        buttons = out.r[2];
        pressed = buttons & ~previousButtons;
        mousePixelX = out.r[0] >> 1;
        mousePixelY = SCREEN_H - 1 - (out.r[1] >> 1);
        validZoomOrigin = IsMapInterior(mousePixelX, mousePixelY) &&
            MapZoomOrigin(mousePixelX, mousePixelY,
                          &zoomOriginX, &zoomOriginY);

        if (KeyPress(112))
            break;
        if (ReadSpace() && IsMapInterior(mousePixelX, mousePixelY) &&
            (selection->count == TERRAIN_MAX_ZOOM || validZoomOrigin))
        {
            TerrainSelectPlayer(selection, mousePixelX - MAP_LEFT,
                                mousePixelY - MAP_TOP);
            result = 0;
            break;
        }

        if ((pressed & 4) && selection->count < TERRAIN_MAX_ZOOM)
        {
            /* Mode 13 has two OS units per pixel and OS_Mouse measures Y up
               from the bottom of the screen. */
            if (validZoomOrigin)
            {
                int index = selection->count;

                selection->x[index] = (unsigned char)zoomOriginX;
                selection->y[index] = (unsigned char)zoomOriginY;
                ++selection->count;
                if (RebuildMap(source, selection, grid, display) != 0)
                {
                    result = -1;
                    break;
                }
                RenderMapBitmap(bitmap, display, selection->count);
                mapDirty = 1;
            }
        }
        else if ((pressed & 1) && selection->count > 0)
        {
            --selection->count;
            if (RebuildMap(source, selection, grid, display) != 0)
            {
                result = -1;
                break;
            }
            RenderMapBitmap(bitmap, display, selection->count);
            mapDirty = 1;
        }
        previousButtons = buttons;

        /* Wait for refresh, then redraw the one visible map bank in place. */
        in.r[0] = 19;
        error = _kernel_swi(OS_Byte, &in, &out);
        if (error != NULL)
        {
            result = -1;
            break;
        }
        if (mapDirty)
        {
            ClearMapSurroundings(screen);
            BlitMap(screen, bitmap);
            mapDirty = 0;
        }
        else
        {
            if (drawnBoxVisible)
                RestoreRect(screen, bitmap, drawnBoxLeft, drawnBoxTop,
                            100, 100);
            RestoreRect(screen, bitmap, drawnMouseX - 5, drawnMouseY - 5,
                        11, 11);
        }

        drawnBoxVisible = 0;
        if (selection->count < TERRAIN_MAX_ZOOM &&
            validZoomOrigin &&
            ZoomBoxOrigin(mousePixelX, mousePixelY,
                          &drawnBoxLeft, &drawnBoxTop))
        {
            DrawZoomBox(screen, drawnBoxLeft, drawnBoxTop);
            drawnBoxVisible = 1;
        }
        DrawMouseCursor(screen, mousePixelX, mousePixelY);
        drawnMouseX = mousePixelX;
        drawnMouseY = mousePixelY;
    }

    free(grid);
    free(display);
    free(bitmap);
    return result;
#else
    (void)source;
    (void)selection;
    return -1;
#endif
}
