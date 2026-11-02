#ifndef QOTCUPDATES_H
#define QOTCUPDATES_H

/*
 *  Quest of the Companions updates on macOS, through Sparkle: a daily check
 *  of the feed in Info.plist (SUFeedURL), and "Check for Updates..." in the
 *  app menu.  Built only when configure finds Sparkle (QOTC_SPARKLE); see
 *  docs/friend-mode/updates.md.
 */

// Call once SDL's video is up (SDL makes the app menu then).  Does nothing
// outside an app bundle that has the Sparkle keys, e.g. ./exult in the
// build folder.
void QOTC_start_updates();

#endif
