/*
 * Friend Mode's corner minimap with player dots.  (The world map on U is
 * shelved in docs/friend-mode/shelved/world-map.cc.)
 */
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendmap.h"

#include "actors.h"
#include "chunkter.h"
#include "exult_constants.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "palette.h"
#include "shapeid.h"

#include <algorithm>
#include <array>

bool Friend_minimap_enabled = false;

namespace {
// Each chunk's ground as a grid of averaged colors for the corner minimap
// (and one color for the whole chunk, which the shelved world map used).
constexpr int cells     = 4;
constexpr int cell_size = c_tiles_per_chunk / cells;    // Tiles per cell side.

struct Terrain_colors {
	bool                                    ready = false;
	std::array<unsigned char, cells * cells> cell{};
	unsigned char                           whole = 0;
};

std::vector<Terrain_colors> terrain_colors;    // By terrain number.

const Palette& day_palette() {
	static Palette palette;
	static bool    set = false;
	if (!set) {
		palette.set(PALETTE_DAY, 100, false);
		set = true;
	}
	return palette;
}

// Work out a terrain's colors from its rendered flats (as Exult's own
// minimap writer does), once.
const Terrain_colors& colors_for(int tnum) {
	static const Terrain_colors none;
	Game_map*                   map = Game_window::get_instance()->get_map();
	if (tnum < 0 || tnum >= map->get_num_chunk_terrains()) {
		return none;
	}
	if (terrain_colors.size() < static_cast<size_t>(map->get_num_chunk_terrains())) {
		terrain_colors.resize(map->get_num_chunk_terrains());
	}
	Terrain_colors& colors = terrain_colors[tnum];
	if (colors.ready) {
		return colors;
	}
	Chunk_terrain* terrain = Game_map::get_terrain(tnum);
	Image_buffer8* flats   = terrain ? terrain->get_rendered_flats() : nullptr;
	if (!flats) {
		return none;
	}
	const Palette&       pal   = day_palette();
	const unsigned char* bits  = flats->get_bits();
	const int            w     = flats->get_width();
	const int            h     = flats->get_height();
	unsigned long        all[3] = {0, 0, 0};
	for (int cy = 0; cy < cells; ++cy) {
		for (int cx = 0; cx < cells; ++cx) {
			unsigned long sum[3] = {0, 0, 0};
			int           count  = 0;
			for (int y = cy * h / cells; y < (cy + 1) * h / cells; ++y) {
				for (int x = cx * w / cells; x < (cx + 1) * w / cells; ++x) {
					const unsigned char pixel = bits[y * w + x];
					sum[0] += pal.get_red(pixel);
					sum[1] += pal.get_green(pixel);
					sum[2] += pal.get_blue(pixel);
					++count;
				}
			}
			count = std::max(count, 1);
			for (int c = 0; c < 3; ++c) {
				all[c] += sum[c];
			}
			colors.cell[cy * cells + cx] = static_cast<unsigned char>(
					pal.find_color(sum[0] / count, sum[1] / count, sum[2] / count));
		}
	}
	const unsigned long pixels = std::max(1, w * h);
	colors.whole               = static_cast<unsigned char>(pal.find_color(all[0] / pixels, all[1] / pixels, all[2] / pixels));
	colors.ready               = true;
	return colors;
}

// A player's dot: their color with a black edge.
void paint_dot(Image_buffer8* buffer, int x, int y, int color) {
	buffer->fill8(0, 4, 4, x - 2, y - 2);
	buffer->fill8(static_cast<unsigned char>(color), 2, 2, x - 1, y - 1);
}

void paint_corner_map(const std::vector<Friend_map_dot>& dots) {
	Game_window* gwin   = Game_window::get_instance();
	Actor*       viewer = gwin->get_camera_actor();
	if (!viewer || gwin->is_in_dungeon()) {
		return;    // Overworld only.
	}
	Game_map*      map    = gwin->get_map();
	Image_buffer8* buffer = gwin->get_win()->get_ib8();
	constexpr int  span   = 12 * cells;    // 12 chunks across, a pixel per cell.
	const int      x0     = gwin->get_width() - span - 6;
	const int      y0     = 6;
	const Tile_coord at   = viewer->get_tile();
	const int      left   = at.tx - span / 2 * cell_size;
	const int      top    = at.ty - span / 2 * cell_size;
	buffer->fill8(0, span + 2, span + 2, x0 - 1, y0 - 1);
	for (int y = 0; y < span; ++y) {
		const int ty = ((top + y * cell_size) % c_num_tiles + c_num_tiles) % c_num_tiles;
		for (int x = 0; x < span; ++x) {
			const int             tx     = ((left + x * cell_size) % c_num_tiles + c_num_tiles) % c_num_tiles;
			const Terrain_colors& colors = colors_for(map->get_terrain_num(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk));
			if (colors.ready) {
				const int cell = ((ty % c_tiles_per_chunk) / cell_size) * cells + (tx % c_tiles_per_chunk) / cell_size;
				buffer->put_pixel8(colors.cell[cell], x0 + x, y0 + y);
			}
		}
	}
	for (const Friend_map_dot& dot : dots) {
		const int dx = (dot.tile.tx - left) / cell_size;
		const int dy = (dot.tile.ty - top) / cell_size;
		if (dx >= 2 && dy >= 2 && dx < span - 2 && dy < span - 2) {
			paint_dot(buffer, x0 + dx, y0 + dy, dot.color);
		}
	}
}
}    // namespace

void Friend_map_paint(const std::vector<Friend_map_dot>& dots) {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin->get_map()) {
		return;
	}
	if (Friend_minimap_enabled) {
		paint_corner_map(dots);
		gwin->set_painted();
	}
}
