#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "terrain.h"

#define ZBUFFER_SIZE 10000

/** Interpret a raw height word using Midwinter's signed 16-bit convention.
 * @param value Unsigned word read from a terrain cell.
 * @return The same bit pattern interpreted as a signed height.
 */
short TerrainSignedHeight(unsigned short value)
{
    return (short)value;
}

/** Generate one deterministic midpoint-subdivision terrain cell.
 * @param a First corner and source of the generated hash bits.
 * @param b Second corner surrounding the new sample.
 * @param c Third corner surrounding the new sample.
 * @param d Fourth corner surrounding the new sample.
 * @param shift Roughness shift for the current subdivision level.
 * @return The generated fixed-point height and deterministic detail/hash word.
 */
static HeightCell Midpoint(HeightCell a, HeightCell b, HeightCell c,
                           HeightCell d, int shift)
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
    average = TerrainSignedHeight((unsigned short)(sum >> 16)) >> 2;
    base = TerrainSignedHeight(sumLow) >> shift;
    multiplier = TerrainSignedHeight((unsigned short)(average * 8 + 0x1388));
    product = (int)base * (int)multiplier;

    result.height = (unsigned short)(average + (product >> 16));
    result.detail = (unsigned short)((sumLow ^ a.detail) & 0x3fff);
    return result;
}

/** Double a square terrain grid using Midwinter's decoded subdivision driver.
 * Midwinter generates one fewer midpoint than it interleaves at each edge.
 * Use explicit all-ones scratch sentinels in those unwritten slots rather
 * than duplicating nearby terrain, preserving the original loop topology
 * without depending on uninitialized memory.
 * @param source Row-major source grid containing sourceSize squared cells.
 * @param sourceSize Width and height of the square source grid.
 * @param output Destination for a grid with twice the source dimensions.
 * @param shift Roughness shift applied to newly generated midpoint samples.
 * @return Zero on success; non-zero if scratch allocation fails.
 */
static int Subdivide(const HeightCell *source, int sourceSize,
                     HeightCell *output, int shift)
{
    HeightCell *centers;
    HeightCell *previousCenters;
    HeightCell *horizontal;
    HeightCell *vertical;
    HeightCell sentinel;
    int outputSize;
    int x, y;

    centers = (HeightCell *)malloc(sourceSize * sizeof(HeightCell) * 4);
    if (centers == NULL)
        return 1;
    previousCenters = centers + sourceSize;
    horizontal = previousCenters + sourceSize;
    vertical = horizontal + sourceSize;
    outputSize = sourceSize * 2;

    sentinel.height = 0xffff;
    sentinel.detail = 0xffff;
    memset(centers, 0xff, sourceSize * sizeof(HeightCell) * 4);

    for (y = 0; y < sourceSize; ++y)
    {
        for (x = 0; x < sourceSize - 1; ++x)
        {
            if (y < sourceSize - 1)
                centers[x] = Midpoint(source[y * sourceSize + x],
                                      source[y * sourceSize + x + 1],
                                      source[(y + 1) * sourceSize + x],
                                      source[(y + 1) * sourceSize + x + 1], shift);
            else
                centers[x] = Midpoint(source[y * sourceSize + x],
                                      source[y * sourceSize + x + 1],
                                      sentinel, sentinel, shift);
        }

        for (x = 0; x < sourceSize - 1; ++x)
            horizontal[x] = Midpoint(centers[x], previousCenters[x],
                                     source[y * sourceSize + x],
                                     source[y * sourceSize + x + 1], shift);

        for (x = 0; x < sourceSize - 1; ++x)
        {
            if (y < sourceSize - 1)
                vertical[x] = Midpoint(centers[x], centers[x + 1],
                                       source[y * sourceSize + x + 1],
                                       source[(y + 1) * sourceSize + x + 1], shift);
            else
                vertical[x] = Midpoint(centers[x], centers[x + 1],
                                       source[y * sourceSize + x + 1],
                                       sentinel, shift);
        }

        for (x = 0; x < sourceSize; ++x)
        {
            output[(y * 2) * outputSize + x * 2] =
                source[y * sourceSize + x];
            output[(y * 2) * outputSize + x * 2 + 1] = horizontal[x];
            output[(y * 2 + 1) * outputSize + x * 2] =
                (x == 0) ? sentinel : vertical[x - 1];
            output[(y * 2 + 1) * outputSize + x * 2 + 1] = centers[x];
        }

        for (x = 0; x < sourceSize; ++x)
            previousCenters[x] = centers[x];
    }

    free(centers);
    return 0;
}

/** Load and byte-swap the original 50x50 Midwinter terrain seed.
 * @param source Destination for the decoded height and detail cells.
 * @param baseDirectoryPath RISC OS application path containing assets.ZBUFFER.
 * @return Zero on success; non-zero for invalid arguments or file errors.
 */
int TerrainLoad(TerrainSource *source, const char *baseDirectoryPath)
{
    unsigned char bytes[ZBUFFER_SIZE];
    FILE *file;
    char filename[256];
    int index;

    if (source == NULL || baseDirectoryPath == NULL)
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

    for (index = 0; index < TERRAIN_SEED_SIZE * TERRAIN_SEED_SIZE; ++index)
    {
        int offset = index * 4;
        source->seed[index].height =
            (unsigned short)((bytes[offset] << 8) | bytes[offset + 1]);
        source->seed[index].detail =
            (unsigned short)((bytes[offset + 2] << 8) | bytes[offset + 3]);
    }
    return 0;
}

/** Generate the 100x100 terrain grid represented by a map zoom path.
 * @param source Pristine 50x50 terrain seed loaded from ZBUFFER.
 * @param path Ordered 50x50 extraction windows; NULL selects the full map.
 * @param completeZoom Non-zero to append centered windows through level four.
 * @param output Destination for TERRAIN_GRID_SIZE squared generated cells.
 * @return Zero on success; non-zero if an allocation or subdivision fails.
 */
int TerrainBuildGrid(const TerrainSource *source, const TerrainZoomPath *path,
                     int completeZoom, HeightCell *output)
{
    HeightCell *window;
    TerrainZoomPath emptyPath;
    int zoomCount;
    int level, x, y;

    if (source == NULL || output == NULL)
        return 1;
    if (path == NULL)
    {
        memset(&emptyPath, 0, sizeof(emptyPath));
        path = &emptyPath;
    }

    window = (HeightCell *)malloc(TERRAIN_SEED_SIZE * TERRAIN_SEED_SIZE *
                                  sizeof(HeightCell));
    if (window == NULL)
        return 1;
    memcpy(window, source->seed, TERRAIN_SEED_SIZE * TERRAIN_SEED_SIZE *
                                 sizeof(HeightCell));

    if (Subdivide(window, TERRAIN_SEED_SIZE, output, 4) != 0)
    {
        free(window);
        return 1;
    }

    zoomCount = completeZoom ? TERRAIN_MAX_ZOOM : path->count;
    if (zoomCount < 0)
        zoomCount = 0;
    if (zoomCount > TERRAIN_MAX_ZOOM)
        zoomCount = TERRAIN_MAX_ZOOM;

    for (level = 0; level < zoomCount; ++level)
    {
        int originX = (level < path->count) ? path->x[level] : 25;
        int originY = (level < path->count) ? path->y[level] : 25;
        if (originX < TERRAIN_ZOOM_X_MIN) originX = TERRAIN_ZOOM_X_MIN;
        if (originX > TERRAIN_ZOOM_X_MAX) originX = TERRAIN_ZOOM_X_MAX;
        if (originY < TERRAIN_ZOOM_Y_MIN) originY = TERRAIN_ZOOM_Y_MIN;
        if (originY > TERRAIN_ZOOM_Y_MAX) originY = TERRAIN_ZOOM_Y_MAX;

        for (y = 0; y < TERRAIN_SEED_SIZE; ++y)
            for (x = 0; x < TERRAIN_SEED_SIZE; ++x)
                window[y * TERRAIN_SEED_SIZE + x] =
                    output[(originY + y) * TERRAIN_GRID_SIZE + originX + x];

        if (Subdivide(window, TERRAIN_SEED_SIZE, output, level + 4) != 0)
        {
            free(window);
            return 1;
        }
    }

    free(window);
    return 0;
}

/** Interpolate a generated terrain grid to the 200x200 map resolution.
 * @param grid Source TERRAIN_GRID_SIZE squared terrain cells.
 * @param zoomLevel Current map zoom, used to select midpoint roughness.
 * @param display Destination for TERRAIN_DISPLAY_SIZE squared cells.
 * @return Zero on success; non-zero for invalid arguments or allocation failure.
 */
int TerrainBuildDisplay(const HeightCell *grid, int zoomLevel,
                        HeightCell *display)
{
    HeightCell sentinel;
    int row;

    if (grid == NULL || display == NULL)
        return 1;
    if (zoomLevel < 0) zoomLevel = 0;
    if (zoomLevel > TERRAIN_MAX_ZOOM) zoomLevel = TERRAIN_MAX_ZOOM;
    if (Subdivide(grid, TERRAIN_GRID_SIZE, display, zoomLevel + 4) != 0)
        return 1;

    /* The DOS display pass emits 99 pairs into each even-row scratch buffer.
       Its final source/pair slots therefore retain the 0xffff sentinel. */
    sentinel.height = 0xffff;
    sentinel.detail = 0xffff;
    for (row = 0; row < TERRAIN_GRID_SIZE; ++row)
        display[(row * 2) * TERRAIN_DISPLAY_SIZE +
                TERRAIN_DISPLAY_SIZE - 2] = sentinel;
    return 0;
}

/** Convert the current viewport center to Midwinter strategic world units.
 * @param path Zoom windows defining the current viewport; NULL means full map.
 * @param worldX Optional destination for the east/west world coordinate.
 * @param worldY Optional destination for the north/south world coordinate.
 */
void TerrainZoomCenter(const TerrainZoomPath *path, int *worldX, int *worldY)
{
    int originX = 0;
    int originY = 0;
    int extent = 3200;
    int count = path == NULL ? 0 : path->count;
    int level;

    if (count < 0) count = 0;
    if (count > TERRAIN_MAX_ZOOM) count = TERRAIN_MAX_ZOOM;
    for (level = 0; level < count; ++level)
    {
        originX += path->x[level] * (extent / TERRAIN_GRID_SIZE);
        originY += path->y[level] * (extent / TERRAIN_GRID_SIZE);
        extent /= 2;
    }

    if (worldX != NULL) *worldX = originX + extent / 2;
    if (worldY != NULL) *worldY = originY + extent / 2;
}

/** Preserve a map cursor position while generating all remaining detail levels.
 * The function appends clamped extraction windows and records the cursor as a
 * 16.16 cell coordinate within the final 100x100 terrain grid.
 * @param path Current zoom path, updated in place to contain four levels.
 * @param displayX Horizontal cursor coordinate in the 200x200 map.
 * @param displayY Vertical cursor coordinate in the 200x200 map.
 */
void TerrainSelectPlayer(TerrainZoomPath *path, int displayX, int displayY)
{
    int positionX;
    int positionY;

    if (path == NULL)
        return;

    displayX = displayX < 0 ? 0 : (displayX > 199 ? 199 : displayX);
    displayY = displayY < 0 ? 0 : (displayY > 199 ? 199 : displayY);
    positionX = displayX << 15;
    positionY = displayY << 15;

    while (path->count < TERRAIN_MAX_ZOOM)
    {
        int originX = (positionX >> 16) - TERRAIN_SEED_SIZE / 2;
        int originY = (positionY >> 16) - TERRAIN_SEED_SIZE / 2;
        int level = path->count;

        if (originX < TERRAIN_ZOOM_X_MIN) originX = TERRAIN_ZOOM_X_MIN;
        if (originX > TERRAIN_ZOOM_X_MAX) originX = TERRAIN_ZOOM_X_MAX;
        if (originY < TERRAIN_ZOOM_Y_MIN) originY = TERRAIN_ZOOM_Y_MIN;
        if (originY > TERRAIN_ZOOM_Y_MAX) originY = TERRAIN_ZOOM_Y_MAX;
        path->x[level] = (unsigned char)originX;
        path->y[level] = (unsigned char)originY;
        ++path->count;

        positionX = (positionX - (originX << 16)) << 1;
        positionY = (positionY - (originY << 16)) << 1;
    }

    path->playerGridX = positionX;
    path->playerGridY = positionY;
}

/** Convert the selected final-grid player position to strategic world units.
 * @param path Completed zoom path and 16.16 local player coordinates.
 * @param worldX Optional destination for the east/west world coordinate.
 * @param worldY Optional destination for the north/south world coordinate.
 */
void TerrainPlayerWorldPosition(const TerrainZoomPath *path,
                                int *worldX, int *worldY)
{
    int originX = 0;
    int originY = 0;
    int extent = 3200;
    int count = path == NULL ? 0 : path->count;
    int level;

    if (count < 0) count = 0;
    if (count > TERRAIN_MAX_ZOOM) count = TERRAIN_MAX_ZOOM;
    for (level = 0; level < count; ++level)
    {
        originX += path->x[level] * (extent / TERRAIN_GRID_SIZE);
        originY += path->y[level] * (extent / TERRAIN_GRID_SIZE);
        extent /= 2;
    }

    if (path != NULL)
    {
        int cellSize = extent / TERRAIN_GRID_SIZE;
        originX += (path->playerGridX * cellSize) >> 16;
        originY += (path->playerGridY * cellSize) >> 16;
    }
    if (worldX != NULL) *worldX = originX;
    if (worldY != NULL) *worldY = originY;
}
