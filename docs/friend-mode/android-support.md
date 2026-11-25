# Android support

Status (2026-11-25): **not started**. A redditor asked about an Android
port. These are notes for when we take it on; nothing has been built for
Android yet.

## What Exult already has

Exult runs on Android, and the port is in this repository (`android/`,
[`android/README.md`](../../android/README.md)):

- An APK that players install by hand, from exult.info. It isn't in any
  app store.
- A launcher (Java, `android/app/src/main/java/info/exult/`) that installs
  the game files from GOG's Mac `.pkg` or from zips, plus music packs and
  mods, and imports and exports saves.
- The game itself is the same Exult code: `configure --enable-libexult`
  for the Android NDK
  (`android/app/src/main/cpp/dependencies/exult/CMakeLists.txt.in`),
  with SDL, Ogg/Vorbis, FluidSynth and MT32emu built alongside, and a
  small wrapper around it. Exult's own touch controls are in
  `android/lib/src/TouchUI_Android.cc`.
- Upstream's `ci-android.yml` builds a debug APK. On our fork it's
  skipped, like upstream's other workflows (Actions builds are manual).

## What we'd need

From easiest to hardest:

1. **Building it.** Probably straightforward: the Android build compiles
   Exult with the same makefiles (`Makefile.am`) our Friend Mode code is
   already in, so it comes along. Expect some compile fixes.
2. **Our own app.** Like the Mac app (`com.xcast3d.qotc`), it needs its
   own package id, name and icon instead of Exult's (`info.exult`), so it
   installs next to Exult's Android app instead of replacing it. Check
   where it keeps its files, so they don't mix with Exult's either.
3. **Playing with a controller.** Likely works almost as is: SDL supports
   Bluetooth and USB controllers on Android, and the Target System, aimed
   drops and co-op menus already work on a pad
   ([controller plan](controller-plan.md)). A tablet with a controller is
   the realistic first target.
4. **Co-op.**
   - Direct IP (port 1992) should work on Wi-Fi: `friendtcp.cc` only
     splits `_WIN32` from everything else, and Android takes the POSIX
     path.
   - Lobby codes need the WebRTC libraries (libdatachannel, Mbed TLS)
     built for Android: `tools/friend-mode/build-rtc.sh` with the NDK's
     CMake toolchain, for arm64-v8a at least. A moderate, known job.
   - Phones on mobile data are behind carrier NAT: lobby codes, not
     direct IP, would be the way to play there.
5. **Touch controls.** The big one. Our additions assume a keyboard or a
   controller: the Target System, aimed drops, co-op menus, Party order
   (Shift+O) and the Ctrl shortcuts. Making them usable on a phone's
   touch screen is real design work. The browser version turns phones
   away for the same reason ([web browser](web-browser.md)).
6. **Updates.** No Sparkle on Android. Options: players download new
   APKs by hand (as Exult does), a simple in-game "new version" check
   against our feed, or a Play Store listing (a developer account, a
   signing key kept like the Sparkle key, store review, and Google's
   rules on what an app may download).

## Plan

1. **Spike.** Build a debug APK from `feature/friend-mode` on a branch of
   its own (following `android/README.md`), leaving lobby codes out. Run
   it in the Android emulator on the Mac if it can: the launcher,
   installing Black Gate from a zip, starting a game, a controller if
   the emulator passes one through, and direct-IP co-op against the
   desktop build. Write down what broke and what ran.
2. **Our own identity.** Package id, name, icon and folders, as above.
3. **Lobby codes.** WebRTC libraries for the NDK.
4. **A tablet tester.** We'd need someone with an Android tablet and a
   controller to try a sideloaded APK.
5. **Touch controls**, only if there's demand beyond tablets with
   controllers.

## Open questions

- Phones or tablets first? Phones need touch controls before they're
  playable; tablets with a controller may not.
- Sideloaded APK only (like Exult), or the Play Store too?
- Which Android versions? Exult's port asks for Android 9 (`minSdkVersion
  28`).
- Do upstream's touch controls (`TouchUI_Android.cc`) get in the way of
  our own keys and menus?
