# Security

Co-Op Mode talks to strangers' computers: the lobby is public on the
internet, a host by IP has an open port, and host and guest each trust what
the other sends. **The rule (decided 2026-10-23): anything that uses the
lobby is made as secure as it can be.** This page lists what is protected,
what a sweep found and fixed, and what is still open.

Related: [the lobby](lobby.md), [playing over the internet](internet-play.md),
the [interaction authority policy](interaction-policy.md) (what guests may
do in the world).

## Who can reach what

| From | Reaches | How |
| --- | --- | --- |
| Anyone on the internet | The lobby (`u7-multiplayer-lobby.xcast3d.com`) | Opening a code, joining with one, asking for their own address |
| Anyone with the code | A host by lobby code | WebRTC data channel, encrypted |
| Anyone who finds the port | A host by IP | TCP port 1992, plain text |
| The host | Every guest | The world download (a save file), and everything the guest shows |
| A guest | The host | Its moves, chat, item use, shop purchases, journal notes |

## What is protected

### The lobby (`tools/lobby/src/index.js`)

- **Codes are hard to guess:** a word and four digits (32 x 9000 =
  288,000), from `crypto.getRandomValues`. Joining is rate limited per
  address (`JOIN_LIMITER`, 20 a minute), so guessing a live code from one
  address takes days; opening games too (`HOST_LIMITER`, 10 a minute),
  which also keeps TURN credentials (once set up) from being handed out in
  bulk. Cloudflare's Workers Rate Limiting, set in `wrangler.toml`.
- **Only real codes reach a Durable Object:** anything that isn't a word,
  a dash and two to four digits is refused at once.
- **Signals are checked:** only `{sdp, kind: offer|answer}` and
  `{candidate, mid}` with text fields are passed on, rebuilt without any
  other fields.
- **Browsers are turned away:** a request with an `Origin` header (every
  browser sends one; the game doesn't) gets 403, so a web page can't use
  the lobby on a visitor's behalf. The one exception is the game's own web
  build: pages from the exact sites in `BROWSER_ORIGINS` (wrangler.toml)
  may join a game by code, never host or ask `/ip` (2026-11-01). A browser
  always sends its page's real site, so no other site's page gets past.
- **Idle guests can't fill a game:** a guest's lobby connection ends after
  60 seconds.
- **Limits:** 8 guests a code, 16 KB a message, 240 messages a minute a
  connection, codes end when the host leaves or after 12 hours.
- **`/ip`** only ever tells a computer its own address, refuses browsers,
  and shares the join rate limit.
- **No secrets in the repo:** TURN keys are Worker secrets; wrangler's
  login lives outside the repo.

### The game's lobby and WebRTC side (`friendrtc.cc`)

- **The lobby's certificate is always checked:** `wss://` only (plain
  `ws://` only to this computer, for `wrangler dev`), and without a list
  of certificate authorities the connection is refused, never made
  unchecked. The same for the internet-address lookup. Tested with a
  broken list: no connection.
- **The game is encrypted** between host and guest by WebRTC (DTLS); the
  lobby only sees the offers and candidates.
- **Limits on what a peer can make us hold:** lobby messages over 64 KB
  are refused, and a peer that sends more than 64 MB we haven't read yet
  is cut off.
- **Everything from the lobby is checked:** bad JSON, or a field of the
  wrong type, is ignored instead of risking a crash; error text is cut to
  printable characters before it is shown, and a code must look like one.

### The session (`friendnet.cc`, `friendtcp.cc`)

- **One way in at a time:** by lobby code the host doesn't also listen on
  port 1992; by IP there is no code. Hosting can be switched off, or
  closed to new players, from the game menu.
- **Password, optional:** the host sends a random number and the guest
  answers with the first 8 bytes of SHA-1 over it, the game and the
  password, so the password never crosses the connection. After 5 wrong
  answers in a minute, every try is refused for a minute.
- **Half-open connections:** at most 4 that haven't finished joining, each
  closed after 15 seconds, so a stranger can't tie up the host.
- **Names:** printable, 20 characters, taken once (a guest can't rename
  itself later to pose as someone else or take their companion back).
- **Kick:** the host's X in the Tab list; that name can't rejoin until
  hosting starts over.
- **Sizes:** a guest's message is 1 MB at most (the host's world 16 MB);
  chat and whispers 80 characters, printable only; every guest message
  checks its length before it is read.
- **What guests may do** is checked on the host (their own companion, their
  own items): see the [interaction authority policy](interaction-policy.md).

### Saves (`gamedat.cc`)

- **Names inside a save stay inside gamedat:** any name with `..`, a
  backslash, a colon or a leading slash is skipped, in all three ways a
  save is unpacked. This matters because a guest's world comes from the
  host as a save file (and it also protects saves downloaded from the web).

## Sweep, 2026-10-24

A pass over everything above. Found and fixed (not yet committed when
written):

| Found | Risk | Fix |
| --- | --- | --- |
| Save names were trusted: a name like `../../../.zshrc` was written outside the game's folder | **Serious.** A host (or a host's hacked computer) could write files anywhere in a guest's home folder, for example one that runs commands when a terminal opens | Names with `..`, `\`, `:` or a leading `/` are skipped (`gamedat.cc`) |
| No limit on connections that never finish joining | A stranger could hold the host's connections open | 4 at most, closed after 15 s |
| No limit on password guesses (one per connection, but endless connections) | Guessing a short password | 5 wrong a minute, then a minute's wait |
| A guest could send a new name at any time | Posing as another player; taking a companion held for someone who dropped out | A name is taken once |
| Guests could send 16 MB messages | Memory on the host | 1 MB for guests |
| A lobby message with a field of the wrong type | Could crash the game (an exception on libdatachannel's thread) | Caught and ignored; lobby text cleaned before it is shown |

Checked and fine: chat, names and whispers (shown only as printable
text); every guest message handler checks lengths before reading; the
lobby's codes, signals and address lookup.

Still to do for this sweep: try the save fix with a deliberately broken
save (it was checked by reading the code and builds).

## Still open

**Priority (decided 2026-10-24):** anything that could cause harm outside
Exult itself (on a player's computer, or to the lobby and its Cloudflare
account) comes first. Cheating, encryption of direct connections and
player identity are not worth more work for a co-op game among friends.

### Next

- **Lobby: hibernation and a cap on codes** (built and deployed
  2026-10-24: 22 smoke checks and the session test pass against the live
  lobby, a quiet code held through its sleep, and a guest that never
  connects is let go at 60 s). Each open code is a Durable
  Object, and today it is billed the whole time the host is connected
  (`server.accept()`). Cloudflare's free plan gives 100,000 requests and
  13,000 GB-s of duration a day (reset at 00:00 UTC); at 128 MB an object
  that is about 29 hours of open codes a day, shared by everyone. Past it,
  nothing is charged on the free plan, but new lobby connections fail
  until the reset (joining by IP still works); one address holding 10 new
  codes a minute would use up the day in about 20 minutes. (On the paid
  Workers plan going over costs money instead: check the account's plan.)
  The fix: the WebSocket Hibernation API (`state.acceptWebSocket`), so an
  object sleeps between messages and isn't billed while idle (the host's
  keep-alive every 30 s wakes it for an instant), and a cap of 3 open codes
  per address (hosts only). For guests nothing changes: a sleeping code
  wakes in milliseconds when they join; the limits stay 8 at once in the
  lobby (connecting, not playing), 20 tries a minute per address, 60 s to
  connect. To build it: keep each guest's id with its connection
  (`serializeAttachment`), since a sleeping object forgets its memory, and
  make the 60 s guest timer a Durable Object alarm. Redeploying drops any
  code open at that moment.
- **Try the save fix with a deliberately broken save.**
- **Save files from strangers** (below) before any public games.

### Known, not worth more work


- **Joining a host means trusting them.** The host sends the world, and
  Exult's save and map readers were written for one's own saves, not for
  files from strangers: a deliberately broken one could at least crash the
  guest's Exult, and readers like that can hide worse bugs. Fine between
  friends; the main thing to solve before public games (the Private /
  Public idea in [the lobby](lobby.md)).
- **A modified guest can cheat in shops and training.** Exult's cheat mode
  is already off on guests; this needs a rebuilt Exult. The protocol
  number (36) only keeps different builds from mixing by accident: Exult
  is open source, so someone could change what their copy sends and keep
  the number. The guest runs the merchant's conversation on its own copy,
  and the host checks a purchase is sane (gold for items, small stat
  changes), not the price.
- **By IP, nothing is encrypted:** chat and the world travel as plain
  text, and someone watching could try passwords offline against what they
  saw (a long password helps). Lobby codes are encrypted.
- **The host's password is never saved** (since 2026-10-24): it lasts
  while Exult runs, so each session starts with none. A password saved in
  `exult.cfg` by an older build is erased at startup.
- **A name is not an identity:** a kicked player can come back under
  another name (unless there is a password). An IP address wouldn't do
  better: a VPN changes it, and it would block people sharing one home
  connection.
- **The lobby is trusted to pass the offers honestly.** A lobby that was
  taken over could put itself in the middle. The fix, if wanted later: a
  short check from each side's DTLS fingerprint that both players compare.
- **Each side's addresses** (local and public) reach the other through the
  lobby, as in any WebRTC call.
- **A lobby someone runs themselves** (`config/friend/lobby_url`) is theirs
  to secure.

## Checking

- **Lobby:** `node test/smoke.mjs wss://u7-multiplayer-lobby.xcast3d.com`
  in `tools/lobby` (23 checks: codes, signals, refusals, browsers and the web build, `/ip`).
- **Session:** `tools/friend-mode/rtc-session-test/build.sh`, then
  `/tmp/rtc_session_test` (26 checks: joining by code and by IP, password,
  the guessing brake, kick, half-open connections, stopping hosting).
- **For any change to the lobby or the network code:** go through the
  lists above, and add a check to one of these tests for each new limit.
