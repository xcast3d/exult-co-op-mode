# The lobby (Phase 8)

Status: the lobby is built and tested on one Mac (2026-10-20, `npm test`
against `wrangler dev`: 15 checks pass). Not deployed yet; the Exult side
(WebRTC through libdatachannel) is the next step.

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
- In Exult (to come): a setting, `config/friend/lobby_url`, defaulting to
  that address, so anyone can point a copy at their own lobby.
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
message, 240 messages a minute per connection, codes end when the host
leaves or after 12 hours.

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

Not proven yet: two machines on different networks (needs the deployed
lobby), TURN, and Windows (the same script under the Windows workflow, or
its CMake steps in Visual Studio's).

## Next: the Exult side

1. Build libdatachannel (MPL-2.0) on the Mac, and on Windows through GitHub
   Actions; prove two programs connect through it with this lobby.
2. `friendrtc.cc` behind `Friend_connection` (`friendconn.h`), next to
   `friendtcp.cc`: a reliable, ordered data channel, so the game's messages
   don't change. The world download goes in pieces.
3. The lobby connection (a WebSocket) from Exult.
4. Menus: Host a Game shows the code; Join a Game takes a code or an
   address.
