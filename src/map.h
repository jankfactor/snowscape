#ifndef MAP_H
#define MAP_H

#include "terrain.h"

/** Run the 200x200 terrain-map selector.
 * @param source Loaded pristine Midwinter terrain seed.
 * @param view Persistent map zoom path.
 * @param playerSelection Persistent selected player location.
 * @return Zero when F2 accepts the player location, one when
 * Escape cancels, or a negative value if map generation fails.
 */
int RunMapScreen(const TerrainSource *source, TerrainZoomPath *view,
                 TerrainZoomPath *playerSelection);

#endif
