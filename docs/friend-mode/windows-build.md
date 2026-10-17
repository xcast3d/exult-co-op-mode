# Windows build plan

Goal: a Windows Exult with Friend Mode that joins (and hosts) games with the
Mac build, so a Mac and a Windows player can play together. This is part of
Phase 7 in the [project brief](../ultima-7-friend-mode.md), brought forward.

## Where things stand

Upstream Exult already builds on Windows two ways, and both are kept up to
date by the Exult team:

- **MinGW (MSYS2)** with `Makefile.mingw`, which takes its object list from
  `Makefile.common` and links `ws2_32` (Windows sockets). Upstream CI:
  `.github/workflows/ci-windows.yml`.
- **Visual Studio 2019+** with `msvcstuff/vs2019/Exult.vcxproj`, which also
  links `Ws2_32.lib`. Upstream CI: `.github/workflows/ci-msvc.yml`.

Friend Mode's Windows networking in `friendnet.cc`:

1. **Done: Windows sockets.** `winsock2.h`, `closesocket` and `ioctlsocket`;
   `WSAStartup` runs once before the first host or join (`start_sockets`),
   with `WSACleanup` at exit.
2. **Done: build files.** `friendnet.o` is in `Makefile.common` (used by
   `Makefile.mingw`), and `friendnet.cc` / `friendnet.h` are in
   `msvcstuff/vs2019/Exult.vcxproj` and its `.filters`.
3. **Done: errors.** "Would block" checks `WSAEWOULDBLOCK` on Windows;
   `ignore_broken_pipe` does nothing there (Windows has no `SIGPIPE`).
4. **To check: paths and the guest profile.** The one-machine test profile
   (`.friend-mode-local/guest.cfg`) is Mac-specific; a Windows tester uses
   the normal config. Check that the world-download temp file and gamedat
   paths work with Windows paths.
5. **To check: firewall.** Windows Defender asks to allow Exult on first
   host; note it in [internet play](internet-play.md).

## How it is built: GitHub Actions

Mac builds stay local (`make -j8`). Windows builds come from GitHub Actions
on a **private** GitHub repository that mirrors this branch. Bitbucket stays
`origin`; GitHub is a second remote named `github`:

```bash
git push github feature/friend-mode
```

Pushing does **not** build: to save Actions minutes,
`.github/workflows/friend-mode-windows.yml` runs only when started by hand,
at milestones worth testing on Windows. Push first, then either open the
repository on GitHub, **Actions → friend-mode-windows → Run workflow**
(branch `feature/friend-mode`), or from the Mac:

```bash
gh workflow run friend-mode-windows.yml -R xcast3d/exult-co-op-mode --ref feature/friend-mode
```

The workflow uses MSYS2 UCRT64 (64-bit Windows 10 and 11, including Windows on
Arm through emulation) and `Makefile.mingw`, like upstream's
`ci-windows.yml`, but builds only `Exult.exe` (no Exult Studio, tools or
installer) to save minutes. A run takes about 15–25 minutes, most of it
installing MSYS2 packages. Starting a new run cancels one still going.

### Downloading the build

1. On GitHub, open the repository's **Actions** tab, then the latest green
   **friend-mode-windows** run.
2. Under **Artifacts**, download `exult-friend-mode-windows-<commit>` (you
   must be signed in). It is a zip, kept for 14 days.
3. Unzip it anywhere. The `Exult` folder has `Exult.exe`, the DLLs it
   needs, `data/` (Exult's own data, not Ultima VII), `COPYING.txt`,
   `README.windows.txt`, these Friend Mode notes and `FRIEND-MODE-COMMIT.txt`
   (the commit it was built from).
4. Add the game: copy the `STATIC` folder of your own Ultima VII into
   `Exult\blackgate\` (Serpent Isle into `Exult\serpentisle\`), as
   `README.windows.txt` says, then run `Exult.exe`. Game files are never
   in the zip.

If a run fails, open it and read the **Build Exult** step; `stdout.txt` and
`stderr.txt` next to `Exult.exe` help when the game itself misbehaves.

### Actions minutes

GitHub Free gives a private repository 2,000 minutes a month; Windows
minutes count double, so that is roughly 40–60 Windows builds; builds are
manual for that reason. Upstream Exult's own workflows (Linux, macOS, Android, iOS, snapshots, CodeQL,
Coverity and so on) stay in `.github/workflows/` for easy upstream merges,
but each job has `if: ${{ github.repository_owner == 'exult' }}` (some had
it already), so they are skipped here and cost nothing. When merging from
upstream, keep that line on any job upstream adds or changes.

### Other options

- **A Windows PC or VM with MSYS2.** Follow Exult's `README.windows` (its
  build section), then `make -f Makefile.mingw`. Useful for debugging.
- **Cross-compiling from the Mac** with MinGW needs SDL3 and the other
  libraries built for Windows; not worth it while Actions works.

## Testing

- Windows guest joins a Mac host on the same network, then the other way.
- Run the [test checklist](test-checklist.md) sections that send the most
  over the network: join, items, combat, journal, music.
- Then over the internet ([internet play](internet-play.md)).

**Exit criterion:** a Mac and a Windows player finish the checklist
together.
