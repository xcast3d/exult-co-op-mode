/*
 *  gamerend.h - Rendering methods.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GAMEREND_H
#define GAMEREND_H 1

class Game_object;

/*
 *  A helper-class for rendering.
 */
class Game_render {
	unsigned long render_seq = 0;     // For marking rendered objects.
	int           skip       = 31;    // Set for each render.  We skip
									  //   painting at or above this.
	int bbox_palindex = -1;

public:
	void paint_terrain_only(int start_chunkx, int start_chunky, int stop_chunkx, int stop_chunky);
	// Render the map & objects.
	int paint_map(int x, int y, int w, int h);
	// Paint "flat" scenery in a chunk.
	void paint_chunk_flats(int cx, int cy, int xoff, int yoff);
	void paint_chunk_flat_rles(int cx, int cy, int xoff, int yoff);
	//              // Paint blackness in a dungeon
	// void paint_dungeon_black(int cx, int cy, int xoff, int yoff, int
	// index=0);
	// Paint objects in given chunk at
	//   given lift.
	int paint_chunk_objects(int cx, int cy);
	// Paint an obj. after dependencies.
	void paint_object(Game_object* obj);
	// Render dungeon blackness
	void paint_blackness(int start_chunkx, int start_chunky, int stop_chunkx, int stop_chunky, int index = 0);
	int  get_light_strength(const Game_object* obj, const Game_object* av) const;

	void increment_bbox_index();

	void set_bbox_index(int index) {
		bbox_palindex = index;
	}

	int get_bbox_index() {
		return bbox_palindex;
	}
};

/*
 *  Relight (experimental): a light layer over the original pixels.  While
 *  the map paints, a height is recorded for every pixel from each shape's
 *  3D box; then sun shadows and ambient occlusion are traced through those
 *  heights and the frame is darkened through the current palette, so the
 *  colours stay the game's own.
 */
namespace Relight {
	// Chosen on the Lighting page of the game menu (config/video/lighting).
	struct Settings {
		bool enabled     = false;
		bool soft        = true;    // Soft or hard shadow edges.
		int  strength    = 1;       // Shadows: 0 low, 1 medium, 2 high.
		bool full_colour = true;    // Or stay in the game's 256 colours.
		int  sunlight    = 1;       // Brighten sunlit areas: 0 off, 1 soft, 2 bright.
		int  night_edges = 1;       // Darken the view's edges at night: 0 off, 1 subtle, 2 deeper.
	};

	// Debug views, cycled with Ctrl+Alt+Shift+L (Ctrl+Shift+L turns lighting on/off).
	enum View {
		normal = 0,
		show_height,
		show_light,
		num_views
	};

	const Settings& settings();
	void            set_settings(const Settings& s);    // Also saves them.
	View            view();
	void            cycle_view();
	const char*     view_name();
	bool            active();    // Anything to do?
	bool            fading();    // Shadows fading with fog: repaint each frame.

	// Lighting is left out while one of these is alive (save thumbnails).
	struct Pause {
		Pause();
		~Pause();
		Pause(const Pause&)            = delete;
		Pause& operator=(const Pause&) = delete;
	};
}    // namespace Relight

#endif
