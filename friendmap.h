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

#include <string>
#include <vector>

struct Friend_map_dot {
	Tile_coord  tile;
	int         color;    // Palette index.
};

// The corner minimap option (Game Display), off by default, and its size:
// large is half as big again.
extern bool Friend_minimap_enabled;
extern bool Friend_minimap_large;

// The hold / follow button under the minimap (or in the top-right corner
// without it), an alternative to Y.  'label' empty: not shown.
struct Friend_map_button {
	std::string label;    // "Hold", "Follow"...
};

// Draw the corner minimap over the game view, if it is on, and the button.
void Friend_map_paint(const std::vector<Friend_map_dot>& dots, const Friend_map_button& button);
// Hide them now (a full-screen menu is about to show; the next frame puts them back).
void Friend_map_hide();
// Is the screen point (an event's, after SDL_ConvertEventToRenderCoordinates)
// on the button?
bool Friend_map_button_at(float sx, float sy);
// How far down from the top of the view the minimap and the button reach,
// in HUD pixels (0 with neither), for what goes under them (the skill bar).
int Friend_map_top_right_height();

#endif
