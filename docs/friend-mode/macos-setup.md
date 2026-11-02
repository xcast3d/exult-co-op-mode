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
game-data path in `~/Library/Preferences/exult.cfg`:

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
origin    Bitbucket project repository
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

How the 2026-10-26 release (`Exult-Friend-Mode-macOS-arm64-617fa85.dmg`)
was made. Development builds use plain `./configure`, which has no
optimization; a release needs it:

```sh
tools/friend-mode/get-sparkle.sh           # once: the updater (docs: updates.md)
./configure --with-optimization=normal     # -O2
make clean && make -j8
rm -rf Exult.app && make bundle_shared      # the app, with Homebrew's libraries copied in (dylibbundler)
for f in Exult.app/Contents/Resources/lib/*.dylib; do codesign --force --sign - "$f"; done
codesign --force --sign - Exult.app         # ad-hoc: each library first, then the app
codesign --verify --deep --strict Exult.app
```

Sign every library by itself: `dylibbundler` rewrites their paths, which
breaks their signatures, and `codesign --deep` doesn't reach
`Contents/Resources/lib`; an unsigned library gets the app killed at launch
("Code Signature Invalid"). Check nothing still points at Homebrew:
`otool -L` on the app and each library shouldn't list `/opt/homebrew`.
Sparkle.framework (in `Contents/Frameworks`) keeps its own signature; signing
the app without `--deep` leaves it alone. To put the build online, and in
the update feed, see [updates](updates.md#releasing-a-mac-build).

Then a disk image with `hdiutil`: a folder with `Exult.app`, a link to
`/Applications`, the Friend Mode `README.md`, `README.macOS`, `COPYING` and
the commit, made with
`hdiutil create -volname "Exult Friend Mode" -srcfolder <folder> -format UDZO <name>.dmg`.
(`make osxdmg` is upstream's "Git Snapshot" image and needs `create-dmg`.)

The build is arm64 only (Homebrew's libraries are), and ad-hoc signed, not
notarized: on first launch players right-click → Open, or allow it under
System Settings → Privacy & Security. Notarizing needs an Apple Developer
ID. The Windows release is the `friend-mode-windows` workflow's artifact
([Windows build](windows-build.md)).
