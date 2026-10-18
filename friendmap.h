/*
 * Friend Mode maps: a world map with a dot for each player (toggled with
 * a key) and an optional corner minimap of the ground around the view.
 *
 * Both are drawn from each chunk's ground (its terrain's flats), averaged
 * into a few colors per chunk, so towns show as floors without walls (walls
 * and roofs are objects).  The corner minimap is for the overworld only.
 */
#ifndef FRIENDMAP_H
#define FRIENDMAP_H

#include "tiles.h"

#include <string>
#include <vector>

struct Friend_map_dot {
	Tile_coord  tile;
	int         color;    // Palette index.
	std::string label;    // "Marc (Iolo)", for the world map's key.
};

// The corner minimap option (Game Engine options), off by default.
extern bool Friend_minimap_enabled;

// Show or hide the world map.
void Friend_map_toggle();
bool Friend_map_shown();
// Draw what is on (world map, corner minimap) over the game view.
void Friend_map_paint(const std::vector<Friend_map_dot>& dots);

#endif
