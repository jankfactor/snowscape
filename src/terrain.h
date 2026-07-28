#ifndef TERRAIN_H
#define TERRAIN_H

#define TERRAIN_SEED_SIZE 50
#define TERRAIN_GRID_SIZE 100
#define TERRAIN_DISPLAY_SIZE 200
#define TERRAIN_MAX_ZOOM 4
/* Original extraction limits exclude the subdivision scratch fringe. */
#define TERRAIN_ZOOM_X_MIN 1
#define TERRAIN_ZOOM_X_MAX 48
#define TERRAIN_ZOOM_Y_MIN 1
#define TERRAIN_ZOOM_Y_MAX 49

typedef struct HeightCell
{
    unsigned short height;
    unsigned short detail;
} HeightCell;

typedef struct TerrainSource
{
    HeightCell seed[TERRAIN_SEED_SIZE * TERRAIN_SEED_SIZE];
} TerrainSource;

typedef struct TerrainZoomPath
{
    int count;
    unsigned char x[TERRAIN_MAX_ZOOM];
    unsigned char y[TERRAIN_MAX_ZOOM];
    /* Player position within the final 100x100 grid, in 16.16 cells. */
    int playerGridX;
    int playerGridY;
} TerrainZoomPath;

/** Load Midwinter's 50x50 fixed-point terrain seed.
 * @param source Destination for decoded terrain cells.
 * @param baseDirectoryPath RISC OS application path containing the asset.
 * @return Zero on success; non-zero on invalid arguments or file errors.
 */
int TerrainLoad(TerrainSource *source, const char *baseDirectoryPath);

/** Generate a 100x100 terrain grid for the supplied map zoom path.
 * @param source Loaded pristine terrain seed.
 * @param path Ordered map extraction windows.
 * @param completeZoom If non-zero, append centered zooms until the original
 * four-level 3D detail is reached.
 * @param output Destination for the generated 100x100 grid.
 * @return Zero on success; non-zero on generation failure.
 */
int TerrainBuildGrid(const TerrainSource *source, const TerrainZoomPath *path,
                     int completeZoom, HeightCell *output);

/** Interpolate a generated grid to the 200x200 map display resolution.
 * @param grid Source 100x100 terrain grid.
 * @param zoomLevel Current map zoom level.
 * @param display Destination for the interpolated 200x200 cells.
 * @return Zero on success; non-zero on invalid arguments or allocation failure.
 */
int TerrainBuildDisplay(const HeightCell *grid, int zoomLevel,
                        HeightCell *display);

/** Resolve display position (100,100) to Midwinter strategic world units.
 * @param path Zoom path defining the current viewport.
 * @param worldX Optional destination for the horizontal world coordinate.
 * @param worldY Optional destination for the vertical world coordinate.
 */
void TerrainZoomCenter(const TerrainZoomPath *path, int *worldX, int *worldY);

/** Resolve a 200x200 map cursor through all remaining terrain zoom levels.
 * @param path Zoom path to complete and update with the final local position.
 * @param displayX Horizontal cursor coordinate in the map.
 * @param displayY Vertical cursor coordinate in the map.
 */
void TerrainSelectPlayer(TerrainZoomPath *path, int displayX, int displayY);

/** Resolve the selected player position to Midwinter strategic world units.
 * @param path Completed zoom path and local player coordinate.
 * @param worldX Optional destination for the horizontal world coordinate.
 * @param worldY Optional destination for the vertical world coordinate.
 */
void TerrainPlayerWorldPosition(const TerrainZoomPath *path,
                                int *worldX, int *worldY);

/** Interpret a stored height word with Midwinter's signed semantics.
 * @param value Unsigned stored height word.
 * @return The same bit pattern interpreted as a signed height.
 */
short TerrainSignedHeight(unsigned short value);

#endif
