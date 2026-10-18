/*
 * SHELVED, not built: Friend Mode's world map on U (2026-10-17).
 *
 * With the current leash, everyone is close to the Avatar, so the corner
 * minimap (friendmap.cc) is enough.  Kept here in case a longer leash or
 * independent exploration brings it back.
 *
 * To bring it back:
 * - Put these functions back in friendmap.cc's anonymous namespace (with
 *   `int terrains_done = 0;` and `bool world_shown = false;`), and
 *   Friend_map_toggle()/Friend_map_shown() outside it, declared in
 *   friendmap.h.
 * - Friend_map_paint(): `if (world_shown) { paint_world_map(dots);
 *   gwin->set_painted(); } else if (Friend_minimap_enabled) ...`
 * - Friend_map_dot needs `std::string label;` ("Marc (Iolo)"), filled in
 *   by Friend_mode_paint_maps() in exult.cc:
 *   `name.empty() ? character : name + " (" + character + ")"`, where
 *   character is "Avatar" or actor->get_name().
 * - In exult.cc, skip the leash arrow while Friend_map_shown(), and in
 *   Handle_event(), first thing:
 *
 *	if (!friend_chat_active && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_U && !event.key.repeat
 *		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !Notebook_gump::get_instance()
 *		&& gwin->get_main_actor()) {
 *		Friend_map_toggle();    // U: the world map with every player on it.
 *		return;
 *	}
 */

void paint_world_map(const std::vector<Friend_map_dot>& dots) {
	Game_window*   gwin   = Game_window::get_instance();
	Game_map*      map    = gwin->get_map();
	Image_buffer8* buffer = gwin->get_win()->get_ib8();
	// Spread the first drawing over a few frames rather than stall the game
	// (and the guests waiting on the host).
	if (terrains_done < map->get_num_chunk_terrains()) {
		const int end = std::min(map->get_num_chunk_terrains(), terrains_done + 150);
		for (; terrains_done < end; ++terrains_done) {
			colors_for(terrains_done);
		}
	}
	const int w    = gwin->get_width();
	const int h    = gwin->get_height();
	const int size = std::min(h - 8, w - 8);
	if (size < 32) {
		return;
	}
	const int x0 = (w - size) / 2;
	const int y0 = (h - size) / 2;
	buffer->fill8(0, size + 4, size + 4, x0 - 2, y0 - 2);
	for (int y = 0; y < size; ++y) {
		const int cy = y * c_num_chunks / size;
		for (int x = 0; x < size; ++x) {
			const int             cx     = x * c_num_chunks / size;
			const Terrain_colors& colors = terrain_colors.size() > static_cast<size_t>(map->get_terrain_num(cx, cy))
														   ? terrain_colors[map->get_terrain_num(cx, cy)]
														   : Terrain_colors();
			if (colors.ready) {
				buffer->put_pixel8(colors.whole, x0 + x, y0 + y);
			}
		}
	}
	// The key: each dot's player, top left inside the map.
	Shape_manager* sman   = Shape_manager::get_instance();
	const int      line_h = sman->get_text_height(0) + 1;
	int            key_y  = y0 + 4;
	for (const Friend_map_dot& dot : dots) {
		paint_dot(buffer, x0 + dot.tile.tx * size / c_num_tiles, y0 + dot.tile.ty * size / c_num_tiles, dot.color);
		paint_dot(buffer, x0 + 8, key_y + line_h / 2, dot.color);
		sman->paint_text(0, dot.label.c_str(), x0 + 14, key_y);
		key_y += line_h;
	}
}

void Friend_map_toggle() {
	world_shown = !world_shown;
	Game_window::get_instance()->set_all_dirty();    // Uncover the view.
}

bool Friend_map_shown() {
	return world_shown;
}
