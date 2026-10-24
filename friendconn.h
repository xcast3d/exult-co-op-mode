/*
 * Friend Mode connections: one small interface between the session's
 * messages and the transport that carries them.
 *
 * A connection is a reliable, ordered byte stream (TCP, or a WebRTC data
 * channel met through the lobby), read and written without blocking.  The session keeps its
 * own framing on top, so a new transport only has to move bytes.
 */
#ifndef FRIENDCONN_H
#define FRIENDCONN_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Friend_connection {
public:
	virtual ~Friend_connection() = default;
	// Bytes taken (0 when the transport is full for now), or -1 once closed.
	virtual int write(const unsigned char* data, std::size_t size) = 0;
	// Bytes read (0 when nothing has arrived yet), or -1 once closed.
	virtual int  read(unsigned char* data, std::size_t size) = 0;
	virtual void close()                                     = 0;
	// Why it closed or never opened, when the transport knows ("No game
	// with that code"); empty otherwise.
	virtual std::string problem() const {
		return {};
	}
};

class Friend_listener {
public:
	virtual ~Friend_listener() = default;
	// A newly connected peer, or null when none is waiting.
	virtual std::unique_ptr<Friend_connection> accept() = 0;
};

// TCP on every local address (IPv4 and IPv6 where possible).
std::unique_ptr<Friend_listener> Friend_tcp_listen(int port);
// TCP to a host name or address, giving up after 'timeout_ms' per address.
std::unique_ptr<Friend_connection> Friend_tcp_connect(const std::string& address, int port, int timeout_ms);
// This computer's IPv4 addresses others could use (home network first,
// then Tailscale's 100.x), without the loopback one.
std::vector<std::string> Friend_local_addresses();

// The host's side of the lobby (friendrtc.cc): a code from the lobby, and
// guests who typed it, once their data channel is open.
class Friend_lobby_host : public Friend_listener {
public:
	// The code to give guests; empty until the lobby answers (or after it
	// went away).
	virtual std::string code() const = 0;
	// Why there is no code, when something went wrong.
	virtual std::string problem() const = 0;
};

// This computer's internet (IPv4) address as the lobby sees it, for hosts
// who share an IP (with port forwarding): asked for once in the
// background; empty until it arrives, or when it can't be had.
void        Friend_ask_public_ip(const std::string& lobby_url);
std::string Friend_public_ip();
// True once the answer is in (it may still be empty).
bool        Friend_public_ip_done();

// A word and a number, such as "MOONGLOW-42": a lobby code, not an address.
bool Friend_is_lobby_code(const std::string& text);
// Open a code through the lobby at 'lobby_url'; null (with 'problem' saying
// why) when this build has no WebRTC or the address is refused.
std::unique_ptr<Friend_lobby_host> Friend_rtc_host(const std::string& lobby_url, std::string& problem);
// Join by code.  Returns at once: the connection reads and writes nothing
// until its channel opens, and closes (with a problem()) if that fails or
// takes longer than 'timeout_ms'.
std::unique_ptr<Friend_connection> Friend_rtc_connect(
		const std::string& lobby_url, const std::string& code, int timeout_ms, std::string& problem);

#endif
