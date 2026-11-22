/*
 *  gamerend.cc - Rendering methods.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2025  The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "gamerend.h"

#include "Gump.h"
#include "Gump_manager.h"
#include "actors.h"
#include "cheat.h"
#include "chunks.h"
#include "combat_opts.h"
#include "drag.h"
#include "effects.h"
#include "font.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "items.h"
#include "ignore_unused_variable_warning.h"
#include "objiter.h"
#include "perf.h"

#include "Configuration.h"
#include "endianio.h"
#include "palette.h"
#include "vgafile.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/*
 *  Relight (experimental).  See gamerend.h.
 *
 *  Heights are in screen pixels: one lift raises a pixel 4 pixels up and
 *  left, so a point at world (wx, wy, z) shows at (wx - z, wy - z).
 */
namespace Relight {
	namespace {
		Settings cur_settings;
		bool     settings_read = false;
		View     cur_view      = normal;

		// Height of each pixel, and which way it faces, laid out like the
		// 8-bit game buffer.
		enum Face : unsigned char {
			face_top = 0,
			face_east,
			face_south
		};

		std::vector<unsigned char> heights, faces;
		// The base (4 x lift) of what each pixel's height belongs to: a
		// flying carpet's is high off the ground, so the ground beside it
		// isn't darkened as if by a wall (ambient occlusion).
		std::vector<unsigned char> bases;
		// 1 where an actor (a person or creature) is: standing figures cast
		// shadows right from their feet (see sweep_shadows).
		std::vector<unsigned char> figures;
		bool                       stamping_figure = false;
		// Indoors, the roof Exult hides: its heights still cast shadows
		// (casters = heights + roofs), and the floor under it is covered.
		std::vector<unsigned char> casters, covered;
		unsigned char*             hbase = nullptr;    // At game (0, 0).
		unsigned char*             fbase = nullptr;
		unsigned char*             bbase = nullptr;
		unsigned char*             abase = nullptr;
		int                        hline = 0;
		int                        hmin_x = 0, hmin_y = 0, hmax_x = 0, hmax_y = 0;

		// Shade tables built from the current palette.
		// Levels 0..15 darken (k / 15); 16..19 brighten in steps of 7.5%.
		constexpr int     num_levels    = 20;
		constexpr int     unlit_level   = 15;
		inline float      level_factor(int k) {
            return k <= unlit_level ? float(k) / unlit_level : 1.0f + 0.075f * (k - unlit_level);
		}
		inline int        level_for(float lum) {
            const int k = lum <= 1.0f ? int(lum * unlit_level + 0.5f) : unlit_level + int((lum - 1.0f) / 0.075f + 0.5f);
            return std::clamp(k, 0, num_levels - 1);
		}
		constexpr int     first_cycling = 0xe0;    // Water etc. stay as is.
		unsigned char     shade[num_levels][256];
		unsigned char     gray[num_levels];
		unsigned char     pal_seen[768];
		bool              pal_valid = false;
		std::vector<unsigned char> shadow;    // Scratch: 1 = in sun shadow.
		std::vector<float>         light;     // Scratch: RGB light per pixel.
		std::vector<float>         lamp_loss, lamp_tmp, lamp_floor;    // Scratch: soft lamp shadows.

		// Full colour: the light layer, and the map as painted, so what is
		// drawn over it afterwards (gumps, text) can be left unlit.
		std::vector<uint32>        overlay;
		std::vector<unsigned char> map_copy;
		int                        ov_x = 0, ov_y = 0, ov_w = 0, ov_h = 0;
		bool                       ov_pending = false;

		constexpr int margin = 128;
		int           view_x0 = 0, view_y0 = 0, view_x1 = 0, view_y1 = 0;    // Painted area.

		void ensure_buffer(Image_window8* win) {
			// The buffer reaches 'margin' pixels past the screen on every side,
			// so things just out of view still cast their shadows into it.
			const int line   = win->get_full_width() + 2 * margin;
			const int rows   = win->get_full_height() + 2 * margin;
			const int sx     = win->get_start_x() - margin;
			const int sy     = win->get_start_y() - margin;
			if (heights.size() != size_t(line) * rows || hline != line || hmin_x != sx || hmin_y != sy) {
				heights.assign(size_t(line) * rows, 0);
				faces.assign(size_t(line) * rows, face_top);
				bases.assign(size_t(line) * rows, 0);
				figures.assign(size_t(line) * rows, 0);
				casters.assign(size_t(line) * rows, 0);
				covered.assign(size_t(line) * rows, 0);
				hline  = line;
				hmin_x = sx;
				hmin_y = sy;
				hbase  = heights.data() - sy * line - sx;
				fbase  = faces.data() - sy * line - sx;
				bbase  = bases.data() - sy * line - sx;
				abase  = figures.data() - sy * line - sx;
				hmax_x = sx + line;
				hmax_y = sy + rows;
			}
		}

		inline bool in_buffer(int x, int y) {
			return x >= hmin_x && y >= hmin_y && x < hmax_x && y < hmax_y;
		}

		inline int idx(int x, int y) {    // Into the vectors.
			return (y - hmin_y) * hline + (x - hmin_x);
		}

		inline int height_at(int x, int y) {
			return in_buffer(x, y) ? hbase[y * hline + x] : 0;
		}

		inline int base_at(int x, int y) {
			return in_buffer(x, y) ? bbase[y * hline + x] : 0;
		}

		inline int caster_at(int x, int y) {
			return in_buffer(x, y) ? casters[idx(x, y)] : 0;
		}

		void rebuild_tables(const Palette* pal) {
			unsigned char cur[768];
			for (int i = 0; i < 256; i++) {
				cur[3 * i]     = pal->get_red(i);
				cur[3 * i + 1] = pal->get_green(i);
				cur[3 * i + 2] = pal->get_blue(i);
			}
			if (pal_valid && std::memcmp(cur, pal_seen, sizeof(cur)) == 0) {
				return;
			}
			std::memcpy(pal_seen, cur, sizeof(cur));
			pal_valid = true;
			auto nearest = [&](float r, float g, float b) {
				int   best      = 0;
				float best_dist = 1e30f;
				for (int j = 0; j < first_cycling; j++) {
					const float dr = cur[3 * j] - r;
					const float dg = cur[3 * j + 1] - g;
					const float db = cur[3 * j + 2] - b;
					// Weighted toward green, as the eye is.
					const float d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
					if (d < best_dist) {
						best_dist = d;
						best      = j;
					}
				}
				return static_cast<unsigned char>(best);
			};
			int maxc = 1;
			for (const unsigned char c : cur) {
				maxc = std::max<int>(maxc, c);
			}
			for (int k = 0; k < num_levels; k++) {
				const float f = level_factor(k);
				for (int i = 0; i < 256; i++) {
					if (i >= first_cycling || k == unlit_level) {
						shade[k][i] = static_cast<unsigned char>(i);
					} else {
						auto c      = [&](int v) {
                            return std::min<float>(maxc, v * f);
						};
						shade[k][i] = nearest(c(cur[3 * i]), c(cur[3 * i + 1]), c(cur[3 * i + 2]));
					}
				}
				const float g = std::min<float>(maxc, maxc * float(k) / (num_levels - 1));
				gray[k]       = nearest(g, g, g);
			}
		}

		// Record heights for a painted shape whose 3D box has its lower
		// south-east corner at screen (hotx, hoty), on lift 'lift', 'zt'
		// lifts tall.  Each pixel gets the highest point of the box along
		// its line of sight: the top, or the east or south face.
		// stamp_outside records only pixels off the painted area, keeping
		// the higher height (there is no painting order to follow there).
		// stamp_roof is for a hidden roof: it raises the casters and marks
		// the floor below it ('floor' high) as covered.
		enum Stamp_mode {
			stamp_paint,
			stamp_outside,
			stamp_roof
		};


		void stamp(Shape_frame* frame, int hotx, int hoty, int lift, int zt, int cx, int cy, int cw, int ch,
				   Stamp_mode mode = stamp_paint, int floor = 0) {
			if (!frame || !frame->get_data()) {
				return;
			}
			const int base = 4 * lift;
			const int top  = 4 * (lift + zt);
			auto      put  = [&](int x, int y) {
                const int east  = base + hotx - x;
                const int south = base + hoty - y;
                int       h     = std::min({top, east, south});
                Face      f     = h == top ? face_top : east <= south ? face_east : face_south;
                if (h <= base) {
                    // Drawn beyond its box's south or east side: a part
                    // that overhangs it (a ship's sail).  Hanging at the
                    // top of it, not lying at its foot, or the lower part
                    // sat in the shadow of the upper (2026-11-21).  Not a
                    // figure: drawn bigger than its tile, its overhang
                    // raised that way cast broken, striped shadows.
                    h = stamping_figure ? base : top;
                    f = face_top;
                }
                // A rim a lift or so deep (the edge of a roof piece) reads
                // as part of the top, or every piece gets an outline.
                if (top - h <= 5) {
                    h = top;
                    f = face_top;
                }
                const auto hv = static_cast<unsigned char>(std::min(h, 255));
                if (mode == stamp_roof) {
                    unsigned char& c = casters[idx(x, y)];
                    c                = std::max(c, hv);
                    const int d      = h - floor;    // Straight down to the floor.
                    if (in_buffer(x + d, y + d)) {
                        covered[idx(x + d, y + d)] = 1;
                    }
                    return;
                }
                if (mode == stamp_outside) {
                    if ((x >= view_x0 && x < view_x1 && y >= view_y0 && y < view_y1) || hbase[y * hline + x] >= hv) {
                        return;
                    }
                }
                hbase[y * hline + x] = hv;
                fbase[y * hline + x] = f;
                bbase[y * hline + x] = static_cast<unsigned char>(std::min(base, 255));
                abase[y * hline + x] = stamping_figure ? 1 : 0;
			};
			auto run = [&](int x, int y, int n) {
				if (y < cy || y >= cy + ch) {
					return;
				}
				const int x0 = std::max(x, cx);
				const int x1 = std::min(x + n, cx + cw);
				for (int px = x0; px < x1; px++) {
					put(px, y);
				}
			};
			if (!frame->is_rle()) {
				for (int y = 0; y < c_tilesize; y++) {
					run(hotx - c_tilesize, hoty - c_tilesize + y, c_tilesize);
				}
				return;
			}
			const unsigned char* in = frame->get_data();
			int                  scanlen;
			while ((scanlen = little_endian::Read2(in)) != 0) {
				const int encoded = scanlen & 1;
				scanlen >>= 1;
				int       scanx = hotx + static_cast<sint16>(little_endian::Read2(in));
				const int scany = hoty + static_cast<sint16>(little_endian::Read2(in));
				if (!encoded) {
					run(scanx, scany, scanlen);
					in += scanlen;
					continue;
				}
				while (scanlen > 0) {
					const int bcnt_raw = *in++;
					const int repeat   = bcnt_raw & 1;
					const int bcnt     = bcnt_raw >> 1;
					run(scanx, scany, bcnt);
					in += repeat ? 1 : bcnt;
					scanx += bcnt;
					scanlen -= bcnt;
				}
			}
		}

		void read_settings() {
			if (settings_read) {
				return;
			}
			settings_read = true;
			std::string val;
			config->value("config/video/lighting/enabled", val, "no");
			cur_settings.enabled = val == "yes";
			config->value("config/video/lighting/shadows", val, "soft");
			cur_settings.soft = val != "hard";
			config->value("config/video/lighting/strength", val, "medium");
			cur_settings.strength = val == "low" ? 0 : val == "high" ? 2 : 1;
			config->value("config/video/lighting/colour", val, "full");
			cur_settings.full_colour = val != "palette";
			config->value("config/video/lighting/sunlight", val, "soft");
			cur_settings.sunlight = val == "off" ? 0 : val == "bright" ? 2 : 1;
			config->value("config/video/lighting/night_edges", val, "subtle");
			cur_settings.night_edges = val == "off" ? 0 : val == "deeper" ? 2 : 1;
		}

		/*
		 *  Splits the lighting's row loops across the CPU's cores; the
		 *  calling thread takes bands too.  Rows are independent, so the
		 *  result is the same as working through them in order.
		 */
		class Row_pool {
		public:
			Row_pool() {
				const int cores = int(std::thread::hardware_concurrency());
				const int n     = std::clamp(cores - 1, 0, 7);
				for (int i = 0; i < n; i++) {
					threads.emplace_back([this] {
						work();
					});
				}
			}

			~Row_pool() {
				{
					const std::lock_guard<std::mutex> lock(mutex);
					stop = true;
				}
				wake.notify_all();
				for (auto& t : threads) {
					t.join();
				}
			}

			Row_pool(const Row_pool&)            = delete;
			Row_pool& operator=(const Row_pool&) = delete;

			bool serial = false;    // For timing: one core only.

			// Call f(begin, end) for bands of [0, n), on every core.
			void run(int n, const std::function<void(int, int)>& f) {
				if (n <= 0) {
					return;
				}
				if (threads.empty() || serial || n < 32) {
					f(0, n);
					return;
				}
				{
					const std::lock_guard<std::mutex> lock(mutex);
					job   = &f;
					total = n;
					band  = std::max(4, n / (int(threads.size() + 1) * 6));
					next  = 0;
					busy  = int(threads.size());
					generation++;
				}
				wake.notify_all();
				take();
				std::unique_lock<std::mutex> lock(mutex);
				done.wait(lock, [this] {
					return busy == 0;
				});
				job = nullptr;
			}

		private:
			std::vector<std::thread>                 threads;
			std::mutex                               mutex;
			std::condition_variable                  wake, done;
			const std::function<void(int, int)>*     job        = nullptr;
			int                                      total      = 0;
			int                                      band       = 1;
			std::atomic<int>                         next{0};
			int                                      busy       = 0;
			unsigned                                 generation = 0;
			bool                                     stop       = false;

			void take() {
				for (;;) {
					const int b = next.fetch_add(band);
					if (b >= total) {
						return;
					}
					(*job)(b, std::min(total, b + band));
				}
			}

			void work() {
				unsigned seen = 0;
				for (;;) {
					{
						std::unique_lock<std::mutex> lock(mutex);
						wake.wait(lock, [&] {
							return stop || generation != seen;
						});
						if (stop) {
							return;
						}
						seen = generation;
					}
					take();
					const std::lock_guard<std::mutex> lock(mutex);
					if (--busy == 0) {
						done.notify_one();
					}
				}
			}
		};

		Row_pool& pool() {
			static Row_pool p;
			return p;
		}

		// QOTC_RELIGHT_BENCH=1 in the environment: print how long lighting
		// takes (to the console), and once, a comparison at three hours.
		bool bench_wanted() {
			static const bool on = [] {
				const char* v = std::getenv("QOTC_RELIGHT_BENCH");
				return v && *v && *v != '0';
			}();
			return on;
		}

		double now_ms() {
			using clk = std::chrono::steady_clock;
			return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
		}

		// Weather (2026-11-22): sunlight through fog is scattered, so it
		// casts no shadows; under an overcast sky (a rain storm too) they
		// are about half as strong.  How hazy it is (0 clear, 1 thick fog)
		// follows the clock's weather, easing over the same four seconds as
		// the weather palette (20 ticks of 200 ms), so the shadows fade as
		// it rolls in.
		constexpr double fog_fade_ms = 4000.0;
		float            fog_now     = 0.0f;
		double           fog_last_ms = -1e9;

		float fog_target(const Game_window* gwin) {
			const Game_clock* clock = gwin->get_clock();
			return clock->is_foggy() ? 1.0f : clock->is_overcast() ? 0.5f : 0.0f;
		}

		void update_fog(const Game_window* gwin) {
			const float  target = fog_target(gwin);
			const double t      = now_ms();
			const double dt     = t - fog_last_ms;
			fog_last_ms         = t;
			if (dt > 1000.0) {
				fog_now = target;    // Just loaded, or lighting just turned on.
			} else if (fog_now < target) {
				fog_now = std::min(target, fog_now + float(dt / fog_fade_ms));
			} else if (fog_now > target) {
				fog_now = std::max(target, fog_now - float(dt / fog_fade_ms));
			}
		}
	}    // namespace

	bool fading() {
		return active() && fog_now != fog_target(Game_window::get_instance());
	}

	const Settings& settings() {
		read_settings();
		return cur_settings;
	}

	void set_settings(const Settings& s) {
		settings_read = true;
		cur_settings  = s;
		config->set("config/video/lighting/enabled", s.enabled ? "yes" : "no", false);
		config->set("config/video/lighting/shadows", s.soft ? "soft" : "hard", false);
		config->set("config/video/lighting/strength", s.strength == 0 ? "low" : s.strength == 2 ? "high" : "medium", false);
		config->set("config/video/lighting/sunlight", s.sunlight == 0 ? "off" : s.sunlight == 2 ? "bright" : "soft", false);
		config->set("config/video/lighting/night_edges", s.night_edges == 0 ? "off" : s.night_edges == 2 ? "deeper" : "subtle", false);
		config->set("config/video/lighting/colour", s.full_colour ? "full" : "palette", true);
		Game_window* gwin = Game_window::get_instance();
		if (!active()) {
			gwin->get_win()->clear_light_overlay();
		}
		gwin->set_all_dirty();
	}

	View view() {
		return cur_view;
	}

	void cycle_view() {
		cur_view = static_cast<View>((cur_view + 1) % num_views);
	}

	const char* view_name() {
		switch (cur_view) {
		case show_height:
			return "Relight: height view";
		case show_light:
			return "Relight: light view";
		default:
			return "Relight: normal view";
		}
	}

	namespace {
		int paused = 0;
	}

	Pause::Pause() {
		paused++;
	}

	Pause::~Pause() {
		paused--;
	}

	bool active() {
		return !paused && (settings().enabled || cur_view != normal);
	}

	void begin_map(Image_window8* win, int x, int y, int w, int h) {
		ensure_buffer(win);
		// The whole frame is painted each time (see Game_window::paint).
		std::fill(heights.begin(), heights.end(), 0);
		std::fill(faces.begin(), faces.end(), face_top);
		std::fill(bases.begin(), bases.end(), 0);
		std::fill(figures.begin(), figures.end(), 0);
		std::fill(covered.begin(), covered.end(), 0);
		view_x0 = x;
		view_y0 = y;
		view_x1 = x + w;
		view_y1 = y + h;
	}

	namespace {
		struct Lamp {
			float x, y, z;    // World pixels (z up).
			float radius;
			float power  = 1.0f;     // Carried torches are a little softer.
			bool  indoor = false;    // Under a roof: lights the room evenly.
			bool  spell  = false;    // The Light spell: pale and soft.
			bool  window = false;    // Daylight through a window, indoors only.
			bool  magic  = false;    // Moongates and magic things: a blue glow.
			bool  worn   = false;    // Magic things the party wears.
			bool  radial = false;    // A plain round glow on screen (x, y), no shadows.
			float top    = 0;        // Radial: the thing's own height (screen px).
		};

		std::vector<Lamp> lamps;

		// Moongates are light, not solid: no shadow, no occlusion.
		bool is_moongate(int shnum) {
			static std::vector<signed char> known;
			if (shnum < 0) {
				return false;
			}
			if (size_t(shnum) >= known.size()) {
				known.resize(shnum + 1, -1);
			}
			if (known[shnum] < 0) {
				std::string name;
				if (shnum < get_num_item_names() && get_item_name(shnum)) {
					name = get_item_name(shnum);
				}
				for (auto& c : name) {
					c = char(std::tolower(static_cast<unsigned char>(c)));
				}
				known[shnum] = name.find("moongate") != std::string::npos;
			}
			return known[shnum] > 0;
		}

		// A round glow centred on an object's picture, like a small torch.
		template <typename L>
		void centre_on_picture(L& l, Game_object* obj, int hx, int hy) {
			if (const Shape_frame* pic = obj->get_shape()) {
				l.x = hx + (pic->get_xright() - pic->get_xleft()) / 2.0f;
				l.y = hy + (pic->get_ybelow() - pic->get_yabove()) / 2.0f;
			} else {
				l.x = hx - 4.0f;
				l.y = hy - 4.0f;
			}
			l.top    = l.z;
			l.z      = 0;
			l.radial = true;
		}

		// Moongates and magic things glow blue; found by name like windows.
		bool is_magic(int shnum) {
			static std::vector<signed char> known;
			if (shnum < 0) {
				return false;
			}
			if (size_t(shnum) >= known.size()) {
				known.resize(shnum + 1, -1);
			}
			if (known[shnum] < 0) {
				std::string name;
				if (shnum < get_num_item_names() && get_item_name(shnum)) {
					name = get_item_name(shnum);
				}
				for (auto& c : name) {
					c = char(std::tolower(static_cast<unsigned char>(c)));
				}
				static const char* const words[]
						= {"magic", "moongate", "orb of the moons", "crystal", "energy field", "wand", "glimmer"};
				known[shnum] = 0;
				for (const char* w : words) {
					// Not spell names like "dispel magic" or "unlock magic".
					if (name.find(w) != std::string::npos && name.find("dispel") == std::string::npos
						&& name.find("unlock") == std::string::npos) {
						known[shnum] = 1;
					}
				}
			}
			return known[shnum] > 0;
		}

		// Windows and open shutters let daylight in; found by name so it
		// works for either game.
		bool is_window(int shnum) {
			static std::vector<signed char> known;
			if (shnum < 0) {
				return false;
			}
			if (size_t(shnum) >= known.size()) {
				known.resize(shnum + 1, -1);
			}
			if (known[shnum] < 0) {
				std::string name;
				if (shnum < get_num_item_names() && get_item_name(shnum)) {
					name = get_item_name(shnum);
				}
				for (auto& c : name) {
					c = char(std::tolower(static_cast<unsigned char>(c)));
				}
				known[shnum] = name.find("closed") == std::string::npos
							   && (name.find("window") != std::string::npos || name.find("shutters") != std::string::npos);
			}
			return known[shnum] > 0;
		}
		std::vector<float>    glow;       // Lamp light to add on top (full colour).
		std::vector<uint32>   glow_px;    // The same, coloured, for the window.
		std::vector<unsigned char> beam;    // Indoor floor the sun reaches.
		std::vector<uint16_t> spill;      // Distance to daylight, indoors.
		std::vector<unsigned char> spill_v;    // How bright that daylight is.
		bool              roofed     = false;    // Hidden roof over the viewer?
		int               roof_floor = 0;        // The viewer's floor (screen px).

		// Call f(chunk) for every chunk under the buffer, plus room for tall
		// things standing below or right of it (they show up and left).
		template <typename F>
		void for_chunks(Game_window* gwin, F&& f) {
			const int tx0 = gwin->get_scrolltx() + (hmin_x / c_tilesize) - 1;
			const int ty0 = gwin->get_scrollty() + (hmin_y / c_tilesize) - 1;
			const int tx1 = gwin->get_scrolltx() + (hmax_x / c_tilesize) + 10;
			const int ty1 = gwin->get_scrollty() + (hmax_y / c_tilesize) + 10;
			const int ncx = (tx1 - tx0) / c_tiles_per_chunk + 2;
			const int ncy = (ty1 - ty0) / c_tiles_per_chunk + 2;
			const int cx0 = ((tx0 + c_num_tiles) % c_num_tiles) / c_tiles_per_chunk;
			const int cy0 = ((ty0 + c_num_tiles) % c_num_tiles) / c_tiles_per_chunk;
			for (int j = 0; j < ncy; j++) {
				for (int i = 0; i < ncx; i++) {
					f(gwin->get_map()->get_chunk((cx0 + i) % c_num_chunks, (cy0 + j) % c_num_chunks));
				}
			}
		}
	}    // namespace

	/*
	 *  Record heights for what stands just out of view (not painted), so
	 *  its shadows reach onto the screen; indoors, the hidden roof; and
	 *  find the lamps.
	 */
	void scan_world(Game_window* gwin) {
		const int skip = gwin->get_render_skip_lift();
		for_chunks(gwin, [&](Map_chunk* chunk) {
			Nonflat_object_iterator next(chunk);
			Game_object*            obj;
			while ((obj = next.get_next()) != nullptr) {
				if (obj->get_lift() >= skip || is_moongate(obj->get_shapenum()) || obj->as_barge()) {
					continue;
				}
				int hx, hy;
				gwin->get_shape_location(obj, hx, hy);
				stamping_figure = obj->as_actor() != nullptr;
				stamp(obj->get_shape(), hx, hy, obj->get_lift(), obj->get_info().get_3d_height(), hmin_x, hmin_y,
					  hmax_x - hmin_x, hmax_y - hmin_y, stamp_outside);
				stamping_figure = false;
			}
		});
		casters = heights;
		roofed  = gwin->is_main_actor_inside();
		if (roofed) {
			const Actor* viewer = gwin->get_camera_actor() ? gwin->get_camera_actor() : gwin->get_main_actor();
			const int    floor  = viewer ? 4 * viewer->get_lift() : 0;
			roof_floor          = floor;
			for_chunks(gwin, [&](Map_chunk* chunk) {
				Nonflat_object_iterator next(chunk);
				Game_object*            obj;
				while ((obj = next.get_next()) != nullptr) {
					if (obj->get_lift() < skip || obj->as_barge()) {
						continue;
					}
					int hx, hy;
					gwin->get_shape_location(obj, hx, hy);
					stamp(obj->get_shape(), hx, hy, obj->get_lift(), obj->get_info().get_3d_height(), hmin_x, hmin_y,
						  hmax_x - hmin_x, hmax_y - hmin_y, stamp_roof, floor);
				}
			});
		}
		lamps.clear();
		const bool dungeon = gwin->is_in_dungeon();
		for_chunks(gwin, [&](Map_chunk* chunk) {
			for (const auto& obj : dungeon ? chunk->get_dungeon_lights() : chunk->get_non_dungeon_lights()) {
				const Shape_info& info  = obj->get_info();
				const int         light = info.get_object_light(obj->get_framenum());
				if (light <= 0 || obj->get_lift() >= skip) {
					continue;
				}
				int hx, hy;
				gwin->get_shape_location(obj, hx, hy);
				const int lift = obj->get_lift();
				const int xt   = info.get_3d_xtiles(obj->get_framenum());
				const int yt   = info.get_3d_ytiles(obj->get_framenum());
				// The middle of its top, in world pixels.
				Lamp l;
				l.z      = 4.0f * (lift + info.get_3d_height());
				l.x      = hx + 4.0f * lift - 4.0f * xt;
				l.y      = hy + 4.0f * lift - 4.0f * yt;
				l.radius = 40.0f + 20.0f * std::min(light, 5);
				// Inside a building (or a dungeon), light fills the room:
				// about 16 tiles, stopped by the walls.
				const int fx = hx - 4 * xt, fy = hy - 4 * yt;
				if (gwin->is_in_dungeon() || (in_buffer(fx, fy) && covered[idx(fx, fy)])) {
					l.radius = 128.0f;
					l.indoor = true;
				}
				l.magic = is_magic(obj->get_shapenum());
				if (l.magic) {
					// A round blue glow on the picture, like magic items,
					// bigger and brighter for a moongate.
					const bool gate = is_moongate(obj->get_shapenum());
					centre_on_picture(l, obj, hx, hy);
					l.indoor = false;
					l.radius = gate ? 72.0f : 48.0f;
					l.power  = gate ? 2.2f : 1.6f;
				}
				lamps.push_back(l);
			}
		});
		// Magic things that aren't light sources (a magic sword on a
		// table, an energy field) give a small blue glow; moongates a big one.
		for_chunks(gwin, [&](Map_chunk* chunk) {
			Nonflat_object_iterator next(chunk);
			Game_object*            obj;
			while ((obj = next.get_next()) != nullptr) {
				if (obj->get_lift() >= skip || !is_magic(obj->get_shapenum()) || obj->as_actor()) {
					continue;
				}
				const Shape_info& info = obj->get_info();
				const int         fr   = obj->get_framenum();
				if (info.get_object_light(fr) > 0) {
					continue;    // Already in the list above.
				}
				int hx, hy;
				gwin->get_shape_location(obj, hx, hy);
				const int lift = obj->get_lift();
				const int xt = info.get_3d_xtiles(fr), yt = info.get_3d_ytiles(fr);
				Lamp      l;
				// The glow hangs a little above the thing, so it lights the
				// floor around it.
				l.z      = 4.0f * (lift + std::max(info.get_3d_height(), 1)) + 10.0f;
				l.x      = hx + 4.0f * lift - 4.0f * xt;
				l.y      = hy + 4.0f * lift - 4.0f * yt;
				l.magic  = true;
				const bool gate = is_moongate(obj->get_shapenum());
				l.radius = gate ? 72.0f : 36.0f;
				l.power  = gate ? 2.2f : 2.4f;    // Small but bright.
				// A plain round glow around the middle of the picture, like
				// a little blue torch.
				centre_on_picture(l, obj, hx, hy);
				lamps.push_back(l);
			}
		});
		// Torches and other lights the party carries.
		Actor*    party[9];
		const int cnt = gwin->get_party(party, 1);
		for (int i = 0; i < cnt; i++) {
			Actor* act = party[i];
			if (!act || act->get_light_source() <= 0 || act->get_lift() >= skip) {
				continue;
			}
			int hx, hy;
			gwin->get_shape_location(act, hx, hy);
			const Shape_info& info = act->get_info();
			const int         lift = act->get_lift();
			// Held about head high, at the top of the figure.
			Lamp l;
			l.z      = 4.0f * (lift + std::max(info.get_3d_height(), 4));
			l.x      = hx + 4.0f * lift - 2.0f;
			l.y      = hy + 4.0f * lift - 2.0f;
			l.radius = 70.0f + 15.0f * std::min(act->get_light_source(), 4);
			l.power  = 0.8f;
			if (gwin->is_in_dungeon() || (in_buffer(hx, hy) && covered[idx(hx, hy)])) {
				l.radius = 128.0f;
				l.indoor = true;
			}
			lamps.push_back(l);
		}
		// Daylight through windows and open shutters, falling into the room
		// (add_lamps keeps it to the floor under the roof).
		if (roofed && !dungeon) {
			for_chunks(gwin, [&](Map_chunk* chunk) {
				Nonflat_object_iterator next(chunk);
				Game_object*            obj;
				while ((obj = next.get_next()) != nullptr) {
					if (obj->get_lift() >= skip || !is_window(obj->get_shapenum())) {
						continue;
					}
					const Shape_info& info = obj->get_info();
					const int         fr   = obj->get_framenum();
					const int         lift = obj->get_lift();
					const int         xt = info.get_3d_xtiles(fr), yt = info.get_3d_ytiles(fr);
					int               hx, hy;
					gwin->get_shape_location(obj, hx, hy);
					const float cx = hx + 4.0f * lift - 4.0f * xt, cy = hy + 4.0f * lift - 4.0f * yt;
					// The room is on one side of the wall; try both.
					const bool along_x = xt >= yt;
					for (const int side : {-1, 1}) {
						const float px = along_x ? cx : cx + 10.0f * side;
						const float py = along_x ? cy + 10.0f * side : cy;
						const int   fx = int(px) - 4 * lift, fy = int(py) - 4 * lift;
						if (!in_buffer(fx, fy) || !covered[idx(fx, fy)]) {
							continue;
						}
						Lamp l;
						l.x      = px;
						l.y      = py;
						l.z      = 4.0f * lift + 2.0f * info.get_3d_height();
						l.radius = 64.0f;
						l.power  = 0.8f;
						l.indoor = true;
						l.window = true;
						lamps.push_back(l);
						break;
					}
				}
			});
		}
		// Magic things the party wears give a faint blue glow: each about a
		// fifth of a torch.
		for (int i = 0; i < cnt; i++) {
			Actor* act = party[i];
			if (!act || act->get_lift() >= skip) {
				continue;
			}
			int worn = 0;
			for (int spot = 0; spot < 18; spot++) {
				const Game_object* it = act->get_readied(spot);
				if (it && is_magic(it->get_shapenum()) && it->get_info().get_object_light(it->get_framenum()) <= 0) {
					worn++;
				}
			}
			if (!worn) {
				continue;
			}
			int hx, hy;
			gwin->get_shape_location(act, hx, hy);
			const int lift = act->get_lift();
			Lamp      l;
			l.z      = 4.0f * (lift + std::max(act->get_info().get_3d_height(), 4));
			l.x      = hx + 4.0f * lift - 2.0f;
			l.y      = hy + 4.0f * lift - 2.0f;
			if (const Shape_frame* pic = act->get_shape()) {
				l.x = hx + (pic->get_xright() - pic->get_xleft()) / 2.0f;
				l.y = hy + (pic->get_ybelow() - pic->get_yabove()) / 2.0f;
			} else {
				l.x = hx - 4.0f;
				l.y = hy - 8.0f;
			}
			l.z      = 0;
			l.radial = true;
			l.radius = 60.0f;
			l.power  = 0.9f * std::min(1.0f, 0.2f * worn);
			l.magic  = true;
			l.worn   = true;
			if (gwin->is_in_dungeon() || (in_buffer(hx, hy) && covered[idx(hx, hy)])) {
				l.indoor = true;
			}
			lamps.push_back(l);
		}
		// The Light spell: a wide, pale glow over whoever the view follows.
		const Actor* viewer = gwin->get_camera_actor() ? gwin->get_camera_actor() : gwin->get_main_actor();
		if (gwin->is_special_light() && viewer && viewer->get_lift() < skip) {
			int hx, hy;
			gwin->get_shape_location(viewer, hx, hy);
			const int lift = viewer->get_lift();
			Lamp      l;
			l.z      = 4.0f * (lift + 10);
			l.x      = hx + 4.0f * lift - 2.0f;
			l.y      = hy + 4.0f * lift - 2.0f;
			l.radius = 200.0f;
			l.power  = 0.7f;
			l.indoor = true;    // Even spread.
			l.spell  = true;
			lamps.push_back(l);
		}
	}

	void stamp_object(Game_object* obj) {
		if (is_moongate(obj->get_shapenum())) {
			return;    // Light, not a solid thing: no shadow.
		}
		if (obj->as_barge()) {
			return;    // A vehicle's frame paints nothing (its parts do): no shadow.
		}
		Game_window* gwin = Game_window::get_instance();
		int          hx, hy;
		gwin->get_shape_location(obj, hx, hy);
		const Shape_info& info = obj->get_info();
		int               cx, cy, cw, ch;
		gwin->get_win()->get_ib8()->get_clip(cx, cy, cw, ch);
		stamping_figure = obj->as_actor() != nullptr;
		stamp(obj->get_shape(), hx, hy, obj->get_lift(), info.get_3d_height(), cx, cy, cw, ch);
		stamping_figure = false;
	}

	/*
	 *  Warm pools of light around lamps, torches and fires: fading with
	 *  distance, brighter on surfaces that face them, blocked by walls.
	 *  Full strength at night, in dungeons and under roofs.
	 */
	void add_lamps(Game_window* gwin, int x, int y, int w, int h, float sun_up, bool sunlit, bool tint, std::vector<float>& dest) {
		if (lamps.empty()) {
			return;
		}
		const float day      = sunlit ? std::clamp(sun_up * 4.0f, 0.0f, 1.0f) : 0.0f;
		const float night    = gwin->is_in_dungeon() ? 1.0f : 1.0f - day;
		const float warm_r   = 1.0f, warm_g = tint ? 0.72f : 1.0f, warm_b = tint ? 0.42f : 1.0f;
		const float strength = 0.9f;
		// The lamps that show, with the pixels each can reach and its colour.
		struct Lit {
			const Lamp* l;
			int         x0, x1, y0, y1;
			float       lr, lg, lb;
		};
		std::vector<Lit> lit;
		for (const Lamp& l : lamps) {
			const float r = l.radius;
			// A light under a roof that is shown (you are outside) is hidden
			// from view: its light stays inside, off the roof tiles.
			{
				const int sx = int(l.x - l.z), sy = int(l.y - l.z);
				const float own = l.radial ? l.top : l.z;
				if (!l.worn && in_buffer(sx, sy) && height_at(sx, sy) > own + 12.0f) {
					continue;
				}
			}
			// Where the lamp shows on screen, and the pixels it can reach.
			const int lx = int(l.x - l.z), ly = int(l.y - l.z);
			Lit       e;
			e.l  = &l;
			e.x0 = std::max(x, lx - int(r) - 64);
			e.x1 = std::min(x + w, int(l.x + r) + 2);
			e.y0 = std::max(y, ly - int(r) - 64);
			e.y1 = std::min(y + h, int(l.y + r) + 2);
			if (e.x0 >= e.x1 || e.y0 >= e.y1) {
				continue;
			}
			e.lr = l.spell ? 0.85f : warm_r;
			e.lg = l.spell ? 0.92f : warm_g;
			e.lb = l.spell ? 1.0f : warm_b;
			if (l.magic && tint) {    // Cool blue.
				e.lr = 0.45f;
				e.lg = 0.7f;
				e.lb = 1.3f;
			}
			if (l.window) {    // Soft daylight.
				e.lr = 1.05f;
				e.lg = tint ? 0.98f : 1.0f;
				e.lb = tint ? 0.88f : 1.0f;
			}
			lit.push_back(e);
		}
		if (lit.empty()) {
			return;
		}
		// Smooth shadows: what each lamp would have lit where it is blocked
		// goes into 'lamp_loss' as well as 'dest'; blurred (as the sun's
		// shadows are), it is taken off again, so lamp shadows have soft
		// edges too (2026-11-21: they were hard and stepped, from the
		// walk toward the lamp).  'lamp_floor' keeps what was there before.
		const bool soft = settings().soft;
		if (soft) {
			lamp_loss.assign(size_t(w) * h * 3, 0.0f);
			lamp_floor.assign(dest.begin(), dest.begin() + size_t(w) * h * 3);
		}
		// Row by row, every lamp on that row in turn, so each core has rows
		// of its own and the lamps add up in the same order as before.
		pool().run(h, [&](int r0, int r1) {
			for (int gy = y + r0; gy < y + r1; gy++) {
				for (const Lit& e : lit) {
					if (gy < e.y0 || gy >= e.y1) {
						continue;
					}
					const Lamp& l = *e.l;
					const float r = l.radius;
					for (int gx = e.x0; gx < e.x1; gx++) {
						const int  i     = idx(gx, gy);
						const bool under = covered[i] != 0;
						// Indoors, lamps are always lit, but by day only half as
						// bright, so daylight through doors and windows leads.
						float amt = l.window ? (under ? day : 0.0f) : under ? std::max(night, 0.5f) : night;
						if (l.magic && !l.worn) {
							amt = std::max(amt, 0.4f);    // Magic glows faintly even by day.
						}
						if (amt <= 0.01f) {
							continue;
						}
						if (l.radial) {
							const float rx = gx - l.x, ry = gy - l.y;
							const float rd = std::sqrt(rx * rx + ry * ry);
							if (rd >= r) {
								continue;
							}
							const float u = 1.0f - rd / r;
							const float k = amt * strength * l.power * u * u;
							float*      o = &dest[(size_t(gy - y) * w + (gx - x)) * 3];
							o[0] += k * e.lr;
							o[1] += k * e.lg;
							o[2] += k * e.lb;
							continue;
						}
						const float hp = hbase[gy * hline + gx];
						// This pixel's point in the world, and the way to the lamp.
						const float wx = gx + hp, wy = gy + hp;
						const float dx = l.x - wx, dy = l.y - wy, dz = l.z - hp;
						const float d  = std::sqrt(dx * dx + dy * dy + dz * dz);
						if (d >= r) {
							continue;
						}
						// Indoors the light spreads evenly, then fades near its reach.
						const float u    = 1.0f - d / r;
						const float fall = l.indoor ? std::min(1.0f, u * 1.6f) * 0.75f : u * u;
						const float most = amt * strength * l.power * fall;
						if (most < 0.004f) {
							continue;    // Too faint to show: skip the walk.
						}
						// Blocked?  Walk toward the lamp through the heights.
						// Only what is shown blocks lamps: the hidden roof (or a
						// dungeon's ceiling) would shut every light in.  At most
						// 16 steps, so towns full of lamps stay quick.  Low
						// clutter (grass, reeds, rubble) doesn't count, and things
						// standing above the light (tree tops, roofs) take a
						// gentler share unblocked, so the pool has no hard rim.
						const bool  above   = hp > l.z + 4.0f;
						const float step    = std::max(3.0f, d / 16.0f);
						bool        blocked = false;
						for (float t = 4.0f; !above && t < d - 6.0f; t += step) {
							const float qx = wx + dx * t / d, qy = wy + dy * t / d, qz = hp + dz * t / d;
							if (height_at(int(qx - qz), int(qy - qz)) > qz + 9.0f) {
								blocked = true;
								break;
							}
						}
						if (blocked && !soft) {
							continue;
						}
						float hit;
						switch (fbase[gy * hline + gx]) {
						case face_east:
							hit = dx / std::max(d, 1.0f);
							break;
						case face_south:
							hit = dy / std::max(d, 1.0f);
							break;
						default:
							hit = dz / std::max(d, 1.0f);
							break;
						}
						const float k   = most * (above ? 0.45f : l.magic ? 1.0f : 0.4f + 0.6f * std::max(0.0f, hit));
						float*      out = &dest[(size_t(gy - y) * w + (gx - x)) * 3];
						out[0] += k * e.lr;
						out[1] += k * e.lg;
						out[2] += k * e.lb;
						if (blocked) {
							float* lost = &lamp_loss[(size_t(gy - y) * w + (gx - x)) * 3];
							lost[0] += k * e.lr;
							lost[1] += k * e.lg;
							lost[2] += k * e.lb;
						}
					}
				}
			}
		});
		if (!soft) {
			return;
		}
		// The same 5 x 5 box as the sun's shadows, in two passes.
		constexpr int bb = 2;
		lamp_tmp.assign(lamp_loss.size(), 0.0f);
		pool().run(h, [&](int r0, int r1) {
			for (int py = r0; py < r1; py++) {
				for (int px = 0; px < w; px++) {
					float sum[3] = {0, 0, 0};
					for (int ox = -bb; ox <= bb; ox++) {
						const float* q = &lamp_loss[(size_t(py) * w + std::clamp(px + ox, 0, w - 1)) * 3];
						sum[0] += q[0];
						sum[1] += q[1];
						sum[2] += q[2];
					}
					float* t = &lamp_tmp[(size_t(py) * w + px) * 3];
					t[0]     = sum[0];
					t[1]     = sum[1];
					t[2]     = sum[2];
				}
			}
		});
		const float n = float((2 * bb + 1) * (2 * bb + 1));
		pool().run(h, [&](int r0, int r1) {
			for (int py = r0; py < r1; py++) {
				for (int px = 0; px < w; px++) {
					float sum[3] = {0, 0, 0};
					for (int oy = -bb; oy <= bb; oy++) {
						const float* q = &lamp_tmp[(size_t(std::clamp(py + oy, 0, h - 1)) * w + px) * 3];
						sum[0] += q[0];
						sum[1] += q[1];
						sum[2] += q[2];
					}
					const size_t i = (size_t(py) * w + px) * 3;
					for (int c = 0; c < 3; c++) {
						dest[i + c] = std::max(lamp_floor[i + c], dest[i + c] - sum[c] / n);
					}
				}
			}
		});
	}

	/*
	 *  Is the room the viewer stands in open to the outside: a window or
	 *  open shutter in this building, or floor leading from the viewer out
	 *  past the roof's edge (an open door)?
	 */
	bool room_is_open(Game_window* gwin, int x, int y, int w, int h) {
		for (const Lamp& l : lamps) {
			if (l.window) {
				return true;
			}
		}
		const Actor* viewer = gwin->get_camera_actor() ? gwin->get_camera_actor() : gwin->get_main_actor();
		if (!viewer) {
			return false;
		}
		int hx, hy;
		gwin->get_shape_location(viewer, hx, hy);
		auto floor_ok = [&](int gx, int gy) {
			return gx >= x && gy >= y && gx < x + w && gy < y + h && hbase[gy * hline + gx] <= roof_floor + 8;
		};
		// Start on the floor at the viewer's feet (the figure itself is tall).
		int sx = -1, sy = -1;
		for (int r = 0; r <= 12 && sx < 0; r++) {
			for (int dy = -r; dy <= r && sx < 0; dy++) {
				for (int dx = -r; dx <= r; dx++) {
					const int gx = hx - 4 + dx, gy = hy - 4 + dy;
					if (floor_ok(gx, gy) && covered[idx(gx, gy)]) {
						sx = gx;
						sy = gy;
						break;
					}
				}
			}
		}
		if (sx < 0) {
			return false;
		}
		std::vector<unsigned char> seen(size_t(w) * h, 0);
		std::vector<int>           todo;
		todo.push_back((sy - y) * w + (sx - x));
		seen[todo.back()] = 1;
		while (!todo.empty()) {
			const int i = todo.back();
			todo.pop_back();
			const int px = i % w, py = i / w;
			static const int steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			for (const auto& st : steps) {
				const int qx = px + st[0], qy = py + st[1];
				if (qx < 0 || qy < 0 || qx >= w || qy >= h || seen[qy * w + qx]) {
					continue;
				}
				const int gx = x + qx, gy = y + qy;
				if (!floor_ok(gx, gy)) {
					continue;
				}
				if (!covered[idx(gx, gy)]) {
					return true;    // Walked out under the open sky.
				}
				seen[qy * w + qx] = 1;
				todo.push_back(qy * w + qx);
			}
		}
		return false;
	}

	namespace {
		// What the light was last worked out from.  When all of it is the
		// same again (only a menu, the cursor or an animation changed) the
		// light is reused instead of traced again.
		struct Light_key {
			int      x = 0, y = 0, w = 0, h = 0, hx = 0, hy = 0, hl = 0;
			int      hour = -1, minute = -1;
			int      fog  = 0;    // fog_now in 1/64ths.
			bool     dungeon = false, special = false, roofed = false;
			int      roof_floor = 0;
			Settings set;
			View     view = normal;
			int      gx = 0, gy = 0, gw = 0, gh = 0;
		};

		bool same_key(const Light_key& a, const Light_key& b) {
			return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.hx == b.hx && a.hy == b.hy && a.hl == b.hl
				   && a.hour == b.hour && a.minute == b.minute && a.fog == b.fog && a.dungeon == b.dungeon && a.special == b.special
				   && a.roofed == b.roofed && a.roof_floor == b.roof_floor && a.set.enabled == b.set.enabled
				   && a.set.soft == b.set.soft && a.set.strength == b.set.strength
				   && a.set.full_colour == b.set.full_colour && a.set.sunlight == b.set.sunlight
				   && a.set.night_edges == b.set.night_edges && a.view == b.view && a.gx == b.gx && a.gy == b.gy
				   && a.gw == b.gw && a.gh == b.gh;
		}

		bool same_lamps(const std::vector<Lamp>& a, const std::vector<Lamp>& b) {
			if (a.size() != b.size()) {
				return false;
			}
			for (size_t i = 0; i < a.size(); i++) {
				const Lamp& p = a[i];
				const Lamp& q = b[i];
				if (p.x != q.x || p.y != q.y || p.z != q.z || p.radius != q.radius || p.power != q.power
					|| p.indoor != q.indoor || p.spell != q.spell || p.window != q.window || p.magic != q.magic
					|| p.worn != q.worn || p.radial != q.radial || p.top != q.top) {
					return false;
				}
			}
			return true;
		}

		Light_key                  last_key;
		bool                       last_valid = false;
		std::vector<unsigned char> last_heights, last_faces, last_bases, last_figures, last_casters, last_covered;
		std::vector<Lamp>          last_lamps;
		std::vector<uint32>        out_overlay, out_glow;    // As last sent to the window.

		// Timing (QOTC_RELIGHT_BENCH).
		bool   use_march   = false;    // The old per-pixel sun walk, to compare.
		int    bench_calls = 0, bench_frames = 0, bench_reused = 0;
		double bench_ms = 0, bench_max = 0;
		bool   bench_done = false;
	}    // namespace

	/*
	 *  Sun shadows: a pixel is in shadow when something on the way toward
	 *  the sun stands above the sun's ray from it (within 240 steps).
	 *  Walking that way from every pixel is slow when the sun is low, as
	 *  the ray climbs so slowly it seldom clears everything.  Instead each
	 *  line of pixels toward the sun is swept once, keeping the highest
	 *  thing within reach (a sliding maximum), the same work at any hour.
	 *  Sets shadow[] (w x h, the view) to 1 where shaded.
	 */
	void sweep_shadows(int x, int y, int w, int h, float dirx, float diry, float slope, float dead_zone) {
		const int   bw      = hline;
		const int   bh      = hmax_y - hmin_y;
		const bool  along_x = std::abs(dirx) >= std::abs(diry);
		const float dmaj    = along_x ? dirx : diry;
		const float dmin    = along_x ? diry : dirx;
		if (std::abs(dmaj) < 1e-4f) {
			return;
		}
		const int   nc    = along_x ? bw : bh;    // Pixels along a line.
		const int   nr    = along_x ? bh : bw;
		const float fs    = dmin / dmaj;           // Sideways drift per pixel.
		const int   dir   = dmaj > 0 ? 1 : -1;
		const float drop  = slope / std::abs(dmaj);    // The ray's climb per pixel.
		const int   first = std::max(1, int(std::ceil(1.5f * std::abs(dmaj))));
		const float figure_lift = std::max(0.0f, dead_zone - 1.0f);
		const int   last  = std::max(first, int(std::ceil(240.0f * std::abs(dmaj))) - 1);
		std::vector<int> drift(nc);
		for (int c = 0; c < nc; c++) {
			drift[c] = int(std::floor(c * fs + 0.5f));
		}
		const int lo = -std::max(drift.front(), drift.back());
		const int hi = nr - 1 - std::min(drift.front(), drift.back());
		pool().run(hi - lo + 1, [&](int b0, int b1) {
			std::vector<int>   cell;
			std::vector<float> g;
			std::vector<int>   dq;
			for (int ln = lo + b0; ln < lo + b1; ln++) {
				cell.clear();
				g.clear();
				// This line's pixels, in order toward the sun.
				for (int k = 0; k < nc; k++) {
					const int c = dir > 0 ? k : nc - 1 - k;
					const int r = ln + drift[c];
					if (r < 0 || r >= nr) {
						if (!cell.empty()) {
							break;
						}
						continue;
					}
					const int i = along_x ? r * bw + c : c * bw + r;
					// A figure's pixels skip most of the dead zone, or the
					// shadow starts a step away from its feet.
					g.push_back(casters[i] + (figures[i] ? figure_lift : 0.0f) - float(cell.size()) * drop);
					cell.push_back(i);
				}
				const int n = int(cell.size());
				dq.resize(n);
				int head = 0, tail = 0;
				// From the sun end back: pixel j sees j + first .. j + last.
				for (int j = n - 1; j >= 0; j--) {
					const int add = j + first;
					if (add < n) {
						while (tail > head && g[dq[tail - 1]] <= g[add]) {
							tail--;
						}
						dq[tail++] = add;
					}
					while (tail > head && dq[head] > j + last) {
						head++;
					}
					if (tail == head) {
						continue;
					}
					const int i  = cell[j];
					const int gx = i % bw + hmin_x, gy = i / bw + hmin_y;
					if (gx < x || gy < y || gx >= x + w || gy >= y + h) {
						continue;
					}
					// On a figure, the full dead zone (it doesn't shade itself).
					if (g[dq[head]] + j * drop > heights[i] + dead_zone + (figures[i] ? figure_lift : 0.0f)) {
						shadow[size_t(gy - y) * w + (gx - x)] = 1;
					}
				}
			}
		});
	}

	// The old way, kept to compare against (QOTC_RELIGHT_BENCH only).
	void march_shadows(int x, int y, int w, int h, float dirx, float diry, float slope, float dead_zone, int maxh) {
		pool().run(h, [&](int r0, int r1) {
			for (int py = r0; py < r1; py++) {
				for (int px = 0; px < w; px++) {
					const int hp = hbase[(y + py) * hline + x + px];
					for (float st = 1.5f; st < 240.0f; st += (st < 12.0f ? 1.0f : 2.0f)) {
						const float rz = hp + st * slope;
						if (rz > maxh) {
							break;
						}
						const int sx = x + px + int(std::lround(st * dirx));
						const int sy = y + py + int(std::lround(st * diry));
						if (caster_at(sx, sy) > rz + dead_zone) {
							shadow[py * w + px] = 1;
							break;
						}
					}
				}
			}
		});
	}

	/*
	 *  Work out the light for the rectangle (game coordinates) at the given
	 *  hour, into light[] (and glow[] in full colour).
	 */
	void compute_light(Game_window* gwin, int x, int y, int w, int h, float hour) {
		const Settings& set = settings();
		// Sun: rises in the east (+x), swings round through the north (-y)
		// at noon, sets in the west, so noon shadows fall toward the viewer.
		// It stays fairly low (at most about 50 degrees) to keep shadows long.
		const float       a       = float(M_PI) * (hour - 6.0f) / 12.0f;
		const float       sun_up  = std::sin(a);
		// Indoors the sun still lights the outside; the floor under the
		// hidden roof counts as shade (see scan_world).
		const bool        outdoor = !gwin->is_in_dungeon();
		const bool        sunlit  = outdoor && sun_up > 0.05f;
		static const float strengths[3] = {0.18f, 0.3f, 0.45f};    // Low, medium, high.
		// Shadows fade in after dawn and out before dusk.
		// Sunlight: brightens what the sun reaches (not shadows, not
		// indoors), fading in after dawn like the shadows.
		static const float boosts[3] = {0.0f, 0.12f, 0.25f};
		// In fog the sun's direction is lost: shadows, sunlight, the sun's
		// warmth and the faces it lights fade out (clear = 1, thick fog = 0);
		// overcast, halfway.
		const float        fog   = fog_now * fog_now * (3.0f - 2.0f * fog_now);
		const float        clear = 1.0f - fog;
		const float        sun_boost
				= 1.0f + (sunlit ? clear * boosts[set.sunlight] * std::min(1.0f, sun_up * 4.0f) : 0.0f);
		const float shadow_amt = sunlit ? clear * strengths[set.strength] * std::min(1.0f, sun_up * 4.0f) : 0.0f;
		// Under a roof by day the room is dimmer than outside, enough that
		// light through a door or window shows, whatever the strength.
		float indoor_dim = sunlit ? (0.2f + 0.4f * strengths[set.strength]) * std::min(1.0f, sun_up * 4.0f) : 0.0f;
		// Some contact shade stays in fog, so things still stand on the ground.
		const float ao_amt     = (0.15f + 0.5f * strengths[set.strength]) * (1.0f - 0.5f * fog);
		const float elev       = 0.10f + 0.78f * sun_up;    // Radians.
		const float slope      = std::tan(elev);
		const float ux = std::cos(a), uy = -std::sin(a);
		const float un   = std::sqrt(ux * ux + uy * uy);
		const float dirx = ux / un - slope, diry = uy / un - slope;
		// How squarely the sun hits each face (top, east, south).
		const float sn       = std::sqrt(1.0f + slope * slope);
		const float hit_top  = slope / sn;
		const float hit_east = std::max(0.0f, ux / un / sn);
		const float hit_south = std::max(0.0f, uy / un / sn);
		auto        facing   = [&](int f) {
            const float hit = f == face_east ? hit_east : f == face_south ? hit_south : hit_top;
            return 1.0f + clear * (std::clamp(1.0f + 0.6f * (hit - hit_top), 0.75f, 1.3f) - 1.0f);
		};
		// Warm sun (golden when low), cool sky in the shade.
		const float t     = std::clamp(sun_up * 2.0f, 0.0f, 1.0f);
		auto        haze  = [clear](float v) {    // Toward white light in fog.
            return 1.0f + clear * (v - 1.0f);
		};
		const float sun_r = haze(1.12f + (1.03f - 1.12f) * t);
		const float sun_g = haze(0.92f + (1.00f - 0.92f) * t);
		const float sun_b = haze(0.72f + (0.95f - 0.72f) * t);
		const float sky_r = haze(0.92f), sky_g = haze(0.97f), sky_b = haze(1.10f);

		shadow.assign(size_t(w) * h, 0);
		const int   blur           = set.soft ? 2 : 0;
		const float step_dead_zone = 5.0f;    // Just over a lift.
		if (sunlit) {
			if (use_march) {
				march_shadows(x, y, w, h, dirx, diry, slope, step_dead_zone,
							  *std::max_element(casters.begin(), casters.end()));
			} else {
				sweep_shadows(x, y, w, h, dirx, diry, slope, step_dead_zone);
			}
			// The floor under the hidden roof is in the shade.
			pool().run(h, [&](int r0, int r1) {
				for (int py = r0; py < r1; py++) {
					for (int px = 0; px < w; px++) {
						if (covered[idx(x + px, y + py)]) {
							shadow[size_t(py) * w + px] = 1;
						}
					}
				}
			});
		}
		// By day, a room open to the outside (an open window or shutter, or
		// an open doorway you can walk out of from where you stand) is lit
		// almost to full brightness, however big it is; only a shut room
		// stays dim.  The sunbeams and doorway light still show on top.
		if (indoor_dim > 0 && room_is_open(gwin, x, y, w, h)) {
			indoor_dim = 0.06f * std::min(1.0f, sun_up * 4.0f);
		}
		// Ambient occlusion: how much higher the surroundings are.
		static const int dirs[8][2] = {
				{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
		light.assign(size_t(w) * h * 3, 1.0f);
		const bool tint = set.full_colour && cur_view == normal;
		pool().run(h, [&](int r0, int r1) {
			for (int py = r0; py < r1; py++) {
				for (int px = 0; px < w; px++) {
					const int gx  = x + px, gy = y + py;
					const int hp  = hbase[gy * hline + gx];
					float     occ = 0;
					for (const int r : {3, 7}) {
						for (const auto& d : dirs) {
							const int qx = gx + d[0] * r, qy = gy + d[1] * r;
							const int dh = height_at(qx, qy) - hp;
							// Ignore steps of a lift or so, such as the rows of
							// a sloped roof; only walls and the like count, and
							// only rising from about here or below, not
							// something in the air (the flying carpet).
							if (dh > step_dead_zone && base_at(qx, qy) <= hp + step_dead_zone) {
								occ += std::min(1.0f, (dh - step_dead_zone) / (1.5f * r));
							}
						}
					}
					const float ao = 1.0f - ao_amt * occ / 16.0f;
					float       lr = ao, lg = ao, lb = ao;
					if (sunlit) {
						float sh = 0;    // Shadow fraction, softened for Soft.
						if (shadow_amt > 0) {
							int sum = 0;
							for (int oy = -blur; oy <= blur; oy++) {
								for (int ox = -blur; ox <= blur; ox++) {
									const int qx = std::clamp(px + ox, 0, w - 1);
									const int qy = std::clamp(py + oy, 0, h - 1);
									sum += shadow[qy * w + qx];
								}
							}
							sh = sum / float((2 * blur + 1) * (2 * blur + 1));
						}
						const float lit  = (1.0f - sh) * facing(fbase[gy * hline + gx]) * sun_boost;
						// Under a roof the shade is gentler than a shadow outside.
						const bool  inside = covered[idx(gx, gy)] != 0;
						const float dark   = sh * (1.0f - (inside ? indoor_dim : shadow_amt));
						if (tint) {
							lr *= lit * sun_r + dark * sky_r;
							lg *= lit * sun_g + dark * sky_g;
							lb *= lit * sun_b + dark * sky_b;
						} else {
							lr *= lit + dark;
							lg = lb = lr;
						}
					}
					float* out = &light[(size_t(py) * w + px) * 3];
					out[0]     = lr;
					out[1]     = lg;
					out[2]     = lb;
				}
			}
		});
		// Daylight through doorways: indoors, the floor near an opening to
		// the open air takes some of the light outside (all of it from sunlit
		// ground, about half from shade), fading over several tiles.  It
		// spreads only across the floor, so walls stop it.
		if (sunlit && roofed && indoor_dim > 0) {
			const int      range = 56;    // Pixels.
			const uint16_t far   = 0xffff;
			spill.assign(size_t(w) * h, far);
			spill_v.assign(size_t(w) * h, 0);
			auto passable = [&](int px, int py) {
				return hbase[(y + py) * hline + x + px] <= roof_floor + 8;
			};
			for (int py = 0; py < h; py++) {
				for (int px = 0; px < w; px++) {
					const int gx = x + px, gy = y + py;
					if (!covered[idx(gx, gy)] && passable(px, py)) {
						spill[py * w + px]   = 0;
						spill_v[py * w + px] = shadow[py * w + px] ? 140 : 255;
					}
				}
			}
			// Two-pass chamfer distance (3 straight, 4 diagonal) over the
			// floor under the roof.
			auto relax = [&](int px, int py, int qx, int qy, int cost) {
				if (qx < 0 || qy < 0 || qx >= w || qy >= h) {
					return;
				}
				const uint16_t from = spill[qy * w + qx];
				if (from != far && from + cost < spill[py * w + px]) {
					spill[py * w + px]   = uint16_t(from + cost);
					spill_v[py * w + px] = spill_v[qy * w + qx];
				}
			};
			for (int py = 0; py < h; py++) {
				for (int px = 0; px < w; px++) {
					if (spill[py * w + px] == 0 || !covered[idx(x + px, y + py)] || !passable(px, py)) {
						continue;
					}
					relax(px, py, px - 1, py, 3);
					relax(px, py, px, py - 1, 3);
					relax(px, py, px - 1, py - 1, 4);
					relax(px, py, px + 1, py - 1, 4);
				}
			}
			for (int py = h - 1; py >= 0; py--) {
				for (int px = w - 1; px >= 0; px--) {
					if (spill[py * w + px] == 0 || !covered[idx(x + px, y + py)] || !passable(px, py)) {
						continue;
					}
					relax(px, py, px + 1, py, 3);
					relax(px, py, px, py + 1, 3);
					relax(px, py, px + 1, py + 1, 4);
					relax(px, py, px - 1, py + 1, 4);
				}
			}
			const float tr = tint ? sun_r : 1.0f, tg = tint ? sun_g : 1.0f, tb = tint ? sun_b : 1.0f;
			pool().run(h, [&](int r0, int r1) {
				for (int py = r0; py < r1; py++) {
					for (int px = 0; px < w; px++) {
						const uint16_t dist = spill[py * w + px];
						if (dist == 0 || dist >= 3 * range || !covered[idx(x + px, y + py)]) {
							continue;
						}
						const float f   = 1.0f - dist / (3.0f * range);
						const float amt = 0.75f * f * f * (spill_v[py * w + px] / 255.0f) * std::min(1.0f, sun_up * 4.0f);
						float*      out = &light[(size_t(py) * w + px) * 3];
						out[0] += amt * (tr * sun_boost - out[0]);
						out[1] += amt * (tg * sun_boost - out[1]);
						out[2] += amt * (tb * sun_boost - out[2]);
					}
				}
			});
		}
		// Sunbeams: indoors, floor that can see the sun out through a door
		// or window (walls in the way block it, and the ray must leave
		// below the eaves, not through the roof) gets a patch of direct
		// sunlight, slanting in with the time of day.
		if (sunlit && roofed && indoor_dim > 0 && clear > 0.0f) {
			const float eaves = roof_floor + 26.0f;    // About a door's height.
			beam.assign(size_t(w) * h, 0);
			pool().run(h, [&](int r0, int r1) {
				for (int py = r0; py < r1; py++) {
					for (int px = 0; px < w; px++) {
						const int gx = x + px, gy = y + py;
						if (!covered[idx(gx, gy)]) {
							continue;
						}
						const int hp = hbase[gy * hline + gx];
						if (hp > roof_floor + 8) {
							continue;    // Walls and tall furniture.
						}
						for (float st = 1.5f; st < 96.0f; st += 1.0f) {
							const float rz = hp + st * slope;
							if (rz > eaves) {
								break;    // Would have to pass through the roof.
							}
							const int sx = gx + int(std::lround(st * dirx));
							const int sy = gy + int(std::lround(st * diry));
							if (!in_buffer(sx, sy) || height_at(sx, sy) > rz + step_dead_zone) {
								break;
							}
							if (!covered[idx(sx, sy)]) {
								// Out in the open: lit if that spot is in sun.
								const int  lx = sx - x, ly = sy - y;
								const bool shaded = lx >= 0 && ly >= 0 && lx < w && ly < h && shadow[ly * w + lx];
								beam[py * w + px] = shaded ? 0 : 1;
								break;
							}
						}
					}
				}
			});
			const int   bb   = set.soft ? 2 : 0;
			const float fade = std::min(1.0f, sun_up * 4.0f);
			const float tr = tint ? sun_r : 1.0f, tg = tint ? sun_g : 1.0f, tb = tint ? sun_b : 1.0f;
			pool().run(h, [&](int r0, int r1) {
				for (int py = r0; py < r1; py++) {
					for (int px = 0; px < w; px++) {
						int sum = 0;
						for (int oy = -bb; oy <= bb; oy++) {
							for (int ox = -bb; ox <= bb; ox++) {
								const int qx = std::clamp(px + ox, 0, w - 1), qy = std::clamp(py + oy, 0, h - 1);
								sum += beam[qy * w + qx];
							}
						}
						if (!sum) {
							continue;
						}
						// A touch brighter than open ground, as a sunbeam reads.
						const float k   = 0.9f * clear * fade * sum / float((2 * bb + 1) * (2 * bb + 1));
						const float hi  = sun_boost * 1.08f;
						float*      out = &light[(size_t(py) * w + px) * 3];
						out[0] += k * (tr * hi - out[0]);
						out[1] += k * (tg * hi - out[1]);
						out[2] += k * (tb * hi - out[2]);
					}
				}
			});
		}
		// Night: the edges of the view fade into the dark, a little like
		// fog of war, while the middle stays as the game shows it.  Lamps
		// are added after, so they still glow out at the edges.
		if (set.night_edges > 0 && cur_view == normal) {
			const float day   = std::clamp((sun_up + 0.05f) * 4.0f, 0.0f, 1.0f);
			const float night = gwin->is_in_dungeon() ? 0.7f : 1.0f - day;
			static const float depths[3] = {0.0f, 0.35f, 0.55f};
			const float depth = depths[set.night_edges] * night;
			if (depth > 0.01f) {
				const TileRect game = gwin->get_game_rect();
				const float    cx = game.x + game.w * 0.5f, cy = game.y + game.h * 0.5f;
				const float    rx = game.w * 0.5f, ry = game.h * 0.5f;
				pool().run(h, [&](int r0, int r1) {
					for (int py = r0; py < r1; py++) {
						const float ny = (y + py - cy) / ry;
						for (int px = 0; px < w; px++) {
							const float nx = (x + px - cx) / rx;
							// 0 inside the middle oval, rising smoothly to 1 at the corners.
							float e = std::clamp((std::sqrt(nx * nx + ny * ny) - 0.45f) / 0.95f, 0.0f, 1.0f);
							e       = e * e * (3.0f - 2.0f * e);
							const float k   = 1.0f - depth * e;
							float*      out = &light[(size_t(py) * w + px) * 3];
							out[0] *= k;
							out[1] *= k;
							out[2] *= k;
						}
					}
				});
			}
		}
		// In full colour, lamp light is added on top in the colours the
		// things have by day, so a lamp brings them out of the dark (even
		// a dungeon's near-black) instead of only scaling what is there.
		if (tint) {
			glow.assign(size_t(w) * h * 3, 0.0f);
			add_lamps(gwin, x, y, w, h, sun_up, sunlit, tint, glow);
		} else {
			add_lamps(gwin, x, y, w, h, sun_up, sunlit, tint, light);
		}
	}

	/*
	 *  Put the light worked out above onto the frame: in full colour as a
	 *  layer for the window, else through the palette.
	 */
	void present(Game_window* gwin, int x, int y, int w, int h, unsigned char* bits, int line, bool full) {
		if (full) {
			// Hand the light to the window as a layer; keep the map as
			// painted to tell it from what is drawn on top later.
			overlay.resize(size_t(w) * h);
			glow_px.resize(size_t(w) * h);
			map_copy.resize(size_t(w) * h);
			const Palette& day   = gwin->get_pal()->day_palette();
			const float    scale = 255.0f / std::max(1, gwin->get_pal()->get_max_val());
			pool().run(h, [&](int r0, int r1) {
				for (int py = r0; py < r1; py++) {
					std::memcpy(&map_copy[size_t(py) * w], bits + (y + py) * line + x, w);
					for (int px = 0; px < w; px++) {
						const float* l  = &light[(size_t(py) * w + px) * 3];
						auto         ch = [](float v) {
                            return uint32(std::clamp(int(v * 128.0f + 0.5f), 0, 255));
						};
						overlay[size_t(py) * w + px] = 0xff000000u | (ch(l[0]) << 16) | (ch(l[1]) << 8) | ch(l[2]);
						const float*        gl = &glow[(size_t(py) * w + px) * 3];
						const unsigned char c  = map_copy[size_t(py) * w + px];
						auto                add = [&](float k, int v) {
                            return uint32(std::clamp(int(k * v * scale + 0.5f), 0, 255));
						};
						glow_px[size_t(py) * w + px] = 0xff000000u | (add(gl[0], day.get_red(c)) << 16)
													   | (add(gl[1], day.get_green(c)) << 8) | add(gl[2], day.get_blue(c));
					}
				}
			});
			ov_x       = x;
			ov_y       = y;
			ov_w       = w;
			ov_h       = h;
			ov_pending = true;
			return;
		}
		pool().run(h, [&](int r0, int r1) {
			for (int py = r0; py < r1; py++) {
				unsigned char* row = bits + (y + py) * line + x;
				for (int px = 0; px < w; px++) {
					const float* l   = &light[(size_t(py) * w + px) * 3];
					const float  lum = 0.3f * l[0] + 0.59f * l[1] + 0.11f * l[2];
					const int    k   = level_for(lum);
					row[px]          = cur_view == show_light ? gray[k] : shade[k][row[px]];
				}
			}
		});
	}

	/*
	 *  QOTC_RELIGHT_BENCH: once, time the lighting of this view at dawn,
	 *  noon and night, the old way and the new, and how many pixels' sun
	 *  shadow differ between the old walk and the new sweep.
	 */
	void run_bench(Game_window* gwin, int x, int y, int w, int h) {
		bench_done  = true;
		last_valid  = false;
		const int cores = int(std::thread::hardware_concurrency());
		std::cerr << "Relight bench: " << w << "x" << h << " game pixels, " << cores << " cores" << std::endl;
		auto best = [&](bool march, bool one_core, float hour) {
			use_march     = march;
			pool().serial = one_core;
			double b      = 1e30;
			for (int i = 0; i < 3; i++) {
				const double t0 = now_ms();
				compute_light(gwin, x, y, w, h, hour);
				b = std::min(b, now_ms() - t0);
			}
			use_march     = false;
			pool().serial = false;
			return b;
		};
		for (const float hour : {6.5f, 7.5f, 12.0f, 17.5f, 21.0f}) {
			const double old_way  = best(true, true, hour);
			const std::vector<unsigned char> old_shadow = shadow;
			const double one_core = best(false, true, hour);
			const double all      = best(false, false, hour);
			size_t       differ   = 0;
			for (size_t i = 0; i < shadow.size() && i < old_shadow.size(); i++) {
				differ += shadow[i] != old_shadow[i];
			}
			char msg[200];
			std::snprintf(msg, sizeof(msg),
						  "  %5.2f h: old walk, 1 core %7.1f ms | sweep, 1 core %7.1f ms | sweep, all cores %7.1f ms | "
						  "shadow differs on %.2f%% of pixels",
						  hour, old_way, one_core, all, 100.0 * differ / std::max<size_t>(1, shadow.size()));
			std::cerr << msg << std::endl;
		}
	}

	/*
	 *  Light the map pixels in the rectangle (game coordinates).
	 */
	void apply(Game_window* gwin, int x, int y, int w, int h) {
		const double   t0 = bench_wanted() ? now_ms() : 0;
		Image_buffer8* ib = gwin->get_win()->get_ib8();
		rebuild_tables(gwin->get_pal());
		ov_pending = false;
		x          = std::max(x, hmin_x);
		y          = std::max(y, hmin_y);
		w          = std::min(x + w, hmax_x) - x;
		h          = std::min(y + h, hmax_y) - y;
		if (w <= 0 || h <= 0) {
			return;
		}
		scan_world(gwin);
		unsigned char* bits = ib->get_bits();
		const int      line = ib->get_line_width();
		if (cur_view == show_height) {
			for (int py = y; py < y + h; py++) {
				for (int px = x; px < x + w; px++) {
					const int hv         = hbase[py * hline + px];
					bits[py * line + px] = gray[std::min(num_levels - 1, 2 + hv * (num_levels - 3) / 96)];
				}
			}
			return;
		}
		const Settings&   set   = settings();
		const Game_clock* clock = gwin->get_clock();
		Light_key         key;
		key.x          = x;
		key.y          = y;
		key.w          = w;
		key.h          = h;
		key.hx         = hmin_x;
		key.hy         = hmin_y;
		key.hl         = hline;
		key.hour       = clock->get_hour();
		key.minute     = clock->get_minute();
		update_fog(gwin);
		key.fog        = int(std::lround(fog_now * 64.0f));
		key.dungeon    = gwin->is_in_dungeon();
		key.special    = gwin->is_special_light();
		key.roofed     = roofed;
		key.roof_floor = roof_floor;
		key.set        = set;
		key.view       = cur_view;
		const TileRect game = gwin->get_game_rect();
		key.gx         = game.x;
		key.gy         = game.y;
		key.gw         = game.w;
		key.gh         = game.h;
		if (bench_wanted() && !bench_done && ++bench_calls >= 60) {
			run_bench(gwin, x, y, w, h);
		}
		// Nothing the light depends on changed (a menu or the cursor moved):
		// keep the light already worked out.
		const bool reuse = last_valid && same_key(key, last_key) && heights == last_heights && faces == last_faces
						   && bases == last_bases && figures == last_figures && casters == last_casters && covered == last_covered && same_lamps(lamps, last_lamps);
		if (!reuse) {
			compute_light(gwin, x, y, w, h, clock->get_hour() + clock->get_minute() / 60.0f);
			last_key     = key;
			last_heights = heights;
			last_faces   = faces;
			last_bases   = bases;
			last_figures = figures;
			last_casters = casters;
			last_covered = covered;
			last_lamps   = lamps;
			last_valid   = true;
		}
		present(gwin, x, y, w, h, bits, line, set.full_colour && cur_view == normal);
		if (bench_wanted()) {
			const double ms = now_ms() - t0;
			bench_ms += ms;
			bench_max = std::max(bench_max, ms);
			bench_reused += reuse;
			if (++bench_frames == 120) {
				char msg[160];
				std::snprintf(msg, sizeof(msg), "Relight: %02d:%02d, 120 frames, %.1f ms average, %.1f ms most, %d reused",
							  clock->get_hour(), clock->get_minute(), bench_ms / 120, bench_max, bench_reused);
				std::cerr << msg << std::endl;
				bench_frames = bench_reused = 0;
				bench_ms = bench_max = 0;
			}
		}
	}

	/*
	 *  After the whole frame is painted: hand over the full-colour light
	 *  layer, leaving unlit whatever was drawn over the map since.  Only the
	 *  rows that differ from what the window already has are sent.
	 */
	void finish(Game_window* gwin) {
		Image_window8* win = gwin->get_win();
		if (!ov_pending) {
			win->clear_light_overlay();
			return;
		}
		ov_pending                = false;
		Image_buffer8*       ib   = win->get_ib8();
		const unsigned char* bits = ib->get_bits();
		const int            line = ib->get_line_width();
		const size_t         n    = size_t(ov_w) * ov_h;
		const bool           all  = out_overlay.size() != n;
		if (all) {
			out_overlay.assign(n, 0);
			out_glow.assign(n, 0);
		}
		std::mutex mtx;
		int        first = ov_h, last = -1;
		pool().run(ov_h, [&](int r0, int r1) {
			int lo = ov_h, hi = -1;
			for (int py = r0; py < r1; py++) {
				const unsigned char* row     = bits + (ov_y + py) * line + ov_x;
				const unsigned char* was     = &map_copy[size_t(py) * ov_w];
				const size_t         o       = size_t(py) * ov_w;
				bool                 changed = all;
				for (int px = 0; px < ov_w; px++) {
					uint32 lv = overlay[o + px], gv = glow_px[o + px];
					if (row[px] != was[px]) {
						lv = 0xff808080u;
						gv = 0xff000000u;
					}
					if (out_overlay[o + px] != lv || out_glow[o + px] != gv) {
						out_overlay[o + px] = lv;
						out_glow[o + px]    = gv;
						changed             = true;
					}
				}
				if (changed) {
					lo = std::min(lo, py);
					hi = py;
				}
			}
			if (hi >= 0) {
				const std::lock_guard<std::mutex> lock(mtx);
				first = std::min(first, lo);
				last  = std::max(last, hi);
			}
		});
		win->set_light_overlay(out_overlay.data(), out_glow.data(), ov_x, ov_y, ov_w, ov_h, first, last + 1);
	}
}    // namespace Relight

/*
 *  Paint just the map with given top-left-corner tile.
 */

void Game_window::paint_map_at_tile(
		int x, int y, int w, int h,    // Clip to this area.
		int toptx, int topty,
		int skip_above    // Don't display above this lift.
) {
	const int savescrolltx = scrolltx;
	const int savescrollty = scrollty;
	const int saveskip     = skip_lift;
	scrolltx               = toptx;
	scrollty               = topty;
	skip_lift              = skip_above;
	map->read_map_data();    // Gather in all objs., etc.
	win->set_clip(x, y, w, h);
	render->paint_map(0, 0, get_width(), get_height());
	win->clear_clip();
	scrolltx  = savescrolltx;
	scrollty  = savescrollty;
	skip_lift = saveskip;
}

/*
 *  Figure offsets on screen.
 */

inline int Figure_screen_offset(
		int ch,       // Chunk #
		int scroll    // Top/left tile of screen.
) {
	// Watch for wrapping.
	int t = ch * c_tiles_per_chunk - scroll;
	if (t < -c_num_tiles / 2) {
		t += c_num_tiles;
	}
	t %= c_num_tiles;
	return t * c_tilesize;
}

/*
 *  Show the outline around a chunk.
 */

inline void Paint_chunk_outline(
		Game_window* gwin,
		int          pixel,    // Pixel value to use.
		int cx, int cy,        // Chunk coords.
		int tnum,              // Terrain #.
		int xoff, int yoff     // Where chunk was painted.
) {
	gwin->get_win()->fill8(pixel, c_chunksize, 1, xoff, yoff);
	gwin->get_win()->fill8(pixel, 1, c_chunksize, xoff, yoff);
	char text[40];    // Show chunk #.
	snprintf(text, sizeof(text), "(%d,%d)T%d", cx, cy, tnum);
	Shape_manager::get_instance()->paint_text(2, text, xoff + 2, yoff + 2);
}

/*
 *  Paint tile grid.
 */

static void Paint_grid(
		Game_window*   gwin,
		Xform_palette& xform    // For transparency.
) {
	Image_window8* win = gwin->get_win();
	// Paint grid at edit height.
	const int xtiles     = gwin->get_width() / c_tilesize;
	const int ytiles     = gwin->get_height() / c_tilesize;
	const int lift       = cheat.get_edit_lift();
	const int liftpixels = lift * (c_tilesize / 2) + 1;
	for (int y = 0; y < ytiles; y++) {
		win->fill_translucent8(0, xtiles * c_tilesize, 1, -liftpixels, y * c_tilesize - liftpixels, xform);
	}
	for (int x = 0; x < xtiles; x++) {
		win->fill_translucent8(0, 1, ytiles * c_tilesize, x * c_tilesize - liftpixels, -liftpixels, xform);
	}
}

/*
 *  Highlight selected chunks.
 */

static void Paint_selected_chunks(
		Game_window*   gwin,
		Xform_palette& xform,    // For transparency.
		int start_chunkx, int start_chunky, int stop_chunkx, int stop_chunky) {
	Game_map*      map = gwin->get_map();
	Image_window8* win = gwin->get_win();
	int            cx;
	int            cy;    // Chunk #'s.
	// Paint all the flat scenery.
	for (cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
		const int yoff = Figure_screen_offset(cy, gwin->get_scrollty()) - gwin->get_scrollty_lo();
		for (cx = start_chunkx; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
			Map_chunk* chunk = map->get_chunk(cx, cy);
			if (!chunk->is_selected()) {
				continue;
			}
			const int xoff = Figure_screen_offset(cx, gwin->get_scrolltx()) - gwin->get_scrolltx_lo();
			win->fill_translucent8(0, c_chunksize, c_chunksize, xoff, yoff, xform);
		}
	}
}

/*
 *  Just paint terrain.  This is for terrain_editing mode.
 */

void Game_render::paint_terrain_only(int start_chunkx, int start_chunky, int stop_chunkx, int stop_chunky) {
	Game_window*   gwin = Game_window::get_instance();
	Game_map*      map  = gwin->map;
	Shape_manager* sman = Shape_manager::get_instance();
	int            cx;
	int            cy;    // Chunk #'s.
	// Paint all the flat scenery.
	for (int pass = 1; pass <= 3; pass++) {
		for (cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
			const int yoff = Figure_screen_offset(cy, gwin->scrollty) - gwin->get_scrollty_lo();
			for (cx = start_chunkx; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
				const int xoff = Figure_screen_offset(cx, gwin->scrolltx) - gwin->get_scrolltx_lo();
				if (pass < 3) {
					Map_chunk* chunk = map->get_chunk(cx, cy);
					chunk->get_terrain()->render_all(cx, cy, pass);
				}
				if (cheat.in_map_editor() && pass == 3) {
					Paint_chunk_outline(gwin, sman->get_special_pixel(HIT_PIXEL), cx, cy, map->get_terrain_num(cx, cy), xoff, yoff);
				}
			}
		}
	}
	// Paint tile grid if desired.
	if (cheat.show_tile_grid()) {
		Paint_grid(gwin, sman->get_xform(16));
	}
}

/*
 *  Paint just the map and its objects (no gumps, effects).
 *  (The caller should set/clear clip area.)
 *
 *  Output: # light-sources found.
 */

int Game_render::paint_map(
		int x, int y, int w, int h    // Rectangle to cover.
) {
	Game_window*   gwin = Game_window::get_instance();
	Game_map*      map  = gwin->map;
	Shape_manager* sman = gwin->shape_man;
	render_seq++;    // Increment sequence #.
	gwin->painted = true;

	const int scrolltx      = gwin->scrolltx;
	const int scrollty      = gwin->scrollty;
	int       light_sources = 0;    // Count light sources found.
	// Get chunks to start with, starting
	//   1 tile left/above.
	int start_chunkx = (scrolltx + x / c_tilesize - 1) / c_tiles_per_chunk;
	// Wrap around.
	start_chunkx     = (start_chunkx + c_num_chunks) % c_num_chunks;
	int start_chunky = (scrollty + y / c_tilesize - 1) / c_tiles_per_chunk;
	start_chunky     = (start_chunky + c_num_chunks) % c_num_chunks;
	// End 8 tiles to right.
	// The chunk limits were increased by 1 to support the Smooth Scrolling.
	// The same increase had to be added into the Game_map::read_map_data
	//   which builds the chunk cache that the Edit Terrain mode relies on.
	int stop_chunkx = 2 + (scrolltx + (x + w + c_tilesize - 2) / c_tilesize + c_tiles_per_chunk / 2) / c_tiles_per_chunk;
	int stop_chunky = 2 + (scrollty + (y + h + c_tilesize - 2) / c_tilesize + c_tiles_per_chunk / 2) / c_tiles_per_chunk;
	// Wrap around the world:
	stop_chunkx = (stop_chunkx + c_num_chunks) % c_num_chunks;
	stop_chunky = (stop_chunky + c_num_chunks) % c_num_chunks;
	if (!gwin->skip_lift) {    // Special mode for editing?
		paint_terrain_only(start_chunkx, start_chunky, stop_chunkx, stop_chunky);
		return 10;    // Pretend there's lots of light!
	}
	int cx;
	int cy;    // Chunk #'s.
	if (Relight::active()) {
		Relight::begin_map(gwin->get_win(), x, y, w, h);
	}
	// Paint all the flat scenery.
	for (cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
		const int yoff = Figure_screen_offset(cy, scrollty) - gwin->get_scrollty_lo();
		for (cx = start_chunkx; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
			const int xoff = Figure_screen_offset(cx, scrolltx) - gwin->get_scrolltx_lo();
			paint_chunk_flats(cx, cy, xoff, yoff);
		}
	}
	// Now the flat RLE terrain.
	for (cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
		const int yoff = Figure_screen_offset(cy, scrollty) - gwin->get_scrollty_lo();
		for (cx = start_chunkx; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
			const int xoff = Figure_screen_offset(cx, scrolltx) - gwin->get_scrolltx_lo();
			paint_chunk_flat_rles(cx, cy, xoff, yoff);
		}
	}
	// Draw the chunk grid in Map editor cheat mode.
	if (cheat.in_map_editor()) {
		for (cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
			const int yoff = Figure_screen_offset(cy, scrollty) - gwin->get_scrollty_lo();
			for (cx = start_chunkx; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
				const int xoff = Figure_screen_offset(cx, scrolltx) - gwin->get_scrolltx_lo();
				Paint_chunk_outline(gwin, sman->get_special_pixel(HIT_PIXEL), cx, cy, map->get_terrain_num(cx, cy), xoff, yoff);
			}
		}
	}
	// Draw the chunks' objects
	//   diagonally NE.
	const int tmp_stopy = DECR_CHUNK(start_chunky);
	for (cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
		for (int dx = start_chunkx, dy = cy; dx != stop_chunkx && dy != tmp_stopy; dx = INCR_CHUNK(dx), dy = DECR_CHUNK(dy)) {
			light_sources += paint_chunk_objects(dx, dy);
		}
	}
	for (cx = (start_chunkx + 1) % c_num_chunks; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
		for (int dx = cx, dy = (stop_chunky - 1 + c_num_chunks) % c_num_chunks; dx != stop_chunkx && dy != tmp_stopy;
			 dx = INCR_CHUNK(dx), dy = DECR_CHUNK(dy)) {
			light_sources += paint_chunk_objects(dx, dy);
		}
	}
	/// Dungeon Blackness (but disable in map editor mode)
	if (static_cast<int>(gwin->in_dungeon) >= gwin->skip_above_actor && !cheat.in_map_editor()) {
		paint_blackness(start_chunkx, start_chunky, stop_chunkx, stop_chunky, gwin->ice_dungeon ? 73 : 0);
	}

	// Outline selected objects.
	const Game_object_shared_vector& sel         = cheat.get_selected();
	const int                        render_skip = gwin->get_render_skip_lift();
	for (const auto& it : sel) {
		Game_object* obj = it.get();
		if (!obj->get_owner() && obj->get_lift() < render_skip) {
			obj->paint_outline(HIT_PIXEL);
		}
	}

	// Paint tile grid if desired.
	if (cheat.in_map_editor()) {
		if (cheat.show_tile_grid()) {
			Paint_grid(gwin, sman->get_xform(16));
		}
		if (cheat.get_edit_mode() == Cheat::select_chunks) {
			Paint_selected_chunks(gwin, sman->get_xform(13), start_chunkx, start_chunky, stop_chunkx, stop_chunky);
		}
	}
	return light_sources;
}

static int Get_light_strength(const Game_object* obj, const Game_object* av, int brightness) {
	// Note: originals do not seem to use center tile.
	const Tile_coord t1 = obj->get_center_tile();
	const Tile_coord t2 = av->get_center_tile();
	// Note: originals do not care about distance in Z. Maybe we should?
	const int dx = std::abs(Tile_coord::delta(t1.tx, t2.tx));
	const int dy = std::abs(Tile_coord::delta(t1.ty, t2.ty));
	// This seems to match the originals as far as distance effects go.
	const int dist_decay_factor = std::max(0, 75 - 2 * dx - 3 * dy);
	// Finally, return how bright this light is.
	return dist_decay_factor * brightness;
}

int Game_render::get_light_strength(const Game_object* obj, const Game_object* av) const {
	const Shape_info& info = obj->get_info();
	return Get_light_strength(obj, av, info.get_object_light(obj->get_framenum()));
}

void Game_render::increment_bbox_index() {
	int  bbox_indices[] = {15, 0, 22, 38, 5, 64, 80, 94, -1};
	auto start          = bbox_indices;
	auto end            = bbox_indices + std::size(bbox_indices);

	size_t found = std::find(start, end, bbox_palindex) - start;

	if (found < std::size(bbox_indices)) {
		bbox_palindex = bbox_indices[(found + 1) % std::size(bbox_indices)];
	}
	Game_window::get_instance()->set_all_dirty();
}

/*
 *  Paint a rectangle in the window by pulling in vga chunks.
 */

void Game_window::paint(
		int x, int y, int w, int h    // Rectangle to cover.
) {
	auto perftimer = PerformanceTimer::GetScopedPerfTimer(__func__);

	if (!win->ready()) {
		return;
	}
	if (Relight::active() && main_actor) {
		// Shadows reach across the screen, so light it all at once.  Keep
		// what was asked for outside the game view too (the black bars of
		// Fill mode), or it was never painted black again.
		const int x1 = std::max(x + w, get_width());
		const int y1 = std::max(y + h, get_height());
		x            = std::min(x, 0);
		y            = std::min(y, 0);
		w            = x1 - x;
		h            = y1 - y;
	}
	// This will adjust and clip the rectangle as appropriate, it may end up
	// bigger or smaller
	win->BeginPaintIntoGuardBand(&x, &y, &w, &h);
	int gx = x;
	int gy = y;
	int gw = w;
	int gh = h;
	if (gx < 0) {
		gw += x;
		gx = 0;
	}
	if ((gx + gw) > get_width()) {
		gw = get_width() - gx;
	}
	if (gy < 0) {
		gh += gy;
		gy = 0;
	}
	if ((gy + gh) > get_height()) {
		gh = get_height() - gy;
	}
	win->set_clip(gx, gy, gw, gh);    // Clip to this area.

	int light_sources = 0;

	if (main_actor) {
		light_sources = render->paint_map(gx, gy, gw, gh);
		if (Relight::active() && skip_lift) {
			Relight::apply(this, gx, gy, gw, gh);
		}
	} else {
		win->fill8(0);
	}

	effects->paint();    // Draw sprites.
	if (Combat::health_bars) {
		paint_health_bars();
	}

	win->set_clip(x, y, w, h);    // Clip to this area.
	// Fill black into unpainted regions
	if (y < 0) {
		win->fill8(pal->get_border_index(), w, -y, x, y);    // Region above window
	}
	if (x < 0) {
		win->fill8(pal->get_border_index(), -x, get_height(), x,
				   0);    // Region left of window
	}
	if ((x + w) > get_width()) {
		win->fill8(pal->get_border_index(), (x + w) - get_width(), get_height(), get_width(), 0);    // Region right of window
	}
	if ((y + h) > get_height()) {
		win->fill8(pal->get_border_index(), w, (y + h) - get_height(), x,
				   get_height());    // below window
	}

	gump_man->paint(false);
	if (dragging) {
		dragging->paint();    // Paint what user is dragging.
	}
	effects->paint_text();
	gump_man->paint(true);

	// Complete repaint?
	if (!gx && !gy && gw == get_width() && gh == get_height() && main_actor) {
		// Look for lights.
		Actor*    party[9];    // Get party, including Avatar.
		const int cnt           = get_party(party, 1);
		int       carried_light = 0;
		// Light is measured from whoever the view follows (the Avatar, or a
		// Friend Mode guest's companion).
		const Actor* viewer = get_camera_actor() ? get_camera_actor() : main_actor;
		for (int i = 0; i < cnt; i++) {
			carried_light += Get_light_strength(party[i], viewer, party[i]->get_light_source());
		}
		// Also check light spell.
		if (special_light && clock->get_total_minutes() > special_light) {
			// Just expired.
			special_light = 0;
			clock->set_palette();
		}
		// Set palette for lights.
		clock->set_light_source(carried_light + light_sources, in_dungeon);
	}

	if (main_actor && Relight::active()) {
		Relight::finish(this);
	} else {
		win->clear_light_overlay();
	}

	win->EndPaintIntoGuardBand();
	win->clear_clip();
}

/*
 *  Paint whole window.
 */
/*
 *  Health bars over hurt foes, in the style of Ultima Online (an option;
 *  U7 has none).  Shown over creatures and NPCs outside the party whose
 *  health is below full, so peaceful townsfolk stay clear.
 */

void Game_window::paint_health_bars() {
	constexpr int bar_w = 16;
	Shape_manager* shapes = Shape_manager::get_instance();
	const int     green  = shapes->get_special_pixel(POISON_PIXEL);
	const int     red    = shapes->get_special_pixel(HIT_PIXEL);
	const int     black  = shapes->get_special_pixel(BLACK_PIXEL);
	Actor_vector  nearby;
	get_nearby_npcs(nearby);
	for (Actor* actor : nearby) {
		if (!actor || actor->is_in_party() || actor->is_dead() || actor->get_flag(Obj_flags::invisible)
			|| actor->get_map() != get_map() || actor->is_pos_invalid()) {
			continue;
		}
		const int max_health = actor->get_property(static_cast<int>(Actor::strength));
		const int health     = actor->get_property(static_cast<int>(Actor::health));
		if (max_health <= 0 || health >= max_health) {
			continue;    // Unhurt.
		}
		const TileRect shape = get_shape_rect(actor);
		const int      bx    = shape.x + (shape.w - bar_w) / 2;
		// Inside the 5 pixels Game_window::add_dirty repaints around it.
		const int      by    = shape.y - 4;
		const int      fill  = std::clamp(health * bar_w / max_health, 0, bar_w);
		win->fill8(black, bar_w + 2, 4, bx - 1, by - 1);
		win->fill8(red, bar_w, 2, bx, by);
		if (fill > 0) {
			win->fill8(green, fill, 2, bx, by);
		}
	}
}

void Game_window::paint() {
	if (main_actor != nullptr) {
		map->read_map_data();    // Gather in all objs., etc.
	}
	set_all_dirty();
	paint_dirty();
}

void Game_window::lerp_reset() {
	scrolltx_lp = scrolltx_l;
	scrollty_lp = scrollty_l;
	scrolltx_l  = scrolltx;
	scrollty_l  = scrollty;
}

void Game_window::paint_lerped(int factor) {
	if (factor < 0) {
		factor = 0;
	}
	if (factor > 0x10000) {
		factor = 0x10000;
	}

	const int saved_scrolltx = scrolltx;
	const int saved_scrollty = scrollty;

	scrolltx = scrolltx_l;
	scrollty = scrollty_l;

	int dx = (scrolltx_lp - scrolltx);
	int dy = (scrollty_lp - scrollty);

	// wrap around fixing...
	while (dx < -c_num_tiles / 2) {
		dx += c_num_tiles;
	}
	while (dx > c_num_tiles / 2) {
		dx -= c_num_tiles;
	}
	while (dy < -c_num_tiles / 2) {
		dy += c_num_tiles;
	}
	while (dy > c_num_tiles / 2) {
		dy -= c_num_tiles;
	}

	// Only allow lerping to occur within say a 4 tile limit
	if (dx > -4 && dx < 4 && dy > -4 && dy < 4) {
		dx *= c_tilesize;
		dy *= c_tilesize;
		scrolltx *= c_tilesize;
		scrollty *= c_tilesize;

		scrolltx = scrolltx + (dx * (0x10000 - factor)) / 0x10000;
		scrollty = scrollty + (dy * (0x10000 - factor)) / 0x10000;

		dx = scrolltx % c_tilesize;
		dy = scrollty % c_tilesize;

		avposx_ld = scrolltx - saved_scrolltx * c_tilesize;
		avposy_ld = scrollty - saved_scrollty * c_tilesize;

		while (avposx_ld < -c_num_tiles * c_tilesize / 2) {
			avposx_ld += c_num_tiles * c_tilesize;
		}
		while (avposx_ld > c_num_tiles * c_tilesize / 2) {
			avposx_ld -= c_num_tiles * c_tilesize;
		}
		while (avposy_ld < -c_num_tiles * c_tilesize / 2) {
			avposy_ld += c_num_tiles * c_tilesize;
		}
		while (avposy_ld > c_num_tiles * c_tilesize / 2) {
			avposy_ld -= c_num_tiles * c_tilesize;
		}

		scrolltx = ((scrolltx / c_tilesize) + c_num_tiles) % c_num_tiles;
		scrollty = ((scrollty / c_tilesize) + c_num_tiles) % c_num_tiles;

		// printf ("f %05x %i-%i %i.%i\n", factor, scrolltx_lp, scrolltx_l,
		// scrolltx, dx);
	} else {
		dx = 0;
		dy = 0;
	}

	// Set pixel offset needed for lerping
	scrolltx_lo = dx;
	scrollty_lo = dy;

	paint();

	scrolltx    = saved_scrolltx;
	scrollty    = saved_scrollty;
	scrolltx_lo = scrollty_lo = 0;
	avposx_ld = avposy_ld = 0;
}

/*
 *  Paint the flat (non-rle) shapes in a chunk.
 */

void Game_render::paint_chunk_flats(
		int cx, int cy,       // Chunk coords (0 - 12*16).
		int xoff, int yoff    // Pixel offset of top-of-screen.
) {
	Game_window* gwin  = Game_window::get_instance();
	Map_chunk*   olist = gwin->map->get_chunk(cx, cy);
	// Paint flat tiles.
	Image_buffer8* cflats = olist->get_rendered_flats();
	if (cflats) {
		gwin->win->copy8(cflats->get_bits(), c_chunksize, c_chunksize, xoff, yoff);
	}
}

/*
 *  Paint the flat RLE (terrain) shapes in a chunk.
 */

void Game_render::paint_chunk_flat_rles(
		int cx, int cy,       // Chunk coords (0 - 12*16).
		int xoff, int yoff    // Pixel offset of top-of-screen.
) {
	ignore_unused_variable_warning(xoff, yoff);
	Game_window*         gwin  = Game_window::get_instance();
	Map_chunk*           olist = gwin->map->get_chunk(cx, cy);
	Flat_object_iterator next(olist);    // Do flat RLE objects.
	Game_object*         obj;
	const bool           relight = Relight::active();
	while ((obj = next.get_next()) != nullptr) {
		obj->paint();
		if (relight) {
			Relight::stamp_object(obj);
		}
	}
}

/*
 *  Paint a chunk's objects, left-to-right, top-to-bottom.
 *
 *  Output: # light sources found.
 */

int Game_render::paint_chunk_objects(
		int cx, int cy    // Chunk coords (0 - 12*16).
) {
	Game_object*      obj;
	Game_window*      gwin          = Game_window::get_instance();
	Map_chunk*        olist         = gwin->map->get_chunk(cx, cy);
	int               light_sources = 0;    // Also check for light sources.
	Main_actor* const main_actor    = gwin->get_main_actor();
	// Measure light from whoever the view follows (see paint()).
	const Actor* viewer = gwin->get_camera_actor() ? gwin->get_camera_actor() : main_actor;
	if (viewer != nullptr) {
		const auto& lights = gwin->is_in_dungeon() ? olist->get_dungeon_lights() : olist->get_non_dungeon_lights();
		for (const auto& light_obj : lights) {
			const Shape_info& info = light_obj->get_info();
			if (info.get_object_light(light_obj->get_framenum()) > 0) {
				// Count light sources.
				light_sources += get_light_strength(light_obj, viewer);
			}
		}
	}
	skip = gwin->get_render_skip_lift();
	Nonflat_object_iterator next(olist);

	while ((obj = next.get_next()) != nullptr) {
		if (obj->render_seq != render_seq) {
			paint_object(obj);
		}
	}

	skip = 255;    // Back to a safe #.
	return light_sources;
}

/*
 *  Render an object after first rendering any that it depends on.
 */

void Game_render::paint_object(Game_object* obj) {
	const int lift = obj->get_lift();
	if (lift >= skip) {
		return;
	}
	obj->render_seq                          = render_seq;
	const Game_object::Game_object_set& deps = obj->get_dependencies();
	for (auto* dep : deps) {
		if (dep && dep->render_seq != render_seq) {
			paint_object(dep);
		}
	}
	int bbox_x, bbox_y;
	Game_window::get_instance()->get_shape_location(obj, bbox_x, bbox_y);

	// paint bbox back
	if (bbox_palindex != -1) {
		obj->get_info().paint_bbox(
				bbox_x, bbox_y, obj->get_framenum(), Game_window::get_instance()->get_win()->get_ib8(), bbox_palindex, 2);
	}
	obj->paint();    // Finally, paint this one.
	if (Relight::active()) {
		Relight::stamp_object(obj);
	}
	// paint bbox front
	if (bbox_palindex != -1) {
		obj->get_info().paint_bbox(
				bbox_x, bbox_y, obj->get_framenum(), Game_window::get_instance()->get_win()->get_ib8(), bbox_palindex, 1);
	}
}

/*
 *  Paint 'dirty' rectangle.
 */

void Game_window::paint_dirty() {
	effects->update_dirty_text();

	TileRect box = clip_to_win(dirty);
	if (box.w > 0 && box.h > 0) {
		paint(box);    // (Could create new dirty rects.)
	}
	clear_dirty();
}

/*
 *  Dungeon Blacking
 *
 *  This is really simple. If there is a dungeon roof over our head we
 *  black out every tile on screen that doens't have a roof at the height
 *  of the roof that is directly over our head. The tiles are blacked out
 *  at the height of the the roof.
 *
 *  I've done some simple optimizations. Generally all the blackness will
 *  cover entire chunks. So, instead of drawing each tile individually, I
 *  work out home many tiles in a row that need to be blacked out, and then
 *  black them all out at the same time.
 */

void Game_render::paint_blackness(int start_chunkx, int start_chunky, int stop_chunkx, int stop_chunky, int index) {
	Game_window* gwin = Game_window::get_instance();
	// Calculate the offset due to the lift (4x the lift).
	const int off = gwin->in_dungeon << 2;

	// For each chunk that might be renderable
	for (int cy = start_chunky; cy != stop_chunky; cy = INCR_CHUNK(cy)) {
		for (int cx = start_chunkx; cx != stop_chunkx; cx = INCR_CHUNK(cx)) {
			// Coord of the left edge
			const int xoff = Figure_screen_offset(cx, gwin->scrolltx) - off - gwin->get_scrolltx_lo();
			// Coord of the top edge
			int y = Figure_screen_offset(cy, gwin->scrollty) - off - gwin->get_scrollty_lo();

			// Need the chunk cache (needs to be setup!)
			Map_chunk* mc = gwin->map->get_chunk(cx, cy);
			if (!mc->has_dungeon()) {
				gwin->win->fill8(index, c_tilesize * c_tiles_per_chunk, c_tilesize * c_tiles_per_chunk, xoff, y);
				continue;
			}
			// For each line in the chunk
			for (int tiley = 0; tiley < c_tiles_per_chunk; tiley++) {
				// Start and width of the area to black out
				int x = xoff;
				int w = 0;

				// For each tile in the line
				for (int tilex = 0; tilex < c_tiles_per_chunk; tilex++) {
					// If the tile is blocked by 'roof'
					if (!mc->is_dungeon(tilex, tiley)) {
						// Add to the width of the area
						w += c_tilesize;
					}
					// If not blocked and have area,
					else if (w) {
						// Draw blackness
						gwin->win->fill8(index, w, c_tilesize, x, y);

						// Set the start of the area to the next tile
						x += w + c_tilesize;

						// Clear the width
						w = 0;
					}
					// Not blocked, and no area
					else {
						// Increment the start of the area to the next tile
						x += c_tilesize;
					}
				}

				// If we have an area, paint it.
				if (w) {
					gwin->win->fill8(index, w, c_tilesize, x, y);
				}

				// Increment the y coord for the next line
				y += c_tilesize;
			}
		}
	}
}
