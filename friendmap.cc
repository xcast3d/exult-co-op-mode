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
#include "font.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "palette.h"
#include "shapeid.h"

#include <algorithm>
#include <array>

bool Friend_minimap_enabled = false;
bool Friend_minimap_large   = false;

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

// The map has its own layer, sized by Exult's HUD scale (Game Engine's UI
// options), so it stays the same size on screen whatever the game area's
// resolution; drawn in the game view, a big game area made it tiny.
int  map_layer = -1;
bool map_shown = false;
int  map_bottom = -1;    // Where the minimap ends on screen, while shown.

// The hold / follow button: its own layer, at the HUD's scale like the map.
int         button_layer = -1;
bool        button_shown = false;
std::string button_drawn;               // The label it has.
int         button_rect[4] = {0, 0, 0, 0};    // On screen: x, y, w, h.
constexpr int button_w = 36;            // Game pixels, before the HUD's scale.
constexpr int button_h = 13;

void hide_corner_map() {
	Game_window* gwin = Game_window::get_instance();
	if (map_layer >= 0 && map_shown) {
		gwin->layer_set_visible(map_layer, false);
		gwin->set_painted();
	}
	map_shown  = false;
	map_bottom = -1;
}

void hide_button() {
	Game_window* gwin = Game_window::get_instance();
	if (button_layer >= 0 && button_shown) {
		gwin->layer_set_visible(button_layer, false);
		gwin->set_painted();
	}
	button_shown = false;
}

// Exult's drawn button (as Basic_button's colours) with the label.
void paint_button(const std::string& label) {
	Game_window* gwin = Game_window::get_instance();
	if (button_layer < 0) {
		constexpr int text_layer_z = (1 << 18) + (1 << 16);    // As the map.
		button_layer = gwin->create_layer("Hold button", button_w, button_h, 255, 0, text_layer_z - 2);
		if (button_layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(button_layer, Image_window::UiLayerHudGumps);
		button_drawn.clear();
	}
	if (label != button_drawn) {
		Image_buffer8* buffer = gwin->get_layer_ibuf(button_layer);
		if (!buffer) {
			return;
		}
		constexpr int background = 140;    // Basic_button's colours.
		buffer->clear_clip();
		buffer->fill8(0);    // The black edge.
		buffer->draw_beveled_box(1, 1, button_w - 2, button_h - 2, 1, background, background - 2, background - 2,
								 background + 3, background + 4, background + 1);
		auto font = fontManager.get_font("SMALL_BLACK_FONT");
		if (font) {
			// Fonts paint into the current render target.
			Image_buffer8* prev = gwin->push_render_target(buffer);
			const int      x    = (button_w - font->get_text_width(label.c_str())) / 2;
			const int      y    = (button_h - font->get_text_height()) / 2;
			font->paint_text(buffer, label.c_str(), x, y);
			gwin->pop_render_target(prev);
		}
		gwin->layer_set_dirty(button_layer);
		button_drawn = label;
		gwin->set_painted();
	}
	// Under the minimap, or in the top-right corner without it.
	Image_window* iwin = gwin->get_win();
	const float   f    = iwin->get_ui_scale_factor(Image_window::UiLayerHudGumps);
	int           sx;
	int           sy;
	iwin->game_to_screen(gwin->get_width() - 5, 5, false, sx, sy);
	const int dw  = static_cast<int>(button_w * f);
	const int dh  = static_cast<int>(button_h * f);
	const int top = map_bottom >= 0 ? map_bottom + static_cast<int>(3 * f) : sy;
	if (button_rect[0] != sx - dw || button_rect[1] != top || button_rect[2] != dw || button_rect[3] != dh) {
		button_rect[0] = sx - dw;
		button_rect[1] = top;
		button_rect[2] = dw;
		button_rect[3] = dh;
		gwin->layer_set_dest(button_layer, sx - dw, top, dw, dh);
		gwin->set_painted();
	}
	if (!button_shown) {
		gwin->layer_set_visible(button_layer, true);
		button_shown = true;
		gwin->set_painted();
	}
}

void paint_corner_map(const std::vector<Friend_map_dot>& dots) {
	Game_window* gwin   = Game_window::get_instance();
	Actor*       viewer = gwin->get_camera_actor();
	if (!viewer || gwin->is_in_dungeon()) {
		hide_corner_map();    // Overworld only.
		return;
	}
	constexpr int span = 12 * cells;    // 12 chunks across, a pixel per cell.
	constexpr int size = span + 2;      // With a black edge.
	if (map_layer < 0) {
		constexpr int text_layer_z = (1 << 18) + (1 << 16);    // As effects.cc's text.
		map_layer                  = gwin->create_layer("Corner minimap", size, size, 255, 0, text_layer_z - 2);
		if (map_layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(map_layer, Image_window::UiLayerHudGumps);
	}
	Image_buffer8* buffer = gwin->get_layer_ibuf(map_layer);
	if (!buffer) {
		return;
	}
	// Redrawn only when the view or a dot moves.
	static Tile_coord                  drawn_at(-1, -1, -1);
	static std::vector<Friend_map_dot> drawn_dots;
	const Tile_coord                   at      = viewer->get_tile();
	const bool                         changed = at != drawn_at || drawn_dots.size() != dots.size()
						 || !std::equal(dots.begin(), dots.end(), drawn_dots.begin(),
								 [](const Friend_map_dot& a, const Friend_map_dot& b) {
									 return a.tile == b.tile && a.color == b.color;
								 });
	if (changed || !map_shown) {
		drawn_at   = at;
		drawn_dots = dots;
		Game_map* map  = gwin->get_map();
		const int left = at.tx - span / 2 * cell_size;
		const int top  = at.ty - span / 2 * cell_size;
		buffer->clear_clip();
		buffer->fill8(0);
		for (int y = 0; y < span; ++y) {
			const int ty = ((top + y * cell_size) % c_num_tiles + c_num_tiles) % c_num_tiles;
			for (int x = 0; x < span; ++x) {
				const int tx = ((left + x * cell_size) % c_num_tiles + c_num_tiles) % c_num_tiles;
				const Terrain_colors& colors
						= colors_for(map->get_terrain_num(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk));
				if (colors.ready) {
					const int cell
							= ((ty % c_tiles_per_chunk) / cell_size) * cells + (tx % c_tiles_per_chunk) / cell_size;
					buffer->put_pixel8(colors.cell[cell], 1 + x, 1 + y);
				}
			}
		}
		for (const Friend_map_dot& dot : dots) {
			const int dx = (dot.tile.tx - left) / cell_size;
			const int dy = (dot.tile.ty - top) / cell_size;
			if (dx >= 2 && dy >= 2 && dx < span - 2 && dy < span - 2) {
				paint_dot(buffer, 1 + dx, 1 + dy, dot.color);
			}
		}
		gwin->layer_set_dirty(map_layer);
	}
	// Top right of the game view, 5 game pixels in, at the HUD's scale.
	Image_window* iwin = gwin->get_win();
	const float   f    = iwin->get_ui_scale_factor(Image_window::UiLayerHudGumps);
	int           sx;
	int           sy;
	iwin->game_to_screen(gwin->get_width() - 5, 5, false, sx, sy);
	const int  dw        = static_cast<int>(size * f * (Friend_minimap_large ? 1.5f : 1.0f));
	static int placed[3] = {-1, -1, -1};
	const bool moved     = placed[0] != sx - dw || placed[1] != sy || placed[2] != dw;
	if (moved) {
		placed[0] = sx - dw;
		placed[1] = sy;
		placed[2] = dw;
		gwin->layer_set_dest(map_layer, sx - dw, sy, dw, dw);
	}
	if (!map_shown) {
		gwin->layer_set_visible(map_layer, true);
		map_shown = true;
	}
	map_bottom = sy + dw;
	if (changed || moved) {
		gwin->set_painted();
	}
}
}    // namespace

void Friend_map_hide() {
	hide_corner_map();
	hide_button();
}

void Friend_map_paint(const std::vector<Friend_map_dot>& dots, const Friend_map_button& button) {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin->get_map()) {
		return;
	}
	if (Friend_minimap_enabled && !gwin->is_in_exult_menu()) {
		paint_corner_map(dots);
	} else {
		hide_corner_map();
	}
	if (!button.label.empty() && !gwin->is_in_exult_menu()) {
		paint_button(button.label);
	} else {
		hide_button();
	}
}

int Friend_map_top_right_height() {
	int height = 0;
	if (map_shown) {
		height = 5 + static_cast<int>((12 * cells + 2) * (Friend_minimap_large ? 1.5f : 1.0f));
	}
	if (button_shown) {
		height = (height ? height + 3 : 5) + button_h;
	}
	return height;
}

bool Friend_map_button_at(float sx, float sy) {
	return button_shown && sx >= button_rect[0] && sy >= button_rect[1] && sx < button_rect[0] + button_rect[2]
		   && sy < button_rect[1] + button_rect[3];
}
