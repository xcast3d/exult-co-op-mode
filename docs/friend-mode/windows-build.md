# Windows build plan

Goal: a Windows Exult with Friend Mode that joins (and hosts) games with the
Mac build, so a Mac and a Windows player can play together. This is part of
Phase 7 in the [project brief](../ultima-7-friend-mode.md), brought forward.

## Where things stand

Upstream Exult already builds on Windows two ways, and both are kept up to
date by the Exult team:

- **MinGW (MSYS2)** through the usual `configure` / `make`, the same build
  system as on the Mac. `configure.ac` already links `ws2_32` (Windows
  sockets). Upstream CI: `.github/workflows/ci-windows.yml`.
- **Visual Studio 2019+** with `msvcstuff/vs2019/Exult.vcxproj`, which also
  links `Ws2_32.lib`. Upstream CI: `.github/workflows/ci-msvc.yml`.

Friend Mode code already has Windows branches in `friendnet.cc`
(`winsock2.h`, `closesocket`, `ioctlsocket`). Not done yet:

1. **`WSAStartup`.** Windows sockets must be started once before use (and
   `WSACleanup` at exit). Without it every socket call fails. Add it in
   `Friend_mode_session::start_host` / `start_guest` (once per process).
2. **Visual Studio project.** `friendnet.cc` / `friendnet.h` are in
   `Makefile.am` but not in `Exult.vcxproj`; add them. (MinGW picks them up
   already.)
3. **Errors and blocking.** Check the Windows paths for "would block"
   (`WSAEWOULDBLOCK` rather than `EWOULDBLOCK`) and broken pipes (no
   `SIGPIPE` on Windows; `ignore_broken_pipe` should do nothing there).
4. **Paths and the guest profile.** The one-machine test profile
   (`.friend-mode-local/guest.cfg`) is Mac-specific; a Windows tester uses
   the normal config. Check that the world-download temp file and gamedat
   paths work with Windows paths.
5. **Firewall.** Windows Defender asks to allow Exult on first host; note
   it in [internet play](internet-play.md).

## How to build it

The repository lives on Bitbucket, so upstream's GitHub workflows don't run
for it. Options, easiest first:

1. **A Windows PC or VM with MSYS2** (UTM or Parallels on Apple silicon
   runs Windows 11 for Arm, which runs x64 builds). Follow Exult's
   `INSTALL` / `README.win32`, then `configure` and `make` as on the Mac.
   Best for the first build and for testing Mac-to-Windows play by hand.
2. **Bitbucket Pipelines** with a Windows runner (or a self-hosted one) to
   build on each push. Later, once the manual build works.
3. **A GitHub mirror** of this branch, which would run the existing
   `ci-windows.yml` / `ci-msvc.yml` as they are. Simplest automation, but
   the code then lives on GitHub too (private repositories work).

Cross-compiling from the Mac with MinGW is possible but needs SDL3 and the
other libraries built for Windows; not worth it before option 1 works.

## Testing

- Windows guest joins a Mac host on the same network, then the other way.
- Run the [test checklist](test-checklist.md) sections that send the most
  over the network: join, items, combat, journal, music.
- Then over the internet ([internet play](internet-play.md)).

**Exit criterion:** a Mac and a Windows player finish the checklist
together.
