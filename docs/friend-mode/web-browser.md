# Playing in a web browser

**Status (2026-10-30):** single player runs in a browser. The page asks
for the player's own Ultima VII folder, Start runs Exult, and a game in
progress survives a reload. Tested only in Chromium (the desktop app's
browser pane) on an Apple Silicon Mac. Background, the spike's findings and
sizes are in the "Exult in the browser" section of the
[project brief](../ultima-7-friend-mode.md).

## How it's built

The browser version uses the same source as the Mac and Windows builds, so
a change to the game is in it too after the next build. Code only the browser
needs goes inside `#ifdef __EMSCRIPTEN__`, or in `web/`:

| File | What it is |
| --- | --- |
| `web/build.sh` | Builds with Emscripten (`brew install emscripten`). Needs this checkout built natively first, for the data files. The first build takes about 3 minutes. |
| `web/serve.sh` | Serves the result on this Mac only: http://127.0.0.1:8642/exult.html. It tells the browser not to cache, so a reload gets the latest build. |
| `web/shell.html` | The page: the setup screen and the game's canvas. |
| `web/pre.js` | Choosing the game folder, browser storage, Start. |
| `web/exult-web.cfg` | Exult's config in the browser (paths, screen size, audio). |

`web/build.sh` works in a copy of the sources, `web/build`, because the
native build lives in the source folder. `web/build`, `web/pkg` and
`web/out` are made by the script and git-ignored. Never commit them, and
never put Black Gate's files in them: players bring their own.

Under the hood: Asyncify lets Exult's own loops hand control back to the
browser (every wait goes through `SDL_Delay`); C++ exceptions are on
(`-fexceptions`); the canvas has no alpha channel, or the browser blends
the world away (`imagewin.cc`).

## Done

- [x] Exult compiles and runs: intro, title menu, character creation, the
      opening in Trinsic, conversations, paperdoll, minimap, skill bar
      (2026-10-29).
- [x] Setup page: choose the Ultima VII folder (or one above it). The page
      finds the Black Gate STATIC folder anywhere inside, says so when there
      are no Black Gate files or it looks like Serpent Isle, and copies the
      files into the browser's storage (IndexedDB). Nothing is uploaded.
      Later visits go straight to Start (2026-10-30).
- [x] The game in progress (gamedat) and saves are kept in the same storage,
      written back every 5 seconds and when the page is left. Journey
      Onward picked up a new game after a reload (2026-10-30).
- [x] Moved into this repo as `web/` (2026-10-30).
- [x] Settings (video, UI, status bar, and so on) are kept: Exult's config
      is `/user/exult.cfg` in browser storage, copied from the page's
      `exult-web.cfg` the first time. A later change to `exult-web.cfg`
      reaches only players who haven't played yet. A settings change is
      copied to storage at once (`Configuration::write_back`), not on the
      5-second timer (2026-10-30).
- [x] Main Menu, and Quit in the game menu, go back to the title menu. Exult
      restarts itself for that on the desktop, which a page can't do, so the
      page saves and reloads straight to the title menu, skipping the intro.
      Exit on Exult's own menu goes back to the setup page (2026-10-30).

## To do

### Save files

- [ ] Test saving and loading from the in-game save menu (same storage, not
      tried yet: the opening scene blocks saving until it ends, and a test
      on 2026-10-30 stopped before that).
- [ ] Export and import buttons, so saves can be backed up or moved to
      another computer or browser.
- [ ] Storage that may be cleared: the page asks the browser to keep its
      storage (`navigator.storage.persist()`), but Safari deletes a site's
      storage after 7 days without a visit, unless the page is added to the
      Home Screen or Dock. Tell Safari players, and make export easy.
- [ ] A save written while the page is closed mid-sync can be lost; check
      what happens if the tab is closed during a save.

### Audio

- [ ] Sound effects: the audio device opens when Start is pressed (browsers
      allow sound only after a click), not yet listened to.
- [x] Music with Exult's built-in FM synthesizer (FMOpl), on in
      `exult-web.cfg` (2026-10-30). It runs inside the audio callback,
      and the game waited for it in a loop that never handed control back
      to the browser, so the callback never ran and the game stalled 2
      seconds at a time. In the browser build only, those waits now use
      `SDL_Delay` (`yield_to_producer` in `LowLevelMidiDriver.cpp`), which
      lets the browser run audio in between. No more stalls, and sound
      comes out; not yet listened to.
- [ ] Music packs: an optional "Add a music pack" on the setup page, a
      .zip or folder of OGG files copied into browser storage like the
      game files, with Exult's digital music turned on when one is there.
      Players bring their own, so nothing is hosted: Exult's own digital
      music pack, or a fan remake such as JD Harding's.
- [ ] Exult's audio uses `ScriptProcessorNode`, which browsers have
      deprecated (a console warning today). Check for crackling or gaps
      when the game is busy; an AudioWorklet is the long-term fix.

### Keyboard

The browser sees every key first. Some it lets the page block, some it
never gives up. Go through the Keys page and check each one.

SDL already keeps function keys, Tab, Backspace, the arrows and Ctrl
combinations from the browser while playing, and every key it uses while
nothing is being typed. The page (`pre.js`) adds Cmd combinations other
than cut, copy, paste, select all and undo, and the page can't scroll
(2026-10-30).

A browser names a key by what it types (Shift+A as "A", Shift+1 as "!",
Option+Space on a Mac as a no-break space), so in the browser build only,
`Browser_key_as_desktop` (exult.cc) turns Shift and Alt keys back into the
plain key with the modifier apart, as on the desktop (2026-10-30). It uses
SDL's US layout for those, so on other keyboard layouts a Shift or Alt
shortcut may land on a different key: check.

- [ ] Keys the browser lets the page block, checked one by one: F1 (help
      in some browsers), F3 and Ctrl/Cmd+F (find), F5 and Ctrl/Cmd+R
      (reload), F6, F7 (caret browsing in Chrome and Firefox), F10, F12
      (developer tools), Tab (moves focus off the canvas), Space and the
      arrow keys (scroll the page), Backspace, Alt alone (Windows menu bar),
      Ctrl/Cmd+S, P, O, D, H, L, plus and minus.
- [ ] Keys that can't be blocked: Ctrl/Cmd+W, Ctrl/Cmd+T, Ctrl/Cmd+N,
      Cmd+Q, Ctrl+Tab, Alt+F4, Cmd+Tab, and Escape leaving fullscreen. None
      of the game's keys should need them.
- [ ] Fullscreen with the Keyboard Lock API (Chromium only) captures most of
      the second list too, including Escape. Worth a Fullscreen button.
- [x] Leaving the page while a game runs (closing the tab, Cmd+W, reload)
      asks first (2026-10-30).
- [ ] Option+Space on a Mac: fixed as above, not yet tried with a real Mac
      keyboard (the test browser's keys don't type a no-break space).

### Controller

- [ ] Exult's controller support uses SDL, which reads the browser's
      Gamepad API. Test a PS4 and an Xbox pad. Browsers hide pads until a
      button is pressed while the page has focus.
- [ ] Steam Controller: Steam Input only reaches a browser launched through
      Steam, so it may need the pad's own mode.
- [ ] Button names and the pad's layout can differ from the desktop build
      (the Gamepad API's "standard" mapping).

### Co-op (WebRTC)

- [ ] The browser build is made with `--with-friend-rtc=no` today. Friend
      Mode's connection code (`friendrtc.cc`, libdatachannel) has to use
      the browser's own WebRTC instead, through JavaScript. The lobby
      (Cloudflare Worker) can stay as it is, with its rules in
      [security](security.md).
- [ ] Hosting stays on the Mac and Windows builds; browsers join
      (2026-10-30). Browsers slow down hidden tabs, so a host in a
      background tab would slow every player, while a guest in one only
      stalls their own character. The browser build already knows it's a
      browser (`__EMSCRIPTEN__`), so no new setting: in the Co-Op Mode
      menu, Host is greyed out with a line saying hosting needs the Mac or
      Windows app (greyed, not hidden, so players know where to go), and
      the lobby only ever sees browsers joining. Keep it in one place in
      the code, so it can be turned on if hosting in a browser ever proves
      reliable.
- [x] Until co-op works in the browser, the title menu shows "Co-Op Mode:
      Desktop App Only", which can't be chosen (`game.cc`, 2026-10-30). It
      looks like the other entries; a darker font would read as greyed.

### Page and hosting

- [ ] The page's look, the canvas filling the window, and a Fullscreen
      button. A little room above the game in a window, none in fullscreen
      (2026-10-30).
- [ ] Hosting as a static site (Cloudflare Pages), built from `web/out`.
      The page files should be sent compressed (2.7 MB instead of 11 MB for
      the game).
- [ ] Exult is GPL-2, so the page needs a link to its source code, so it
      can't go public until the repo does. Ship a licences file for the
      parts linked in: Emscripten (MIT), libogg and libvorbis (BSD), SDL3
      and zlib (zlib licence).
- [ ] Serpent Isle (only Black Gate today).
- [ ] Speed while walking and fighting, and on a slower computer.
- [ ] Other browsers: Safari, Firefox, Windows.
- [ ] Drag and drop of the game folder onto the page.
- [x] A touch screen with no mouse or trackpad gets "Phones aren't
      supported" on the setup page. A tablet goes on to the setup once a
      key or controller button is pressed (a browser can't tell they're
      attached until then), or with Try anyway; one with a trackpad or
      mouse goes straight there (2026-10-30).
- [ ] Getting the game files onto a tablet: iPad Safari may not let a page
      pick a whole folder. If not, accept a .zip of the folder.
- [ ] "Und sched. opcode 0x00" appears five times in the log at the start of
      the opening: check whether the desktop build prints it too.
