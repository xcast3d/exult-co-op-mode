# Linux support

Status (2026-11-20): **not started**. A redditor asked for Linux, ideally
a Flatpak for the Steam Deck. These are notes for when we take it on.

## Today: Steam Deck through Proton

The Windows installer should run on a Steam Deck (or any Linux with
Steam) when it's added as a non-Steam game and run with Proton. Not
tested yet. The parts most likely to break are Friend Mode's networking
(direct TCP and the WebRTC lobby codes) and WinSparkle's update
check. If a player tries it, ask how online play went.

## What a native build needs

Exult is native on Linux already (upstream's `ci-linux.yml`). What
Friend Mode adds looks portable:

- **Networking:** `friendtcp.cc` and `friendrtc.cc` only split
  `_WIN32` from everything else. Linux takes the POSIX path macOS uses.
- **WebRTC libraries:** `tools/friend-mode/build-rtc.sh` already works
  out the Linux paths (`uname` for `third_party/build/linux-<cpu>/`,
  `nproc` for the core count). It needs CMake.
- **Updates:** no Linux version. Sparkle (macOS) and WinSparkle (Windows)
  are the only updaters, and `configure` only enables Sparkle on macOS. A
  Linux build ships without update checks. A Flatpak gets its updates
  from Flathub instead. An AppImage could later use AppImageUpdate.
- **Our own folder:** saves and `exult.cfg` live in a "Quest of the
  Companions" folder on Mac and Windows (see
  [updates](updates.md#our-own-folder)). Linux should follow the XDG
  layout, probably `~/.local/share/Quest of the Companions` (or a
  name without spaces). Decide this before the first release so it
  never has to move.

## Plan

1. **Build.** Add a Linux job to GitHub Actions on an Ubuntu runner,
   started by hand like `friend-mode-windows.yml`. Install the SDL and
   build dependencies, run `build-rtc.sh`, then `configure` and `make`.
2. **Test on the Raspberry Pi.** Raspberry Pi OS is a real Linux. It's
   ARM, where the Deck is x86-64, but it's the same code, so it should
   catch nearly every Linux problem. Play a Friend Mode session against
   the Mac (both direct and through a lobby code) and run the
   [test checklist](test-checklist.md).
3. **AppImage first.** It's one file, easy to add to Steam as a
   non-Steam game, and needs no store review. Attach it to the GitHub
   release next to the Mac and Windows downloads.
4. **Flatpak later.** It needs a manifest, a filesystem permission so
   the game can find the player's own Ultima VII files, and Flathub's
   review to be listed there. Check whether upstream Exult already has
   a Flathub package we could learn from.
5. **Find a Deck tester.** We don't have a Steam Deck. Ask the redditor
   or another Deck owner to try the AppImage, including controller
   play (see the [controller plan](controller-plan.md)).

## Open questions

- Does Friend Mode's WebRTC work under Proton at all? That decides how
  much the native build matters for the Deck.
- Folder name on Linux: with spaces, or without?
- Build on an older Ubuntu so the AppImage runs on more systems?
