#ifndef QOTCSERVICES_H
#define QOTCSERVICES_H

/*
 *  The internet services a Quest of the Companions build talks to, all
 *  chosen when building, so another distributor's builds use their own.
 *  Our release builds take ours from tools/friend-mode/qotc-release.env
 *  (configure --with-qotc-services=<file> on macOS, QOTC_SERVICES=<file>
 *  with Makefile.mingw); see docs/friend-mode/updates.md.
 *
 *  - QOTC_LOBBY_URL: the lobby that hands out codes (tools/lobby).  Ours
 *    unless the build names another, or "off" for none (joining by address
 *    only).  Players can still change it in exult.cfg
 *    (config/friend/lobby_url).
 *  - The update feeds and the public key that checks updates: no default.
 *    A build without them has no updater (QOTC_UPDATES in qotcupdates.h),
 *    so builds from the source never check anyone's feed by accident.  On
 *    macOS they go into Info.plist (SUFeedURL, SUPublicEDKey; macosx.am),
 *    on Windows into win32/qotcupdates.cc (QOTC_FEED_WINDOWS,
 *    QOTC_UPDATE_KEY).
 */

#ifndef QOTC_LOBBY_URL
#	define QOTC_LOBBY_URL "wss://u7-multiplayer-lobby.xcast3d.com"
#endif

#endif
