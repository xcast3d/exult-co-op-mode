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

**Tested by hand 2026-10-23:** host and guest on one Mac, joined by code
through the live lobby; movement, chat and the world download (186 KB)
work exactly as over TCP.

**Fixed the same day: codes died after 10 seconds.** libdatachannel's own
WebSocket ping (sent after 10 quiet seconds) closes the connection when it
runs over Mbed TLS ("TLS closed"), so the lobby saw the host leave. Its
ping is off (`pingInterval` 0) and the host sends `{type:"keepalive"}`
every 30 s instead, which the lobby ignores. (The lobby itself keeps a
quiet connection open: checked for 5 minutes.)

**The Co-Op menu (built 2026-10-23, cleaned up 2026-10-24 after the
user's test; protocol 36):**

- **Title menu → Co-Op Mode:** HOST GAME, JOIN GAME, BACK.
  - **HOST GAME:** TYPE: LOBBY CODE / IP ADDRESS (`config/friend/join_by`),
    PASSWORD: NONE / SET (typed on its own screen; kept only while Exult
    runs, never saved), JOURNEY ONWARD (loads the game as Journey
    Onward does, open to Co-Op), BACK. The code is given once the game has
    started.
  - **JOIN GAME:** Your Name, Code/IP, Password (never saved), then JOIN
    GAME and BACK as the menu's own text entries. Click a field or use Tab;
    Enter joins, Esc goes back.
- **In the game:** one short line at the top, "Open to Code YEW-8890" or
  "Open on port 1992", nothing in the chat log. **Escape → Co-Op Mode:**
  - **Hosting: Off / On.** Off stops hosting completely: guests are told
    ("The host stopped hosting") and their companions return to the party.
  - **Type:** Lobby code or IP address. **New players:** Open or Closed
    (closing keeps players already in; opening again gives a new code).
  - **Password:** a box to click and type in, like the chat line (Enter or
    a click elsewhere saves it, Esc cancels).
  - **The code**, or by IP: **IP** (the internet IPv4 address, which the
    lobby reports: two certificate-checked lookups were tried, IPv4 is
    what port forwarding uses, so IPv6 was dropped), **Home network** and
    **Port**, on rows of their own. **Copy** copies the code or the IP.
- **One way in at a time:** by lobby code the host doesn't also listen on
  port 1992; by IP there is no code.
- **Password:** the host sends a random number; the guest answers with the
  first 8 bytes of SHA-1 over it, the game and the password, so the
  password never goes over the connection. Missing or wrong: "This game
  needs a password" / "Wrong password".
- **Kick:** hold Tab; the host has an X by each guest, asks, then removes
  them ("The host removed you"). That name can't rejoin until hosting
  starts over.
- The session test checks all of this (26 checks); see
  [security](security.md) for the limits.

**Idea, not started (user, 2026-10-23): Private / Public.** Public games
listed on `u7-multiplayer-lobby.xcast3d.com` (and maybe in Join a Game) to
join without a code. Needs an acceptance step first (we don't control
those games), ideally the host allowing each joiner, cleaned and
length-limited listing text, listings that expire, and a check of the
storage cost.

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
whatever the game sends; the lobby only passes it on. To build a lobby of
your own, the full contract (code shape, message order, `ice`, `/ip`) is
in [running your own lobby](lobby-protocol.md).

Limits, so a public lobby can't be flooded: 8 guests per code, 16 KB a
message, 240 messages a minute per connection, a guest's lobby connection
at most 60 seconds (the game needs one or two, then closes it itself),
codes end when the host leaves or after 12 hours.

## Security

Anyone on the internet can reach the lobby. What it and the game protect,
the sweep of 2026-10-24 and what is still open: [security](security.md).
In short: hard-to-guess codes with rate limits, checked signals, no
browsers, certificates always checked, encryption between the players, no
secrets in the repo.

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

**TURN (the relay):** needed when two homes can't link directly. First
real test (2026-10-24, a Mac and a Windows PC behind an AT&T gateway):
both reached the lobby and paired, but the direct link failed with STUN
alone. Create a TURN key in the Cloudflare dashboard (Realtime → TURN),
then store its two values as Worker secrets, in the dashboard (the
Worker's Settings → Variables and Secrets, type Secret) or with:

```bash
npx wrangler secret put TURN_KEY_ID
```

```bash
npx wrangler secret put TURN_KEY_API_TOKEN
```

Checked 2026-10-24: the lobby's credentials call
(`POST …/turn/keys/<id>/credentials/generate-ice-servers`, Bearer token,
`{"ttl": seconds}`, answer `{iceServers: [...]}`) matches Cloudflare's
documentation. Credentials last a game's 12 hours and aren't revoked when
the code closes: a host can close the code and keep playing with guests
already in, whose relayed links need them. Pricing: $0.05 per GB relayed,
after 1,000 GB a month free (shared with Cloudflare's SFU). Without the
secrets the lobby gives STUN only.

In the game: libdatachannel's ICE library (libjuice) relays over **UDP
only**, through the first two UDP TURN addresses in the list; Cloudflare's
TCP and TLS ones are skipped with a warning. A network that blocks
outgoing UDP would need libnice instead. The log says which way a link
went (`link open directly` or `through the relay`), and
`FRIEND_RTC_RELAY_ONLY=1` allows only the relay, to test it:

```bash
FRIEND_RTC_RELAY_ONLY=1 /tmp/rtc_session_test
```

## Usage stats (admin page)

The lobby counts three things per day (UTC), and nothing else: games opened
(codes handed out), games at least one guest joined, and guest joins. No
addresses or codes are kept. The counts live in one Durable Object
(`Stats`), and `https://u7-lobby-admin.xcast3d.com/admin` shows them. Like
Kitty Launcher's admin page, that hostname is behind Cloudflare Access, and
the Worker checks the Access login's signature itself before it answers.
Without `CF_ACCESS_AUD` it answers 503 to everyone. Counting began on the
day this shipped; earlier games aren't counted.

One-time setup, before deploying the admin route:

1. Cloudflare dashboard → Zero Trust → Access → Applications → Add an
   application → Self-hosted. Name it "U7 Lobby Admin", domain
   `u7-lobby-admin.xcast3d.com`, and give it a policy that allows only your
   own email.
2. On the application's Overview, copy the Application Audience (AUD) tag
   into `CF_ACCESS_AUD` in `tools/lobby/wrangler.toml`. Check that
   `CF_ACCESS_TEAM_DOMAIN` is your team name (the part before
   `.cloudflareaccess.com`). It's the same team as Kitty Launcher's admin.
   Neither value is a secret.
3. `npx wrangler deploy`. It adds the `u7-lobby-admin.xcast3d.com` custom
   domain and the `Stats` object (migration v3).

To see the page locally without Access: `npm run dev:admin`, then open
<http://localhost:8787/admin>. Only on localhost, and only with that
script's `ADMIN_LOCAL_DEV=1`, which is never in `wrangler.toml`.

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

**Windows (added 2026-10-24, not yet run on GitHub):** the
`friend-mode-windows` workflow checks out the submodules, builds the
libraries with `build-rtc.sh` under MSYS2 (Ninja, cached between runs on
the submodule commits and the script), and `Makefile.mingw` links them when
`third_party/build/windows/lib/libdatachannel.a` exists (otherwise joining
by address only, as before). Windows has no PEM list of certificate
authorities, so the install copies MSYS2's `ca-certificates` bundle
(Mozilla's list) to `data/friend/cacert.pem`, and `friendrtc.cc` looks
there after `RTC_CA_FILE`; on Windows it never reads the Unix paths
(`/etc/ssl` would be `C:\etc\ssl`, which any user can create).
