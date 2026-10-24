# The lobby (Phase 8)

Status: **deployed 2026-10-20** at `wss://u7-multiplayer-lobby.xcast3d.com`
(Cloudflare Worker `u7-multiplayer-lobby` on the xcast3d.com account).
**Exult uses it since 2026-10-23** (`friendrtc.cc`, Mac build): the host
gets a code, the guest types it, and they connect over a WebRTC data
channel. TURN is not set up.

**Hardened and redeployed 2026-10-23:** four-digit codes and the security
changes below; the smoke test (18 checks) and the session test pass
against the live lobby. A redeploy drops every open lobby connection, so a
host running at that moment loses its code ("The lobby closed the code")
and has to host again; games already connected carry on.

**Planned (user, 2026-10-23, not now): less text on screen.** The code
notices are too long for the top line. Move this into a menu instead:
Co-Op Mode → IP | Lobby Code, and a password, with the code shown there
rather than spelled out over the game.

## What it is, in plain words

Today a guest joins by typing the host's internet address, and the host's
router has to let them in (port forwarding). Some internet providers share
one address between customers, and then nothing can be forwarded.

With the lobby, the host's game asks it for a short code, such as
`MOONGLOW-42`, and the host reads it out. The guest types the code. The
lobby passes each side's "here's how to reach me" details to the other
(WebRTC's offer, answer and candidates), then steps aside: the two games
connect to each other directly, the way browser video calls do. The lobby
never carries the game.

- **STUN** (Cloudflare's free `stun.cloudflare.com`): tells each side what
  its address looks like from outside. Enough for most home networks.
- **TURN** (optional, Cloudflare's relay): for the networks where a direct
  connection can't be made, it carries the game traffic. The only part
  that can cost money; off until it is set up.
- Typing an address still works, for home networks, Tailscale, and when the
  lobby is down.

## Where it lives

- The code: `tools/lobby/` (a Cloudflare Worker, `src/index.js`, and a
  Durable Object per game code).
- The address: **`wss://u7-multiplayer-lobby.xcast3d.com`**, a subdomain of
  the project's own domain (decided 2026-10-20), so the Worker can move
  without breaking builds already handed out.
- In Exult: the setting `config/friend/lobby_url` in `exult.cfg`,
  defaulting to that address, so anyone can point a copy at their own
  lobby; `off` turns lobby codes off (hosting by address only). Only
  `wss://` addresses are taken, except `ws://127.0.0.1:8787` and other
  addresses on the same machine, for `wrangler dev`.
- No secrets in the game: TURN's key is a secret of the Worker, which hands
  each game short-lived credentials.

## How it talks

WebSocket, JSON text messages.

| Who | Path or message | Meaning |
| --- | --- | --- |
| Host | `wss://…/host` | Open a game; the lobby answers `{type:"code", code, ice}` |
| Guest | `wss://…/join/CODE` | Join (the code in any case, with a space or a dash); answer `{type:"joined", id, ice}` |
| Lobby → host | `{type:"guest", id}`, `{type:"left", id}` | A guest arrived or left |
| Guest → lobby | `{type:"signal", data}` | Passed to the host as `{type:"signal", from:id, data}` |
| Host → lobby | `{type:"signal", to:id, data}` | Passed to that guest as `{type:"signal", data}` |
| Host → lobby | `{type:"kick", to:id}` | Close that guest's lobby connection |
| Lobby → guests | `{type:"closed"}` | The host left; the code is gone |
| Lobby → anyone | `{type:"error", message}` | "No game with that code", "The game is full", "Too much" |

`ice` is the list of STUN (and TURN) servers to give WebRTC. `data` is
whatever the game sends; the lobby only passes it on.

Limits, so a public lobby can't be flooded: 8 guests per code, 16 KB a
message, 240 messages a minute per connection, a guest's lobby connection
at most 60 seconds (the game needs one or two, then closes it itself),
codes end when the host leaves or after 12 hours.

## Security

Anyone on the internet can reach the lobby, so (2026-10-23):

**In the Worker** (`tools/lobby/src/index.js`):

- **Codes are hard to guess:** a word and four digits (32 x 9000 =
  288,000), from `crypto.getRandomValues` (`Math.random` before). Joining
  is rate limited per address (`JOIN_LIMITER`, 20 a minute), so guessing a
  live code from one address takes days; opening games too
  (`HOST_LIMITER`, 10 a minute), which also keeps TURN credentials (when
  set up) from being handed out in bulk. The limits are Cloudflare's
  Workers Rate Limiting, set in `wrangler.toml`.
- **Only real codes reach a Durable Object:** anything that isn't a word, a
  dash and two to four digits is refused at once ("No game with that
  code").
- **Signals are checked:** only `{sdp, kind: offer|answer}` and
  `{candidate, mid}` with text fields are passed on, rebuilt without any
  other fields.
- **Browsers are turned away:** a request with an `Origin` header (every
  browser sends one; the game doesn't) gets 403, so a web page can't use
  the lobby on a visitor's behalf.
- **Idle guests can't fill a game:** a guest's lobby connection ends after
  60 seconds.
- **No secrets in the repo:** TURN keys are Worker secrets.

**In the game** (`friendrtc.cc`):

- **The lobby's certificate is checked**, always: `wss://` only, and
  without a list of certificate authorities the connection is refused,
  never made unchecked (tested with a broken list: "Could not reach the
  lobby").
- **The game is encrypted** by WebRTC (DTLS) between the two machines; the
  lobby only sees the offers and candidates (which include each side's
  addresses).
- **Limits on what a peer can make us hold:** lobby messages over 64 KB are
  refused, and a peer that sends more than 64 MB we haven't read yet is
  cut off.
- Everything from the lobby is checked before use (bad JSON or fields are
  ignored).

**What it doesn't protect against** (worth knowing, not fixed):

- **Anyone with the code can join** while you host; there is no password
  yet (the optional session password is a Phase 8 item). Codes end when
  you stop hosting.
- **The lobby is trusted to pass the offers honestly.** A lobby that was
  taken over could put itself in the middle. The fix, if wanted later:
  show a short check (from each side's DTLS fingerprint) both players can
  compare.
- **Each side's addresses** (local and public) go to the other through the
  lobby, as in any WebRTC call.
- A lobby someone points their own copy at (`lobby_url`) is theirs to
  secure.

## Deploying (the host's one-time step)

**Credentials stay out of the repo** (decided 2026-10-20). `wrangler login`
keeps its token in your home folder (`~/Library/Preferences/.wrangler`),
not here; `tools/lobby/.gitignore` ignores `.wrangler/` and
`node_modules/`; TURN keys are Worker secrets (`wrangler secret put`), never
in `wrangler.toml`. If an API token is used instead of the login (for a
script or CI), put it in the `CLOUDFLARE_API_TOKEN` environment variable,
never in a file in the repo.

On the Mac, in the repo:

```bash
cd tools/lobby
```

```bash
npm install
```

```bash
npx wrangler login
```

That opens a browser to sign in to Cloudflare and allow wrangler. Then:

```bash
npx wrangler deploy
```

It creates the Worker, the Durable Object, and the custom domain
`u7-multiplayer-lobby.xcast3d.com` on the xcast3d.com zone (Cloudflare adds
the DNS record and certificate itself). Check it:

```bash
curl https://u7-multiplayer-lobby.xcast3d.com/health
```

It answers "Friend Mode lobby, protocol 1". To run the smoke test against
the live lobby:

```bash
node test/smoke.mjs wss://u7-multiplayer-lobby.xcast3d.com
```

**TURN, later and optional:** create a TURN key in the Cloudflare dashboard
(Calls → TURN), check its pricing, then store it as two secrets:

```bash
npx wrangler secret put TURN_KEY_ID
```

```bash
npx wrangler secret put TURN_KEY_API_TOKEN
```

To check: the TURN credentials call (`…/credentials/generate-ice-servers`)
against Cloudflare's current API documentation before turning it on;
without the secrets the lobby gives STUN only.

## Trying it locally

```bash
cd tools/lobby && npm run dev
```

and in another terminal:

```bash
cd tools/lobby && npm test
```

## Spike: libdatachannel through the lobby (passed, 2026-10-20)

`tools/friend-mode/rtc-spike/` is a small program that plays host or guest:
it meets the other through the lobby and opens a WebRTC data channel the way
Exult would. On this Mac, against `wrangler dev`, with both on one machine:

- libdatachannel 0.23.2 builds (static, without media).
- Host and guest met with a code; the channel opened about 1 second after
  the guest joined.
- 50 small messages came back in order, and a 256 KB message (like a piece
  of the world download) arrived intact.

**Decided 2026-10-20, and built:**

- **The libraries are pinned submodules:** `third_party/libdatachannel`
  (v0.23.2) and `third_party/mbedtls` (3.6.7, its long-term-support line).
  `tools/friend-mode/build-rtc.sh` builds both, static, into
  `third_party/build/<system>-<cpu>/` (git-ignored), with CMake, before
  Exult; Exult links the static libraries.
- **Mbed TLS, not OpenSSL:** built into the program, so nothing extra to
  ship or sign. It needs one feature switched on for WebRTC's handshake
  (DTLS-SRTP), in `tools/friend-mode/mbedtls_friend_config.h`.
- The spike, rebuilt that way, passed again: 3.1 MB, linked only to
  macOS's own libraries.

To build and run it:

```bash
git submodule update --init --recursive third_party/libdatachannel third_party/mbedtls
```

```bash
tools/friend-mode/build-rtc.sh
```

```bash
cmake -S tools/friend-mode/rtc-spike -B /tmp/rtc-spike && cmake --build /tmp/rtc-spike
```

With the lobby running (`npm run dev` in `tools/lobby`):

```bash
/tmp/rtc-spike/rtc_spike host ws://127.0.0.1:8787
```

and, with the code it prints:

```bash
/tmp/rtc-spike/rtc_spike join ws://127.0.0.1:8787 CODE
```

**Certificates for wss://** (found on the live lobby): Mbed TLS has no list
of trusted certificate authorities of its own, so the lobby connection
needs one or it fails ("No CA Chain is set"). The spike uses `RTC_CA_FILE`
or macOS's `/etc/ssl/cert.pem`; Exult will do the same on the Mac and ship
a CA bundle (Mozilla's, as curl publishes it) for Windows.

Not proven yet: two machines on different networks, TURN, and Windows (the same script under the Windows workflow, or
its CMake steps in Visual Studio's).

## The Exult side (built 2026-10-23, Mac)

- `friendrtc.cc`, behind `Friend_connection` (`friendconn.h`) next to
  `friendtcp.cc`: a reliable, ordered data channel, so the game's messages
  don't change. Writes go out as messages of at most 16 KB, so the world
  download travels in pieces, and wait while 1 MB is still queued.
- **Host:** hosting opens TCP port 1992 as before **and** asks the lobby for
  a code. The code shows near the top of the screen and in the chat log
  ("Co-Op code: MOONGLOW-4271"), and the Escape menu has a **Code** button
  that shows it again and copies it. If the lobby can't be reached, the
  host says so ("No Co-Op code: ...") and joining by address still works.
- **Guest:** **Join a Game** takes "Code or address". A word and a number
  (`MOONGLOW-4271`, `moonglow 4271`) goes through the lobby; anything else
  (`192.168.1.20`, `my-mac.local`, a Tailscale address) is TCP as before.
  `--friend-join` takes either too. Lobby problems are shown in words ("No
  game with that code", "The host stopped hosting", timed out after 20 s).
- **Build:** `configure` looks for the libraries `build-rtc.sh` made in
  `third_party/build/<system>-<cpu>/` (or `--with-friend-rtc=DIR`) and
  links them; without them Exult still builds, with joining by address
  only. After `build-rtc.sh`, rerun `./configure` (or `make`, which reruns
  it when `configure.ac` changed).
- **Session test** without the game's windows: Friend Mode's own session
  code as a host and two guests, one by code and one by address:

```bash
tools/friend-mode/rtc-session-test/build.sh
```

```bash
/tmp/rtc_session_test
```

  (or `/tmp/rtc_session_test ws://127.0.0.1:8787` with `npm run dev`).
  Passed 2026-10-23 through the deployed lobby: connected in about 1.2 s,
  300 KB intact, chat and a step both ways, by code and by address.

**Windows, still to do** (the GitHub workflow is unchanged, so the Windows
build joins by address only):

1. Check out submodules: `actions/checkout` with `submodules: recursive`.
2. Install `mingw-w64-ucrt-x86_64-cmake` and run
   `tools/friend-mode/build-rtc.sh` under MSYS2 before Exult (it may need
   `-G "MSYS Makefiles"` and `nproc`).
3. `Makefile.mingw` (via `Makefile.common`, which already lists
   `friendrtc.o`): add `-DFRIEND_RTC -DRTC_STATIC -isystem <prefix>/include`
   and the eight static libraries, plus `-lws2_32 -liphlpapi -lbcrypt`.
4. Ship a CA bundle (Mozilla's, as curl publishes it) next to `Exult.exe`
   and point `RTC_CA_FILE` (or a setting) at it: Windows has no
   `/etc/ssl/cert.pem`, and without a bundle the lobby is refused.
