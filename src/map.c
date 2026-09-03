#include <stdlib.h>
#include <string.h>

#include <kernel.h>
#include <swis.h>

#include "map.h"
#include "palette.h"
#include "render.h"

#define MAP_LEFT 0
#define MAP_TOP 0
#define MAP_FRAME_WIDTH 3
#define KEY_F2 114

#ifdef PAL_256
#define MAP_SEA_COLOR 203
#define MAP_HIGHLIGHT_COLOR 255
#define MAP_DARK_COLOR 0
#define SCREEN_STRIDE SCREEN_W
#define MAP_BITMAP_STRIDE TERRAIN_DISPLAY_SIZE
#define PIXELS_PER_WORD 4
#define PIXEL_BYTE_OFFSET(x) (x)
#else
#define MAP_SEA_COLOR 1
#define MAP_HIGHLIGHT_COLOR 9
#define MAP_DARK_COLOR 15
#define SCREEN_STRIDE (SCREEN_W / 2)
#define MAP_BITMAP_STRIDE (TERRAIN_DISPLAY_SIZE / 2)
#define PIXELS_PER_WORD 8
#define PIXEL_BYTE_OFFSET(x) ((x) >> 1)
#endif

extern void UpdateMemAddress(int screenStart, int screenMax);
extern int KeyPress(int keyCode);

#ifdef PAL_256
/* Mode 13 indices for its sixteen exact grey ramp entries. */
static const unsigned char reliefColors[16] = {
    0, 1, 2, 3, 44, 45, 46, 47,
    208, 209, 210, 211, 252, 253, 254, 255
};

/* Three exact Mode 13 greys forming the frame's outer-to-inner rings. */
static const unsigned char mapFrameColors[MAP_FRAME_WIDTH] = {
    0, 208, 255
};
#else
/* Ordered darkest-to-lightest relief indices recovered from Amiga Chip RAM. */
static const unsigned char reliefColors[7] = {
    15, 3, 6, 10, 11, 12, 14
};

static const unsigned char mapFrameColors[MAP_FRAME_WIDTH] = {
    15, 9, 15
};
#endif

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

/** Write one logical pixel to a buffer in the active screen format. */
static void WriteBufferPixel(unsigned char *buffer, int stride, int x, int y,
                             unsigned char color)
{
#ifdef PAL_256
    buffer[y * stride + x] = color;
#else
    unsigned char *packed = buffer + y * stride + (x >> 1);

    if (x & 1)
        *packed = (unsigned char)((*packed & 0x0f) | (color << 4));
    else
        *packed = (unsigned char)((*packed & 0xf0) | (color & 0x0f));
#endif
}

/** Write one logical pixel to the hardware screen. */
static void WriteScreenPixel(unsigned char *screen, int x, int y,
                             unsigned char color)
{
    WriteBufferPixel(screen, SCREEN_STRIDE, x, y, color);
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

/** Select the relief-map colour for one land sample and its eastern neighbor.
 * The VGA painter uses a 64-shade ramp.  The Amiga painter uses seven palette
 * entries, a three-bit coarser slope shift, and a brighter neutral bias.
 * @param rawHeight Midwinter signed height stored in an unsigned word.
 * @param rawEastHeight Height of the sample immediately to the east.
 * @param zoomLevel Current map zoom level from zero through four.
 * @return The active mode's sea colour or one of its ordered relief colours.
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
#ifdef PAL_256
    shade = ((eastHeight - height) >> (6 - zoomLevel)) + 32;
    shade = ClampInt(shade, 0, 63);
    return reliefColors[shade >> 2];
#else
    shade = ((eastHeight - height) >> (9 - zoomLevel)) + 4;
    return reliefColors[ClampInt(shade, 0, 6)];
#endif
}

static void DrawMapFrame(unsigned char *bitmap);

/** Convert interpolated terrain cells into a cached native-format bitmap.
 * @param bitmap Destination map buffer in the active screen format.
 * @param display Source 200x200 terrain display cells.
 * @param zoomLevel Current map zoom level from zero through four.
 */
static void RenderMapBitmap(unsigned char *bitmap, const HeightCell *display,
                            int zoomLevel)
{
    int x, y;

    for (y = 0; y < TERRAIN_DISPLAY_SIZE; ++y)
    {
        unsigned char *row = bitmap + y * MAP_BITMAP_STRIDE;
#ifdef PAL_256
        for (x = 0; x < TERRAIN_DISPLAY_SIZE; ++x)
        {
            int index = y * TERRAIN_DISPLAY_SIZE + x;
            unsigned short eastHeight = (x + 1 < TERRAIN_DISPLAY_SIZE)
                ? display[index + 1].height : 0xffff;
            row[x] = ColorForRelief(
                display[index].height, eastHeight, zoomLevel);
        }
#else
        for (x = 0; x < TERRAIN_DISPLAY_SIZE; x += 2)
        {
            int index = y * TERRAIN_DISPLAY_SIZE + x;
            unsigned short eastHeight = display[index + 1].height;
            unsigned char first = ColorForRelief(
                display[index].height, eastHeight, zoomLevel);
            unsigned short finalEastHeight =
                (x + 2 < TERRAIN_DISPLAY_SIZE)
                ? display[index + 2].height : 0xffff;
            unsigned char second = ColorForRelief(
                display[index + 1].height, finalEastHeight, zoomLevel);

            row[x >> 1] = (unsigned char)(first | (second << 4));
        }
#endif
    }
    DrawMapFrame(bitmap);
}

/** Draw the three-pixel bevel over the undefined terrain fringe. */
static void DrawMapFrame(unsigned char *bitmap)
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
            WriteBufferPixel(bitmap, MAP_BITMAP_STRIDE, x, top, color);
            WriteBufferPixel(bitmap, MAP_BITMAP_STRIDE, x, bottom, color);
            WriteBufferPixel(bitmap, MAP_BITMAP_STRIDE, left, y, color);
            WriteBufferPixel(bitmap, MAP_BITMAP_STRIDE, right, y, color);
        }
    }
}

/** Copy the cached map bitmap into the map area of the hardware screen.
 * @param screen Start of the active 320x256 screen bank.
 * @param bitmap Source native-format 200x200 map buffer.
 */
static void BlitMap(unsigned char *screen, const unsigned char *bitmap)
{
    int y;

    for (y = 0; y < TERRAIN_DISPLAY_SIZE; ++y)
        memcpy(screen + (MAP_TOP + y) * SCREEN_STRIDE +
                   PIXEL_BYTE_OFFSET(MAP_LEFT),
               bitmap + y * MAP_BITMAP_STRIDE,
               MAP_BITMAP_STRIDE);
}

/** Clear the hardware screen before redrawing a changed map.
 * @param screen Start of the active 320x256 screen bank.
 */
static void ClearMapScreen(unsigned char *screen)
{
    memset(screen, 0, SCREEN_STRIDE * SCREEN_H);
}

/** Restore a damaged overlay rectangle from the map bitmap or background.
 * The horizontal bounds are expanded to native 32-bit word boundaries.  This
 * deliberately restores a few extra pixels so the hot path uses aligned word
 * loads and stores instead of individual byte writes.
 * @param screen Start of the active 320x256 screen bank.
 * @param bitmap Cached native-format 200x200 map pixels, including its frame.
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
    left &= ~(PIXELS_PER_WORD - 1);
    right = (right + PIXELS_PER_WORD - 1) & ~(PIXELS_PER_WORD - 1);
    right = ClampInt(right, 0, SCREEN_W);
    for (y = top; y < bottom; ++y)
    {
        unsigned int *destination = (unsigned int *)(
            screen + y * SCREEN_STRIDE + PIXEL_BYTE_OFFSET(left));

        for (x = left; x < right; x += PIXELS_PER_WORD)
        {
            if (y >= MAP_TOP &&
                y < MAP_TOP + TERRAIN_DISPLAY_SIZE &&
                x >= MAP_LEFT &&
                x < MAP_LEFT + TERRAIN_DISPLAY_SIZE)
            {
                const unsigned int *source = (const unsigned int *)(
                    bitmap + (y - MAP_TOP) * MAP_BITMAP_STRIDE +
                    PIXEL_BYTE_OFFSET(x - MAP_LEFT));
                *destination = *source;
            }
            else
                *destination = 0;
            ++destination;
        }
    }
}

/** Restore only the four one-pixel strips occupied by a zoom-box outline. */
static void RestoreZoomBox(unsigned char *screen,
                           const unsigned char *bitmap, int left, int top)
{
    RestoreRect(screen, bitmap, left, top, 100, 1);
    RestoreRect(screen, bitmap, left, top + 99, 100, 1);
    RestoreRect(screen, bitmap, left, top + 1, 1, 98);
    RestoreRect(screen, bitmap, left + 99, top + 1, 1, 98);
}

/** Write one clipped overlay pixel without allowing it to damage the frame.
 * @param screen Start of the active 320x256 screen bank.
 * @param x Horizontal screen coordinate.
 * @param y Vertical screen coordinate.
 * @param color Palette index to write.
 */
static void PutPixel(unsigned char *screen, int x, int y,
                     unsigned char color)
{
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H &&
        !IsMapFramePixel(x, y))
        WriteScreenPixel(screen, x, y, color);
}

/** Resolve a cursor position to the nearest valid 50x50 extraction origin.
 * @param mouseX Horizontal cursor coordinate in screen pixels.
 * @param mouseY Vertical cursor coordinate in screen pixels.
 * @param originX Destination for the 100x100 grid's horizontal origin.
 * @param originY Destination for the 100x100 grid's vertical origin.
 * @return Non-zero while the cursor is over the map; the origin is clamped.
 */
static int MapZoomOrigin(int mouseX, int mouseY,
                         int *originX, int *originY)
{
    int mapX = mouseX - MAP_LEFT;
    int mapY = mouseY - MAP_TOP;

    *originX = ClampInt(mapX / 2 - TERRAIN_SEED_SIZE / 2,
                        TERRAIN_ZOOM_X_MIN, TERRAIN_ZOOM_X_MAX);
    *originY = ClampInt(mapY / 2 - TERRAIN_SEED_SIZE / 2,
                        TERRAIN_ZOOM_Y_MIN, TERRAIN_ZOOM_Y_MAX);
    return IsMapPixel(mouseX, mouseY);
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
 * @param screen Start of the active 320x256 screen bank.
 * @param left Left screen coordinate returned by ZoomBoxOrigin.
 * @param top Top screen coordinate returned by ZoomBoxOrigin.
 */
static void DrawZoomBox(unsigned char *screen, int left, int top)
{
    int offset;

    for (offset = 0; offset < 100; ++offset)
    {
        unsigned char color = (offset & 4)
            ? MAP_DARK_COLOR : MAP_HIGHLIGHT_COLOR;
        PutPixel(screen, left + offset, top, color);
        PutPixel(screen, left + offset, top + 99, color);
        PutPixel(screen, left, top + offset, color);
        PutPixel(screen, left + 99, top + offset, color);
    }
}

/** Draw a dark-outlined bright crosshair at the current mouse position.
 * @param screen Start of the active 320x256 screen bank.
 * @param mouseX Horizontal cursor coordinate in screen pixels.
 * @param mouseY Vertical cursor coordinate in screen pixels.
 */
static void DrawMouseCursor(unsigned char *screen, int mouseX, int mouseY)
{
    int offset;

    for (offset = -5; offset <= 5; ++offset)
    {
        PutPixel(screen, mouseX + offset, mouseY, MAP_DARK_COLOR);
        PutPixel(screen, mouseX, mouseY + offset, MAP_DARK_COLOR);
    }
    for (offset = -3; offset <= 3; ++offset)
    {
        PutPixel(screen, mouseX + offset, mouseY, MAP_HIGHLIGHT_COLOR);
        PutPixel(screen, mouseX, mouseY + offset, MAP_HIGHLIGHT_COLOR);
    }
}

/** Draw the selected player position as a 5x5 dark square with a bright
 * 3x3 interior. */
static void DrawPlayerMarker(unsigned char *screen, int playerX, int playerY)
{
    int x, y;

    for (y = -2; y <= 2; ++y)
        for (x = -2; x <= 2; ++x)
            PutPixel(screen, playerX + x, playerY + y,
                     x == -2 || x == 2 || y == -2 || y == 2
                         ? MAP_DARK_COLOR : MAP_HIGHLIGHT_COLOR);
}

/** Project a selected player's world position into the current map view. */
static int PlayerMapPosition(const TerrainZoomPath *view,
                             const TerrainZoomPath *player,
                             int *playerX, int *playerY)
{
    int centerX, centerY;
    int worldX, worldY;
    int pixelSize = 1 << (TERRAIN_MAX_ZOOM - view->count);
    int halfExtent = TERRAIN_DISPLAY_SIZE / 2 * pixelSize;
    int offsetX, offsetY;

    TerrainZoomCenter(view, &centerX, &centerY);
    TerrainPlayerWorldPosition(player, &worldX, &worldY);
    offsetX = worldX - centerX + halfExtent;
    offsetY = worldY - centerY + halfExtent;
    if (offsetX < 0 || offsetX >= halfExtent * 2 ||
        offsetY < 0 || offsetY >= halfExtent * 2)
        return 0;
    *playerX = MAP_LEFT + offsetX / pixelSize;
    *playerY = MAP_TOP + offsetY / pixelSize;
    return 1;
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
    memset(*screen, 0, SCREEN_STRIDE * SCREEN_H);
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

/** Run single-buffered map selection until F2 accepts or Escape cancels.
 * @param source Loaded pristine Midwinter terrain seed.
 * @param view Persistent map zoom path.
 * @param playerSelection Persistent selected player location.
 * @return Zero when accepted, one when cancelled, or -1 on an error.
 */
int RunMapScreen(const TerrainSource *source, TerrainZoomPath *view,
                 TerrainZoomPath *playerSelection)
{
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
    TerrainZoomPath selectedPlayer;
    int playerX = 0;
    int playerY = 0;
    int playerZoomLevel = 0;
    int mapDirty = 1;
    int result = 1;

    if (source == NULL || view == NULL || playerSelection == NULL)
        return -1;

    selectedPlayer = *playerSelection;

    grid = (HeightCell *)malloc(TERRAIN_GRID_SIZE * TERRAIN_GRID_SIZE *
                                sizeof(HeightCell));
    display = (HeightCell *)malloc(TERRAIN_DISPLAY_SIZE * TERRAIN_DISPLAY_SIZE *
                                   sizeof(HeightCell));
    bitmap = (unsigned char *)malloc(MAP_BITMAP_STRIDE *
                                     TERRAIN_DISPLAY_SIZE);
    if (grid == NULL || display == NULL || bitmap == NULL)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }

    if (RebuildMap(source, view, grid, display) != 0)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }
    RenderMapBitmap(bitmap, display, view->count);

    if (SelectMapScreenBank(&screen) != 0)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }
#ifndef PAL_256
    SetMapPalette();
#else
    in.r[0] = 20;
    error = _kernel_swi(OS_WriteC, &in, &out);
    if (error != NULL)
    {
        free(grid);
        free(display);
        free(bitmap);
        return -1;
    }
#endif

    for (;;)
    {
        int buttons;
        int pressed;
        int zoomOriginX, zoomOriginY;
        int validZoomOrigin;
        int boxLeft = 0;
        int boxTop = 0;
        int boxVisible;
        int boxChanged;
        int mouseChanged;
        int playerVisible;

        error = _kernel_swi(OS_Mouse, &in, &out);
        if (error != NULL)
            break;
        buttons = out.r[2];
        pressed = buttons & ~previousButtons;
        mousePixelX = out.r[0] >> 1;
        mousePixelY = SCREEN_H - 1 - (out.r[1] >> 1);
        validZoomOrigin = MapZoomOrigin(mousePixelX, mousePixelY,
                                        &zoomOriginX, &zoomOriginY);

        if (KeyPress(112))
            break;
        if (KeyPress(KEY_F2))
        {
            if (view->count >= playerZoomLevel &&
                PlayerMapPosition(view, &selectedPlayer,
                                  &playerX, &playerY))
            {
                selectedPlayer = *view;
                TerrainSelectPlayer(&selectedPlayer,
                                    playerX - MAP_LEFT, playerY - MAP_TOP);
            }
            *playerSelection = selectedPlayer;
            result = 0;
            break;
        }

        if ((pressed & 2) && IsMapInterior(mousePixelX, mousePixelY))
        {
            selectedPlayer = *view;
            TerrainSelectPlayer(&selectedPlayer,
                                mousePixelX - MAP_LEFT,
                                mousePixelY - MAP_TOP);
            playerZoomLevel = view->count;
            mapDirty = 1;
        }

        if ((pressed & 4) && view->count < TERRAIN_MAX_ZOOM)
        {
            /* These modes have two OS units per pixel and OS_Mouse measures
               Y up from the bottom of the screen. */
            if (validZoomOrigin)
            {
                int index = view->count;

                view->x[index] = (unsigned char)zoomOriginX;
                view->y[index] = (unsigned char)zoomOriginY;
                ++view->count;
                if (RebuildMap(source, view, grid, display) != 0)
                {
                    result = -1;
                    break;
                }
                RenderMapBitmap(bitmap, display, view->count);
                mapDirty = 1;
            }
        }
        else if ((pressed & 1) && view->count > 0)
        {
            --view->count;
            if (RebuildMap(source, view, grid, display) != 0)
            {
                result = -1;
                break;
            }
            RenderMapBitmap(bitmap, display, view->count);
            mapDirty = 1;
        }
        previousButtons = buttons;
        boxVisible = view->count < TERRAIN_MAX_ZOOM &&
            validZoomOrigin &&
            ZoomBoxOrigin(mousePixelX, mousePixelY, &boxLeft, &boxTop);
        playerVisible = PlayerMapPosition(view, &selectedPlayer,
                                          &playerX, &playerY);

        /* Wait for refresh, then redraw the one visible map bank in place. */
        in.r[0] = 19;
        error = _kernel_swi(OS_Byte, &in, &out);
        if (error != NULL)
        {
            result = -1;
            break;
        }
        boxChanged = drawnBoxVisible != boxVisible ||
            (boxVisible && (drawnBoxLeft != boxLeft ||
                            drawnBoxTop != boxTop));
        mouseChanged = drawnMouseX != mousePixelX ||
            drawnMouseY != mousePixelY;
        if (mapDirty)
        {
            ClearMapScreen(screen);
            BlitMap(screen, bitmap);
        }
        else
        {
            if (boxChanged && drawnBoxVisible)
                RestoreZoomBox(screen, bitmap,
                               drawnBoxLeft, drawnBoxTop);
            if (mouseChanged)
                RestoreRect(screen, bitmap,
                            drawnMouseX - 5, drawnMouseY - 5, 11, 11);
        }

        if (mapDirty || boxChanged || mouseChanged)
        {
            if (boxVisible)
                DrawZoomBox(screen, boxLeft, boxTop);
            DrawMouseCursor(screen, mousePixelX, mousePixelY);
            if (playerVisible)
                DrawPlayerMarker(screen, playerX, playerY);
        }

        mapDirty = 0;
        drawnBoxVisible = boxVisible;
        drawnBoxLeft = boxLeft;
        drawnBoxTop = boxTop;
        drawnMouseX = mousePixelX;
        drawnMouseY = mousePixelY;
    }

    free(grid);
    free(display);
    free(bitmap);
#ifndef PAL_256
    SetPalette();
#endif
    return result;
}
