# Playing over the internet

Co-Op Mode has been tested on one Mac, and once on a Windows PC. Nothing in it is tied to
the local machine, though: the host listens on every network address, and
**Join a Game** takes an IP address or a host name. So a friend outside your
home network can join once the host's router lets them in. This page lists
what that takes and what to expect.

## What the guest needs

- **The same build.** Both sides must speak the same protocol version
  (17 as of `a7ff2ccab`), or the host turns the guest away. Build this
  branch, or share a build (see the GPL note below).
- **A Mac, for now.** Windows is planned next; see the
  [Windows build plan](windows-build.md).
- **Their own copy of Black Gate.** Exult checks that both sides run the
  same game. Game files are never sent; only the save is.
- **Gatekeeper:** a build that isn't signed shows "unidentified developer".
  Right-click the app and choose Open the first time.

## What the host needs

1. **Router:** forward **TCP port 1992** to the host Mac's local address.
   Give the Mac a reserved (fixed) local address in the router first, so
   the forward keeps working.
2. **macOS firewall:** allow incoming connections for Exult when asked.
3. **Your public address:** the host takes IPv4 and IPv6. Give the guest
   your public IP (shown by sites
   such as whatismyip.com), or a dynamic DNS name.
4. Start hosting: **Co-Op Mode → Host a Game** on the title menu, or
   **Open to Co-Op** in the Escape menu during play.

`--friend-port <port>` picks another port on both sides if 1992 is taken.

**Carrier-grade NAT:** some internet providers (most mobile, some fiber)
share one public address between customers. Port forwarding can't work
then. Lobby codes over WebRTC (Phase 5b in the
[project brief](../ultima-7-friend-mode.md)) are the planned answer.

## Security

There is no password and no encryption. While you host, anyone who finds
the open port can join a free companion, and the chat and world travel in
plain text. Host only while playing, and close Exult afterwards (or remove
the port forward).

## Expected rough edges

Not yet tried outside one Mac:

- **Delayed steps.** The sockets don't set `TCP_NODELAY`, so small messages
  (each step) can wait up to about 200 ms for the network to bundle them.
  Walking may feel sticky. Fix: set `TCP_NODELAY` on both sockets in
  `friendnet.cc` before the first real test.
- **Connecting can stall.** The guest connects without a time limit, so a
  wrong address or a closed port can freeze its window for about a minute
  before "Could not connect to host".
- **Lag in general.** The host checks every guest step; over a long
  distance the guest's companion may snap back now and then.
- Joining downloads the host's save (about 190 KB), which takes a moment on
  slow connections.

## Sharing builds and the GPL

Exult is GPL software. Handing a modified build to a friend means offering
them the matching source too (for example access to this repository at the
same commit). The Ultima VII game files are never part of a build.
