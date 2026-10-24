/*
 * Friend Mode over TCP: the socket code behind Friend_connection.
 */
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendconn.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#else
#	include <arpa/inet.h>
#	include <csignal>
#	include <fcntl.h>
#	include <ifaddrs.h>
#	include <netdb.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <sys/select.h>
#	include <sys/socket.h>
#	include <unistd.h>
#endif

namespace {
// Writing to a connection the peer has dropped raises SIGPIPE on Unix, which
// kills the process.  Ignore it so send() fails with EPIPE and the session
// closes the connection normally.
void ignore_broken_pipe() {
#ifndef _WIN32
	std::signal(SIGPIPE, SIG_IGN);
#endif
}

// Windows sockets must be started once per process before any socket call.
bool start_sockets() {
#ifdef _WIN32
	static bool started = false;
	if (!started) {
		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			std::cerr << "Friend Mode: could not start Windows sockets" << std::endl;
			return false;
		}
		std::atexit([] {
			WSACleanup();
		});
		started = true;
	}
#endif
	ignore_broken_pipe();
	return true;
}

void close_socket(int socket) {
	if (socket < 0) {
		return;
	}
#ifdef _WIN32
	closesocket(static_cast<SOCKET>(socket));
#else
	close(socket);
#endif
}

bool set_nonblocking(int socket) {
#ifdef _WIN32
	u_long enabled = 1;
	return ioctlsocket(static_cast<SOCKET>(socket), FIONBIO, &enabled) == 0;
#else
	const int flags = fcntl(socket, F_GETFL, 0);
	return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

// Send small messages (each step) at once instead of letting TCP hold them
// back to bundle them (Nagle's algorithm), which can add ~200 ms per step.
void set_no_delay(int socket) {
	int enabled = 1;
	setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
}

// Connect, giving up after 'timeout_ms' instead of the system's minute or
// more, so a wrong address or closed port doesn't freeze the guest.
// Leaves the socket non-blocking.
bool connect_with_timeout(int socket, const sockaddr* address, socklen_t length, int timeout_ms) {
	if (!set_nonblocking(socket)) {
		return false;
	}
	if (connect(socket, address, length) == 0) {
		return true;
	}
#ifdef _WIN32
	if (WSAGetLastError() != WSAEWOULDBLOCK) {
		return false;
	}
#else
	if (errno != EINPROGRESS) {
		return false;
	}
#endif
	fd_set writable;
	FD_ZERO(&writable);
	FD_SET(static_cast<unsigned>(socket), &writable);
	timeval wait{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
	if (select(socket + 1, nullptr, &writable, nullptr, &wait) <= 0) {
		return false;    // Timed out.
	}
	int       error = 0;
	socklen_t size  = sizeof(error);
	return getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) == 0 && error == 0;
}

bool would_block() {
#ifdef _WIN32
	const int error = WSAGetLastError();
	return error == WSAEWOULDBLOCK;
#else
	return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

class Tcp_connection : public Friend_connection {
public:
	explicit Tcp_connection(int socket) : socket(socket) {
		set_nonblocking(socket);
		set_no_delay(socket);
	}

	~Tcp_connection() override {
		Tcp_connection::close();
	}

	int write(const unsigned char* data, std::size_t size) override {
		if (socket < 0) {
			return -1;
		}
		const int count = static_cast<int>(send(socket, reinterpret_cast<const char*>(data), size, 0));
		if (count >= 0) {
			return count;
		}
		return would_block() ? 0 : -1;
	}

	int read(unsigned char* data, std::size_t size) override {
		if (socket < 0) {
			return -1;
		}
		const int count = static_cast<int>(recv(socket, reinterpret_cast<char*>(data), size, 0));
		if (count > 0) {
			return count;
		}
		if (count < 0 && would_block()) {
			return 0;
		}
		return -1;    // Closed by the peer, or failed.
	}

	void close() override {
		close_socket(socket);
		socket = -1;
	}

private:
	int socket;
};

class Tcp_listener : public Friend_listener {
public:
	explicit Tcp_listener(int socket) : socket(socket) {}

	~Tcp_listener() override {
		close_socket(socket);
	}

	std::unique_ptr<Friend_connection> accept() override {
		const int peer = static_cast<int>(::accept(socket, nullptr, nullptr));
		if (peer < 0) {
			if (!would_block()) {
				std::cerr << "Friend Mode: accept failed" << std::endl;
			}
			return nullptr;
		}
		return std::make_unique<Tcp_connection>(peer);
	}

private:
	int socket;
};
}    // namespace

std::vector<std::string> Friend_local_addresses() {
	std::vector<std::string> found;
	auto                     add = [&found](const in_addr& address) {
        char text[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &address, text, sizeof(text)) && std::string(text).rfind("127.", 0) != 0
            && std::find(found.begin(), found.end(), text) == found.end()) {
            found.emplace_back(text);
        }
	};
#ifdef _WIN32
	if (!start_sockets()) {
		return found;
	}
	char name[256] = {};
	if (gethostname(name, sizeof(name)) == 0) {
		addrinfo hints{};
		hints.ai_family     = AF_INET;
		addrinfo* addresses = nullptr;
		if (getaddrinfo(name, nullptr, &hints, &addresses) == 0) {
			for (addrinfo* current = addresses; current; current = current->ai_next) {
				add(reinterpret_cast<sockaddr_in*>(current->ai_addr)->sin_addr);
			}
			freeaddrinfo(addresses);
		}
	}
#else
	ifaddrs* interfaces = nullptr;
	if (getifaddrs(&interfaces) == 0) {
		for (ifaddrs* current = interfaces; current; current = current->ifa_next) {
			if (current->ifa_addr && current->ifa_addr->sa_family == AF_INET) {
				add(reinterpret_cast<sockaddr_in*>(current->ifa_addr)->sin_addr);
			}
		}
		freeifaddrs(interfaces);
	}
#endif
	// Tailscale (100.64.0.0/10) after the home network's.
	std::stable_partition(found.begin(), found.end(), [](const std::string& address) {
		return address.rfind("100.", 0) != 0;
	});
	return found;
}

std::unique_ptr<Friend_listener> Friend_tcp_listen(int port) {
	if (!start_sockets()) {
		return nullptr;
	}
	addrinfo hints{};
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;
	addrinfo*         addresses = nullptr;
	const std::string service   = std::to_string(port);
	if (getaddrinfo(nullptr, service.c_str(), &hints, &addresses) != 0) {
		std::cerr << "Friend Mode: could not resolve listen address" << std::endl;
		return nullptr;
	}
	int listen_socket = -1;
	// Prefer one IPv6 socket that also takes IPv4 ("dual stack"): Windows
	// IPv6 sockets refuse IPv4 by default, and which family comes first
	// differs between systems.  Fall back to IPv4 alone.
	for (const bool want_ipv6 : {true, false}) {
		for (addrinfo* current = addresses; current && listen_socket < 0; current = current->ai_next) {
			if ((current->ai_family == AF_INET6) != want_ipv6) {
				continue;
			}
			listen_socket = static_cast<int>(socket(current->ai_family, current->ai_socktype, current->ai_protocol));
			if (listen_socket < 0) {
				continue;
			}
			int reuse = 1;
			setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
			if (want_ipv6) {
				int ipv6_only = 0;
				if (setsockopt(listen_socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&ipv6_only),
							   sizeof(ipv6_only))
					!= 0) {
					close_socket(listen_socket);    // Can't take IPv4 too.
					listen_socket = -1;
					continue;
				}
			}
			if (bind(listen_socket, current->ai_addr, current->ai_addrlen) == 0 && listen(listen_socket, 8) == 0
				&& set_nonblocking(listen_socket)) {
				std::cout << "Friend Mode: listening for " << (want_ipv6 ? "IPv4 and IPv6" : "IPv4") << std::endl;
				break;
			}
			close_socket(listen_socket);
			listen_socket = -1;
		}
		if (listen_socket >= 0) {
			break;
		}
	}
	freeaddrinfo(addresses);
	if (listen_socket < 0) {
		return nullptr;
	}
	return std::make_unique<Tcp_listener>(listen_socket);
}

std::unique_ptr<Friend_connection> Friend_tcp_connect(const std::string& address, int port, int timeout_ms) {
	if (!start_sockets()) {
		return nullptr;
	}
	addrinfo hints{};
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo*         addresses = nullptr;
	const std::string service   = std::to_string(port);
	if (getaddrinfo(address.c_str(), service.c_str(), &hints, &addresses) != 0) {
		std::cerr << "Friend Mode: could not resolve " << address << std::endl;
		return nullptr;
	}
	int peer = -1;
	for (addrinfo* current = addresses; current; current = current->ai_next) {
		peer = static_cast<int>(socket(current->ai_family, current->ai_socktype, current->ai_protocol));
		if (peer >= 0
			&& connect_with_timeout(peer, current->ai_addr, static_cast<socklen_t>(current->ai_addrlen), timeout_ms)) {
			break;
		}
		close_socket(peer);
		peer = -1;
	}
	freeaddrinfo(addresses);
	if (peer < 0) {
		return nullptr;
	}
	return std::make_unique<Tcp_connection>(peer);
}
