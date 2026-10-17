/*
 * Friend Mode connections: one small interface between the session's
 * messages and the transport that carries them.
 *
 * A connection is a reliable, ordered byte stream (TCP today; a WebRTC data
 * channel later), read and written without blocking.  The session keeps its
 * own framing on top, so a new transport only has to move bytes.
 */
#ifndef FRIENDCONN_H
#define FRIENDCONN_H

#include <cstddef>
#include <memory>
#include <string>

class Friend_connection {
public:
	virtual ~Friend_connection() = default;
	// Bytes taken (0 when the transport is full for now), or -1 once closed.
	virtual int write(const unsigned char* data, std::size_t size) = 0;
	// Bytes read (0 when nothing has arrived yet), or -1 once closed.
	virtual int  read(unsigned char* data, std::size_t size) = 0;
	virtual void close()                                     = 0;
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

#endif
