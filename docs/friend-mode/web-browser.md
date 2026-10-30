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
| `web/serve.sh` | Serves the result on this Mac only: http://127.0.0.1:8642/exult.html |
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

## To do

### Save files

- [ ] Test saving and loading from the in-game save menu (same storage, not
      tried yet).
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
- [ ] Music: MIDI is off in `exult-web.cfg`. Try Exult's built-in FM
      synthesizer (FMOpl, software only, so it should just work), then
      digital (OGG) music, which needs Exult's music pack hosted with the
      page.
- [ ] Exult's audio uses `ScriptProcessorNode`, which browsers have
      deprecated (a console warning today). Check for crackling or gaps
      when the game is busy; an AudioWorklet is the long-term fix.

### Keyboard

The browser sees every key first. Some it lets the page block, some it
never gives up. Go through the Keys page and check each one.

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
- [ ] Losing work by accident: ask before leaving the page
      (`beforeunload`) while a game is running.
- [ ] Option+Space on a Mac types a non-breaking space; make sure it still
      arrives as Alt+Space.

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
- [ ] Until co-op works in the browser, the whole Co-Op Mode entry on the
      title menu is greyed out the same way (today it's still shown).

### Page and hosting

- [ ] The page's look, the canvas filling the window, and a Fullscreen
      button.
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
- [ ] "Und sched. opcode 0x00" appears five times in the log at the start of
      the opening: check whether the desktop build prints it too.
