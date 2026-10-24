# Playing over the internet

**The easy way (Mac build, 2026-10-23): a lobby code.** When you host,
the game shows a code such as `MOONGLOW-4271` (also in the chat log, and on
the Escape menu's Code button, which copies it). The guest types it in
**Join a Game** instead of an address. No port forwarding; see
[the lobby](lobby.md). Everything below is for joining by address, which
still works for LAN play, Tailscale, and when the lobby is down.

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
then. Lobby codes over WebRTC (Phase 8 in the
[project brief](../ultima-7-friend-mode.md)) are the planned answer.

## Without port forwarding: Tailscale

Tailscale makes a private, encrypted network between players and gets
through routers and carrier-grade NAT by itself, so Co-Op Mode works as on
a LAN:

1. Everyone installs Tailscale and signs in with their own account.
2. The host shares their computer with each player (Tailscale admin
   console, **Share**); players accept and see only that machine.
3. The host starts hosting as usual. Guests use **Join a Game** with the
   host's Tailscale address (`100.x.y.z`, or its MagicDNS name, shown in
   the Tailscale menu); the port stays 1992.

Connections are usually direct; when they can't be, Tailscale relays them,
which adds lag (the Tailscale menu shows "direct" or "relay"). Lobby codes
(Phase 8) are the planned way to do without any of this.

## Security

By lobby code, the connection is encrypted (WebRTC's DTLS), and the code
is the only way in: anyone who has it can join while you host. By
address, there is no password and no encryption. While you host, anyone who finds
the open port can join a free companion, and the chat and world travel in
plain text. Host only while playing, and close Exult afterwards (or remove
the port forward).

## Expected rough edges

Not yet tried outside one Mac:

- **Connecting** gives up after 5 seconds per address, so a wrong address
  or closed port shows "Could not connect to host" quickly. Small messages
  go out at once (`TCP_NODELAY`).
- **Lag in general.** The host checks every guest step; over a long
  distance the guest's companion may snap back now and then.
- Joining downloads the host's save (about 190 KB), which takes a moment on
  slow connections.

## Sharing builds and the GPL

Exult is GPL software. Handing a modified build to a friend means offering
them the matching source too (for example access to this repository at the
same commit). The Ultima VII game files are never part of a build.
