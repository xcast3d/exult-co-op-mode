#ifndef QOTCUPDATES_H
#define QOTCUPDATES_H

/*
 *  Quest of the Companions updates: a daily check of our feed, and "Check
 *  for Updates" in the game menu.  Through Sparkle on macOS
 *  (macosx/qotcupdates.mm, built when configure finds Sparkle: QOTC_SPARKLE)
 *  and WinSparkle on Windows (win32/qotcupdates.cc, built when
 *  Makefile.mingw finds WinSparkle: QOTC_WINSPARKLE).  See
 *  docs/friend-mode/updates.md.
 */

#if defined(QOTC_SPARKLE) || defined(QOTC_WINSPARKLE)
#	define QOTC_UPDATES

// Call once SDL's video is up.  Does nothing where updates can't work: on
// macOS outside an app bundle with the Sparkle keys (./exult in the build
// folder), on Windows in portable mode (-p), whose saves are in the folder
// an update replaces.
void QOTC_start_updates();

// Whether updates started, so "Check for Updates" can be offered.
bool QOTC_updates_on();

// Check now, showing the updater's window either way.
void QOTC_check_for_updates();
#endif

#endif
