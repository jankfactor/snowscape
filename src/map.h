#ifndef MAP_H
#define MAP_H

#include "terrain.h"

/** Run the 200x200 terrain-map selector.
 * @param source Loaded pristine Midwinter terrain seed.
 * @param selection Destination zoom path and selected player coordinate.
 * @return Zero when Space accepts a middle-clicked player location, one when
 * Escape cancels, or a negative value if map generation fails.
 */
int RunMapScreen(const TerrainSource *source, TerrainZoomPath *selection);

#endif
