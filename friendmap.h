/*
 * Friend Mode's optional corner minimap of the ground around the view, with
 * a dot for each player.
 *
 * It is drawn from each chunk's ground (its terrain's flats), averaged into
 * a few colors per chunk, so towns show as floors without walls (walls and
 * roofs are objects).  It is for the overworld only.  A world map on U is
 * shelved in docs/friend-mode/shelved/world-map.cc.
 */
#ifndef FRIENDMAP_H
#define FRIENDMAP_H

#include "tiles.h"

#include <vector>

struct Friend_map_dot {
	Tile_coord  tile;
	int         color;    // Palette index.
};

// The corner minimap option (Game Engine options), off by default.
extern bool Friend_minimap_enabled;

// Draw the corner minimap over the game view, if it is on.
void Friend_map_paint(const std::vector<Friend_map_dot>& dots);

#endif
