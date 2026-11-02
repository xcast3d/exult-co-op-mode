# Updates

Status: **Mac built and tested locally 2026-11-02** (Sparkle 2.10.1); not
in a release yet. **Windows written 2026-11-02** (WinSparkle 0.9.4, an
Inno Setup installer), not yet built or tried. The web build needs none: a
redeploy is the update.

## How it works for players

A Mac build checks
`https://downloads.xcast3d.com/u7-qotc/appcast-macos.xml` once a day
(and when they choose the app menu → **Check for Updates…**). When the
feed has a higher build, Sparkle downloads it, checks its signature and
offers it; installed on quit or with Install and Relaunch.

Only `Exult.app` is replaced. Saves and game data
(`~/Library/Application Support/Exult`) and settings
(`~/Library/Preferences/exult.cfg`) are outside the app and untouched
(checked in the test below).

On Windows the same feed idea, `appcast-windows.xml`: WinSparkle checks it
daily, and the game menu has **Check for Updates** (on both platforms).
Install downloads our installer, checks its signature and runs it quietly
(`/SILENT`): the game asks "really quit?", and once it has closed the
installer replaces the install and starts the game again. It installs where
the player first installed it.

Builds before this one (e.g. `d2597e4`) have no updater: players download
the first updating build by hand, once. On Windows that is the installer,
which replaces the zipped folder.

## The pieces

- **Sparkle**: `tools/friend-mode/get-sparkle.sh` puts a pinned, checksummed
  release in `third_party/build/sparkle/` (git-ignored). `./configure` finds
  it on macOS (`--with-sparkle=DIR`, `--without-sparkle` to leave it out)
  and defines `QOTC_SPARKLE`.
- **The code**: `macosx/qotcupdates.mm`, started from `Init()` in exult.cc
  once SDL's video is up. It does nothing outside an app bundle (plain
  `./exult` in the build folder).
- **The bundle** (`make bundle` / `bundle_shared`, macosx/macosx.am): copies
  Sparkle.framework into `Contents/Frameworks`, and writes into Info.plist:
  - `CFBundleVersion` = the build number (`QOTC_BUILD`), which is what
    Sparkle compares; `CFBundleShortVersionString` = `QOTC_VERSION`;
  - `SUFeedURL`, `SUPublicEDKey`, `SUEnableAutomaticChecks`.
- **The feed**: `tools/downloads/deploy.sh` (see below).

On Windows:

- **WinSparkle**: `tools/friend-mode/get-winsparkle.sh` puts a pinned,
  checksummed release in `third_party/build/winsparkle/` (git-ignored).
  `Makefile.mingw` finds it, defines `QOTC_WINSPARKLE`, links
  `WinSparkle.dll` and installs it next to `Exult.exe`.
- **The code**: `win32/qotcupdates.cc`, from the same place in `Init()`. It
  holds the feed URL and the public key, gives WinSparkle the build number
  to compare, and turns a shutdown request into the window's close (so the
  player is asked). It also holds a named mutex,
  `QuestOfTheCompanionsRunning`, while the game runs. Updates are off in
  portable mode (`-p`) and for a build with no number.
- **The installer**: `win32/qotc_installer.iss` (Inno Setup), made by the
  workflow. Per user (`%LOCALAPPDATA%\Programs\Quest of the Companions` by
  default), no administrator prompt. Its `AppId` must never change: it's
  how an update finds the install. Run silently, it waits up to a minute for
  the mutex to go, then asks to close the game, and starts the game again
  when done. It leaves `blackgate\` and `serpentisle\` (the player's game
  data) alone, through updates and uninstalling.

## The signing key

Every update is signed with an EdDSA key; builds carry its public half
(`QOTC_ED_KEY` in macosx/macosx.am) and refuse anything it didn't sign.
The private half:

- login keychain, account **`u7-qotc`** (made with Sparkle's
  `generate_keys --account u7-qotc`);
- backup and the copy deploy.sh uses: `~/.u7-qotc/sparkle-private-key`
  (mode 600). Reading it from the keychain pops up an "allow access"
  dialog, so the scripts use the file.

Never in the repo. **Losing it means no installed build can ever accept an
update again** (players would have to download by hand), so keep a copy
somewhere safe, e.g. a password manager. For CI later it becomes a
GitHub Actions secret, passed with `--ed-key-file -`.

## Releasing

The usual release build ([macOS setup](macos-setup.md#the-mac-build)),
after `tools/friend-mode/get-sparkle.sh` once, then:

```sh
tools/downloads/deploy.sh Exult-U7-QOTC-macOS-arm64-<commit>.dmg Exult-U7-QOTC-Windows-x64-<commit>-setup.exe
```

The installer is the workflow's `exult-friend-mode-windows-setup-<commit>`
artifact ([Windows build](windows-build.md)).

For a macOS dmg whose app has the updater, deploy.sh zips the app (with
`ditto`, as Finder does: other zippers leave files in Sparkle.framework that
Gatekeeper rejects) as `<name>-update.zip`, signs it, and adds it to
`appcast-macos.xml`. It refuses a build number that isn't higher than every
build already in the feed, before copying anything. The update window links
to the site's changelog.

For a Windows `-setup.exe`, deploy.sh takes the build number and version
from the commit in its name (so that commit must be in this repository),
signs it with Sparkle's `sign_update` (WinSparkle checks the same EdDSA
signatures), and writes `appcast-windows.xml` with it as the one update,
run with `/SILENT /SP- /NOCANCEL /NORESTART`. It too refuses a build that
isn't higher than the feed's. `latest/windows` then points at the
installer.

The build number only goes up with commits, so release from a commit
(nothing uncommitted), as the release steps already say.

## Trying an update locally

`QOTC_UPDATE_FEED=<url>` replaces the feed, for a test without touching the
live one. How it was tested on 2026-11-02:

1. Two copies of `Exult.app` in a scratch folder: "installed" (build 233)
   and "new", with `CFBundleVersion` raised to 234 in its Info.plist and
   re-signed (`codesign --force --sign -`).
2. A feed for the new one, served locally:
   ```sh
   ditto -c -k --sequesterRsrc --keepParent new/Exult.app feed/test234.zip
   (cd feed && generate_appcast . --ed-key-file ~/.u7-qotc/sparkle-private-key \
     --download-url-prefix http://127.0.0.1:8650/ --maximum-deltas 0 -o appcast-macos.xml)
   python3 -m http.server 8650 --bind 127.0.0.1 --directory feed
   ```
3. To skip the wait for the daily check and install without asking:
   `defaults write info.exult SUHasLaunchedBefore -bool YES`,
   `SUAutomaticallyUpdate -bool YES`, and `SULastCheckTime` set to a past
   date. Then run
   `QOTC_UPDATE_FEED=http://127.0.0.1:8650/appcast-macos.xml installed/Exult.app/Contents/MacOS/exult -c <a copy of exult.cfg>`.
4. It fetched the feed and the zip at once; on quit the installed copy
   became build 234, still validly signed, and exult.cfg and the saves were
   unchanged. Afterwards: `defaults delete info.exult` and remove
   `~/Library/Caches/info.exult`.

Not tried yet: the interactive path (Check for Updates… → Install and
Relaunch), Sparkle's windows over a fullscreen game, and a download from
the live site with Gatekeeper's quarantine.

On Windows (not done yet), the same way: install a build, make a second
installer from a later commit, serve a feed for it locally, and run the
installed game with `QOTC_UPDATE_FEED=http://<this PC>:8650/appcast-windows.xml`.
Then the game menu's Check for Updates.

## Windows: where players' data lives

Checked on Windows 11 with the 0.8.1 build (2026-11-02), and in the code
(`Get_home()` in files/utils.cc): everything the game writes goes under
`%LOCALAPPDATA%\Exult`, not the install folder:

- `exult.cfg`, and the logs `stdout.txt` / `stderr.txt`;
- `blackgate\` and `serpentisle\`: saves, `gamedat\` (the current game),
  `gamedat.coop\` (the Friend Mode world) and each mod's `gamedat`/`saves`.

The install folder holds only what we ship (its `gamedat` folders are the
copied game data, untouched by play). So a WinSparkle update can replace
the whole install folder.

The exception is **portable mode**: started with `-p`, Exult keeps
everything in the working folder (`<HOME>` is `.`), usually the install
folder, and an update that replaces it would take the saves with it. (An
`exult.cfg` beside the exe doesn't switch it on; nor does our build define
`PORTABLE_EXULT_WIN32`, which would.) WinSparkle should not run in portable
mode, or should keep the folder's data. A real save (2026-11-02) wrote
`exult00bg.sav` to `%LOCALAPPDATA%\Exult\blackgate\` and nothing to the
install folder.
