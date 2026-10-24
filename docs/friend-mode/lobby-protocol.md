# Running your own lobby: what the game expects

Lobby codes go through a small server, the **lobby**, that hands out codes
and passes WebRTC connection details between a host and its guests, then
steps aside. It never carries game traffic. The project's lobby is a
Cloudflare Worker (`tools/lobby/src/index.js`, see [the lobby](lobby.md)),
but any server that speaks the protocol below works. Lobby protocol 1, as
of 2026-10-24.

## Pointing Exult at it

In `exult.cfg`:

```xml
<config><friend><lobby_url>wss://lobby.example.com</lobby_url></friend></config>
```

- Default `wss://u7-multiplayer-lobby.xcast3d.com`; `off` turns lobby codes
  off (joining by address only).
- Only `wss://` is accepted, except `ws://127.0.0.1`, `ws://localhost` and
  `ws://[::1]` (any port) for testing on one machine. A bare host name gets
  `wss://`; trailing slashes are dropped.
- The certificate must be one a browser would trust. The game checks it
  against Mozilla's list (on Windows the copy shipped as
  `data/friend/cacert.pem`) and never connects unchecked.
- Host and guests must use the same lobby.

## What it must serve

All three at the root of the host name (`/ip` is always asked at the root,
even if `lobby_url` has a path).

| Path | Kind | Who | Purpose |
| --- | --- | --- | --- |
| `/host` | WebSocket | Host | Open a game and get a code |
| `/join/CODE` | WebSocket | Guest | Join that game |
| `/ip` | HTTPS GET | Host | The caller's own internet address (optional) |

WebSocket messages are JSON text, one object per message, each with a
`type`. Unknown types and fields are ignored by the game, so a lobby can
add its own. The game sends no `Origin` header (browsers do, and may be
refused), sends no WebSocket pings (they break the game's TLS library), and
waits at most 10 seconds for the WebSocket to open.

### Codes

The game decides whether "Join" text is a code or an address by its
shape, so codes must look like this: **2 or more letters, then an
optional dash or space, then 1 to 4 digits** (`MOONGLOW-4271`). Anything
else is treated as an address. The game sends the code in `/join/` with
letters upper-cased and spaces turned into a dash (`moonglow 4271` →
`MOONGLOW-4271`), at most 32 characters. It shows the host's code exactly
as the lobby sent it, and ignores one that doesn't have that shape.

### Host

1. Opens `wss://<lobby>/host`.
2. Lobby → host: `{"type":"code", "code":"MOONGLOW-4271", "ice":[…]}`.
3. For each guest that joins, lobby → host: `{"type":"guest", "id":1}`.
   `id` is a number, unique within the game. The host makes a WebRTC peer
   for it with that game's `ice` and waits for the guest's offer.
4. Lobby → host: `{"type":"signal", "from":1, "data":{…}}`, and
   host → lobby: `{"type":"signal", "to":1, "data":{…}}`, until the link
   opens. Pass `data` on unchanged.
5. Lobby → host: `{"type":"left", "id":1}` when a guest's lobby connection
   ends. The game drops a guest still connecting and keeps one already
   linked, because **guests close their lobby connection once linked**.
   That is normal and must not end the game.
6. Host → lobby, every 30 seconds: `{"type":"keepalive"}`. Ignore it or
   answer anything; just don't close a host connection that is quiet for
   less than a minute.
7. The host closing its WebSocket ends the code. Tell its guests
   `{"type":"closed"}` and close them. Linked players are unaffected:
   their link doesn't go through the lobby.

### Guest

1. Opens `wss://<lobby>/join/CODE`.
2. Lobby → guest: `{"type":"joined", "id":1, "ice":[…]}`, the same `ice`
   the host got. Also send the host `{"type":"guest", "id":1}`, before
   any of that guest's signals: the host only takes signals from a guest
   it has been told about.
3. The guest creates the data channel and sends the offer, then its
   candidates: guest → lobby `{"type":"signal", "data":{…}}`, which goes to
   the host as `{"type":"signal", "from":1, "data":{…}}`. The host's
   replies come back to the guest as `{"type":"signal", "data":{…}}`.
4. When the link opens, the guest closes its lobby connection.
5. The guest gives up after 20 seconds without a link ("No direct
   connection could be made" or a timeout).

### Messages any side may get

- `{"type":"error", "message":"No game with that code"}`. The game shows
  `message` to the player as is, so keep it short and plain. Then close.
- `{"type":"closed"}` (to a guest): the host stopped hosting.

### Signal data

Two shapes, made and read by libdatachannel; a lobby only passes them on:

- A description: `{"sdp":"v=0…", "kind":"offer"}` (or `"answer"`).
- A candidate: `{"candidate":"candidate:1 1 UDP …", "mid":"0"}`.

### `ice`

An array of servers in the browser's `RTCIceServer` shape:

```json
[
  {"urls": ["stun:stun.cloudflare.com:3478"]},
  {"urls": ["turn:turn.example.com:3478?transport=udp"], "username": "…", "credential": "…"}
]
```

- `urls` is a string or an array. `stun:` URLs are used as they are;
  `turn:`/`turns:` URLs only with a `username` (`credential` is the
  password). Others are skipped.
- With no STUN entry, the game adds `stun:stun.cloudflare.com:3478`.
- The game's ICE library (libjuice) relays over **UDP only** and uses at
  most two TURN addresses: the first two UDP ones in the list. TCP and
  TLS TURN addresses are skipped, so list UDP ones first.
- TURN credentials must stay valid for the whole game, not just the
  connection: a relayed link stops working when they expire. The
  project's lobby makes short-lived ones per code, lasting 12 hours, and
  keeps the TURN key itself secret on the server.
- An empty array works: STUN only (from the fallback), so some pairs of
  networks won't link.

### `/ip`

`GET /ip` over HTTPS, always on port 443 of `lobby_url`'s host name (even
if `lobby_url` names another port), asked over IPv4 only, and only for a
`wss://` lobby. Answer
`HTTP/1.1 200` with the caller's address as plain text in the body: no
chunked encoding, nothing else in the body. The game uses it to show a
host the address to give guests who join by IP. Without it the host just
doesn't see an address; codes still work.

## What is up to the lobby

Not needed for the game to work, but a public lobby should have them
(the project's lobby does: see the Worker's header and
[security](security.md)):

- Hard-to-guess codes from a secure random source, and rate limits on
  opening and joining, so codes can't be guessed or the lobby flooded.
- Checking that `signal` data has one of the two shapes and a sane size
  (an SDP is a few KB; the game refuses lobby messages over 64 KB).
- Limits per connection and per address: guests per code, messages per
  minute, open codes per address, how long a guest's lobby connection
  may stay open.
- Codes ending after some hours.
- `{"type":"kick", "to":id}` from the host, to close a guest's lobby
  connection (the game doesn't send it today).

## Checking a lobby

The session test plays a host and guests against any lobby, by code and by
address, with no game window:

```bash
tools/friend-mode/rtc-session-test/build.sh
```

```bash
/tmp/rtc_session_test wss://lobby.example.com
```

With `FRIEND_RTC_RELAY_ONLY=1` in front, only TURN paths are allowed, so
it passes only if the lobby's TURN servers work.
