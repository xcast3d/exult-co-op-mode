/*
 *  Quest of the Companions updates on Windows, through WinSparkle.
 *  See qotcupdates.h and docs/friend-mode/updates.md.
 *
 *  An update is our installer (win32/qotc_installer.iss), run quietly over
 *  this install.  Saves and settings are in %LOCALAPPDATA%\Exult, apart from
 *  the install folder, except in portable mode, so updates are off there.
 */

#include "qotcupdates.h"

#include "qotcversion.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <windows.h>
#include <winsparkle.h>

#include <cstdlib>
#include <string>

namespace {
// What every build checks; QOTC_UPDATE_FEED replaces it, for trying an
// update against a local test feed (docs/friend-mode/updates.md).
constexpr const char* feed_url = "https://downloads.xcast3d.com/u7-qotc/appcast-windows.xml";
// The public half of the key that signs updates (the Mac's SUPublicEDKey).
constexpr const char* public_key = "0yHVSoBe8ELk7KcI/CkUiW4dHvXbtCVbbor4rA99/4w=";
// The installer waits for this to go before it replaces files.
constexpr const wchar_t* running_mutex = L"QuestOfTheCompanionsRunning";

bool started = false;

std::wstring wide(const std::string& text) {
	return std::wstring(text.begin(), text.end());    // ASCII only.
}

// Called from WinSparkle's thread once the installer is ready: quit as the
// window's close button does, so the player is asked first.
int __cdecl can_shutdown() {
	return 1;
}

void __cdecl shutdown_request() {
	SDL_Event event{};
	event.type = SDL_EVENT_QUIT;
	SDL_PushEvent(&event);    // Safe from any thread.
}
}    // namespace

void QOTC_start_updates() {
	if (started) {
		return;
	}
	CreateMutexW(nullptr, FALSE, running_mutex);    // Held until we exit.
	if (get_system_path("<HOME>") == "." || QOTC_build() <= 0) {
		return;    // Portable mode, or a build with no number to compare.
	}
	const char* feed = std::getenv("QOTC_UPDATE_FEED");
	win_sparkle_set_appcast_url(feed != nullptr && *feed != '\0' ? feed : feed_url);
	win_sparkle_set_eddsa_public_key(public_key);
	win_sparkle_set_app_details(L"XCAST3D", wide(QOTC_NAME).c_str(), wide(QOTC_VERSION).c_str());
	// The build number is what's compared with the feed's sparkle:version.
	win_sparkle_set_app_build_version(std::to_wstring(QOTC_build()).c_str());
	win_sparkle_set_automatic_check_for_updates(1);    // Daily, without asking first.
	win_sparkle_set_can_shutdown_callback(can_shutdown);
	win_sparkle_set_shutdown_request_callback(shutdown_request);
	win_sparkle_init();
	std::atexit(win_sparkle_cleanup);
	started = true;
}

bool QOTC_updates_on() {
	return started;
}

void QOTC_check_for_updates() {
	if (started) {
		win_sparkle_check_update_with_ui();
	}
}
