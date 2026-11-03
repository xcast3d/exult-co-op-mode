# Releasing

The whole release, start to finish: a Mac and a Windows build of one
commit, put online at `downloads.xcast3d.com/u7-qotc/`, where installed
builds find them as updates. The details are in the pages linked from each
step.

## How players get builds

- **Fresh install:** from the website's download buttons, which should
  point at `https://downloads.xcast3d.com/u7-qotc/latest/macos` and
  `.../latest/windows`. Those always redirect to the newest build, so the
  buttons never need editing. (Not yet checked, 2026-11-02: what the
  site's buttons point at now.)
- **Updates:** a build with the updater checks its feed daily and offers
  newer builds; the game menu's **Check Updates** checks at once (and, on a
  Mac, the app menu's Check for Updates…). Mac: Sparkle replaces
  `Exult.app`. Windows: WinSparkle runs our installer quietly over the
  install and starts the game again. Saves and settings are outside both
  and stay ([updates](updates.md)).
- **The first updating release is 0.8.2.** Builds up to 0.8.1 (`d2597e4`)
  have no updater: testers download 0.8.2 by hand, once (the Mac dmg, or
  the Windows installer, which replaces the zipped folder; they can install
  into that folder to keep their game data). Every release after that
  reaches them by itself.

Commits alone reach no one: only a deploy does. Ordinary work carries on
as usual between releases.

## Steps

1. **Ready the commit** ([Before a
   release](macos-setup.md#before-a-release-mac-and-windows)):
   - no test kits (`grep -n TEMPORARY exult.cc`; `FRIEND_TEST_KEYS` off);
   - `QOTC_VERSION` in `qotcversion.h` set, e.g. `0.8.2`;
   - in CHANGELOG.md, **Unreleased** moved under that version, with the
     date, build number and commit. Its newest release section is exactly
     what the update window shows (`tools/downloads/release-notes.py`),
     so write it for players.

   Commit, with nothing left uncommitted: the build number counts commits,
   and both builds must come from this commit.
2. **Tag and push:** `git tag -a qotc-v<version> -m
   "Quest of the Companions <version> (build <n>)"`, then
   `git push origin feature/friend-mode qotc-v<version>`. The tag is
   where each build's source is found (GPL); `qotc-` keeps it apart from
   Exult's own `v…` tags.
3. **Windows build** on GitHub Actions ([Windows build](windows-build.md)):
   ```sh
   gh workflow run friend-mode-windows.yml -R xcast3d/exult-co-op-mode --ref feature/friend-mode
   gh run watch <run id> -R xcast3d/exult-co-op-mode --exit-status
   gh run download <run id> -R xcast3d/exult-co-op-mode -n exult-friend-mode-windows-setup-<commit>
   ```
   That gives `Exult-U7-QOTC-Windows-x64-<commit>-setup.exe` (about
   16 MB). About 13 minutes.
4. **Mac build** here, meanwhile, from the same commit ([The Mac
   build](macos-setup.md#the-mac-build)): `get-sparkle.sh` once, configure
   with `--with-optimization=normal
   --with-qotc-services=tools/friend-mode/qotc-release.env` (our updater),
   `make bundle_shared`, sign each library then the app, and the dmg named
   `Exult-U7-QOTC-macOS-arm64-<commit>.dmg`. Check `CFBundleVersion` in
   its Info.plist is the new build number, and `SUFeedURL` is there.
5. **Try both** before they go online: install, start, play a little.
6. **Deploy** (needs wrangler logged in, and the update key in
   `~/.u7-qotc/sparkle-private-key`):
   ```sh
   tools/downloads/deploy.sh Exult-U7-QOTC-macOS-arm64-<commit>.dmg Exult-U7-QOTC-Windows-x64-<commit>-setup.exe
   ```
   It signs both, writes `appcast-macos.xml` and `appcast-windows.xml`
   with the release notes, refuses a build number that isn't higher than
   the feed's, and points `latest/macos` and `latest/windows` at the new
   builds. Run it from the repository (it reads the commit's CHANGELOG.md
   and version), and never run wrangler anywhere but `tools/downloads/`.
7. **Check online:** `latest/macos` and `latest/windows` redirect to the
   new files; both feeds show the new build; the lobby still answers
   (`tools/lobby` smoke test). An installed earlier build, with Check
   Updates, offers it.

## Trying an update before a release

Without touching the real feeds: put a newer build and a signed feed under
`tools/downloads/public/u7-qotc/test/` (git-ignored), deploy with
`npx wrangler deploy` in `tools/downloads/`, and start the installed
build with `QOTC_UPDATE_FEED` set to that feed. Remove the folder and
deploy again afterwards. Done for Windows on 2026-11-02 (236 to 237, on a
PC: it offered the update at launch, closed the game, installed and
restarted). Windows, from a Command Prompt:

```
cd /d "%LOCALAPPDATA%\Programs\Quest of the Companions"
set QOTC_UPDATE_FEED=https://downloads.xcast3d.com/u7-qotc/test/appcast-windows.xml
Exult.exe
```

On a Mac: `QOTC_UPDATE_FEED=<url> Exult.app/Contents/MacOS/exult`. The Mac's
interactive path (Check for Updates → Install and Relaunch) hasn't been
tried yet, only its install on quit ([updates](updates.md#trying-an-update-locally)).

## Keep safe

- The update key's private half (`~/.u7-qotc/sparkle-private-key`, and the
  login keychain account `u7-qotc`): lose it and no installed build can
  take an update again. Keep a copy in a password manager. Never in the
  repository.
- The feed URLs, the update key and the installer's `AppId`
  (`tools/friend-mode/qotc-release.env`) are built into every install:
  never change them.
