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

To launch The Black Gate after configuring a valid game-data path in
`~/Library/Preferences/exult.cfg`:

```sh
./exult --bg
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
