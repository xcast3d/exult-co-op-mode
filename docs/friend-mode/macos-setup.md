# macOS development setup

These instructions reproduce the initial Friend Mode development baseline on an
Apple Silicon Mac using Homebrew.

## Prerequisites

- Xcode with its command-line tools selected.
- Homebrew.
- A legitimate copy of the Ultima VII game data. Never commit game data or
  personal save files.

Install the build dependencies:

```sh
brew install autoconf automake libtool autoconf-archive pkgconf libpng \
  libvorbis fluidsynth mt32emu
```

Generate the build system and compile:

```sh
autoreconf -v -i
./configure
make -j4
```

Confirm that the binary starts:

```sh
./exult --version
./exult --help
```

The initial baseline used Exult `1.13.1git`, Apple Clang 21, SDL 3.4.18,
libpng 1.6.59, Vorbis 1.3.7, FluidSynth 2.6.1, and MT32emu 2.8.3.

To launch the unmodified Black Gate baseline after configuring a valid
game-data path in `~/Library/Application Support/Quest of the Companions/exult.cfg`
(Exult's own settings, `~/Library/Preferences/exult.cfg`, are copied there
on the first launch; [updates](updates.md#our-own-folder)):

```sh
./exult --bg
```

After changing generated resources such as the default key bindings, use the
development-only checksum override:

```sh
./exult --bg --nocrc
```

Exult's full platform notes remain in [`README.macOS`](../../README.macOS).

## Git remotes

The intended layout is:

```text
origin    Project repository on GitHub (xcast3d/exult-co-op-mode)
upstream  Official Exult GitHub repository
```

Bring in upstream changes deliberately:

```sh
git fetch upstream
git rebase upstream/master
```

Friend Mode work belongs on `feature/friend-mode`; avoid mixing upstream-only
baseline commits with the experimental engine changes.

## Release build (a disk image to share)

### Before a release (Mac and Windows)

The whole release, both builds and the deploy, step by step:
[releasing](releasing.md).

1. **No test kits.** Release builds have no test keys or spawners:
   - Remove any temporary test code: `grep -n TEMPORARY exult.cc`.
     (The ungated F9 enemy spawner and F7 bedroll were taken out for
     0.8.1; the spawner is back inside the `FRIEND_TEST_KEYS` block.)
   - Don't define `FRIEND_TEST_KEYS` (plain `./configure`, and the Windows
     workflow, leave it off). The host's F8 / F9 test keys are then left out
     of the build entirely.
   - Check: in the release build, F7 and F9 do nothing.
2. **The version.** Set `QOTC_VERSION` in `qotcversion.h` and move
   CHANGELOG.md's **Unreleased** section under that version, with the date,
   build number and commit ([CHANGELOG.md](../../CHANGELOG.md) says how
   versions go up). `./exult --version` shows both.
3. Build from a commit, with nothing uncommitted, so the build number and
   the commit in the release's name match what is in it.

### The Mac build

One script, from a commit:

```sh
tools/friend-mode/get-sparkle.sh          # once: the updater (docs: updates.md)
tools/friend-mode/build-macos-deps.sh     # once: the libraries, for macOS 12
tools/friend-mode/make-macos-release.sh
```

That gives `Exult-U7-QOTC-macOS-arm64-<commit>.dmg` holding
`Quest of the Companions.app`, a link to `/Applications`, `README.md`,
`README.macOS`, `COPYING` and the commit.

**Why its own libraries.** Homebrew builds its libraries for the macOS it
runs on, so 0.8.10, made with them, only started on macOS 27 (older Macs
showed a stop sign on the app in the disk image). `build-macos-deps.sh`
builds the libraries the game bundles (SDL3, libpng, Ogg/Vorbis, FLAC,
Opus, libsndfile, FluidSynth, MT32emu) from their sources, pinned to
Homebrew's versions and checksums, plus the WebRTC libraries, all for
macOS 12 (`QOTC_MACOS_MIN`; 12.0 is the lowest Sparkle supports), into
`third_party/build/macos12-arm64/` (git-ignored). Only FluidSynth's
Core Audio and SoundFont (`.sf2`/`.sf3`) parts are built.

**What the release script does:**
- configures with `--with-optimization=normal` (-O2),
  `--with-qotc-services=tools/friend-mode/qotc-release.env` (our updater:
  without it deploy.sh refuses the app; [updates](updates.md#the-pieces))
  and those libraries only (`PKG_CONFIG_LIBDIR`, `--with-friend-rtc`),
  with `MACOSX_DEPLOYMENT_TARGET=12.0`; it stops if Homebrew's crept in;
- `make clean`, `make`, `make bundle_shared` (dylibbundler copies the
  libraries in), and renames `Exult.app` to `Quest of the Companions.app`.
  A build with the updater has our bundle ID, `com.xcast3d.qotc`, and
  name (`macosx/macosx.am`), so it never replaces or passes for Exult's
  own app (`info.exult`); the script adds `LSMinimumSystemVersion`;
- signs ad-hoc, each library by itself and then the app: `dylibbundler`
  rewrites the libraries' paths, which breaks their signatures, and
  `codesign --deep` doesn't reach `Contents/Resources/lib`; an unsigned
  library gets the app killed at launch ("Code Signature Invalid").
  Sparkle.framework keeps its own signature;
- checks every binary in the app: nothing links to Homebrew or
  `third_party/build`, and none needs a macOS newer than 12;
- makes the disk image with `hdiutil`.

It leaves the tree configured for the release; plain `./configure` goes
back to a development build. To put the build online, and in the update
feed, see [updates](updates.md#releasing-a-mac-build).
(`make osxdmg` is upstream's "Git Snapshot" image and needs `create-dmg`.)

The build is arm64 only, for macOS 12 and newer, and ad-hoc signed, not
notarized: on first launch players right-click → Open, or allow it under
System Settings → Privacy & Security. Notarizing needs an Apple Developer
ID. The Windows release is the `friend-mode-windows` workflow's artifact
([Windows build](windows-build.md)).
