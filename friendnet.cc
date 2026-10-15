#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendnet.h"

#include <cerrno>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#else
#	include <fcntl.h>
#	include <netdb.h>
#	include <sys/socket.h>
#	include <unistd.h>
#endif

namespace {
constexpr unsigned short protocol_version = 1;
constexpr unsigned char  hello_packet      = 1;
constexpr unsigned char  accepted_packet   = 2;
constexpr unsigned char  rejected_packet   = 3;
constexpr unsigned char  roster_begin_packet = 4;
constexpr unsigned char  roster_actor_packet = 5;
constexpr unsigned char  roster_end_packet = 6;
constexpr unsigned char  actor_request_packet = 7;
constexpr unsigned char  actor_accepted_packet = 8;
constexpr unsigned char  actor_rejected_packet = 9;

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

bool would_block() {
#ifdef _WIN32
	const int error = WSAGetLastError();
	return error == WSAEWOULDBLOCK;
#else
	return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

void write_u32(unsigned char* out, uint32 value) {
	out[0] = static_cast<unsigned char>(value >> 24);
	out[1] = static_cast<unsigned char>(value >> 16);
	out[2] = static_cast<unsigned char>(value >> 8);
	out[3] = static_cast<unsigned char>(value);
}

uint32 read_u32(const unsigned char* in) {
	return (static_cast<uint32>(in[0]) << 24) | (static_cast<uint32>(in[1]) << 16)
		   | (static_cast<uint32>(in[2]) << 8) | static_cast<uint32>(in[3]);
}
}    // namespace

Friend_mode_session::~Friend_mode_session() {
	stop();
}

bool Friend_mode_session::start_host(int port, uint32 game_code) {
	stop();
	local_game_code = game_code;

	addrinfo hints{};
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;
	addrinfo* addresses = nullptr;
	const std::string service = std::to_string(port);
	if (getaddrinfo(nullptr, service.c_str(), &hints, &addresses) != 0) {
		std::cerr << "Friend Mode: could not resolve listen address" << std::endl;
		return false;
	}
	for (addrinfo* current = addresses; current; current = current->ai_next) {
		listen_socket = static_cast<int>(socket(current->ai_family, current->ai_socktype, current->ai_protocol));
		if (listen_socket < 0) {
			continue;
		}
		int reuse = 1;
		setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
		if (bind(listen_socket, current->ai_addr, current->ai_addrlen) == 0 && listen(listen_socket, 1) == 0
			&& set_nonblocking(listen_socket)) {
			break;
		}
		close_socket(listen_socket);
		listen_socket = -1;
	}
	freeaddrinfo(addresses);
	if (listen_socket < 0) {
		std::cerr << "Friend Mode: could not listen on port " << port << std::endl;
		return false;
	}
	session_mode = Mode::host;
	std::cout << "Friend Mode: host listening on port " << port << std::endl;
	return true;
}

bool Friend_mode_session::start_guest(const std::string& address, int port, uint32 game_code) {
	stop();
	local_game_code = game_code;

	addrinfo hints{};
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* addresses = nullptr;
	const std::string service = std::to_string(port);
	if (getaddrinfo(address.c_str(), service.c_str(), &hints, &addresses) != 0) {
		std::cerr << "Friend Mode: could not resolve " << address << std::endl;
		return false;
	}
	for (addrinfo* current = addresses; current; current = current->ai_next) {
		peer_socket = static_cast<int>(socket(current->ai_family, current->ai_socktype, current->ai_protocol));
		if (peer_socket >= 0 && connect(peer_socket, current->ai_addr, current->ai_addrlen) == 0) {
			break;
		}
		close_socket(peer_socket);
		peer_socket = -1;
	}
	freeaddrinfo(addresses);
	if (peer_socket < 0 || !set_nonblocking(peer_socket)) {
		close_peer();
		std::cerr << "Friend Mode: could not connect to " << address << ':' << port << std::endl;
		return false;
	}
	session_mode = Mode::guest;
	queue_packet(hello_packet);
	std::cout << "Friend Mode: connected to host; validating session" << std::endl;
	return true;
}

void Friend_mode_session::queue_packet(unsigned char type) {
	std::array<unsigned char, packet_size> packet = {'U', '7', 'F', 'M', static_cast<unsigned char>(protocol_version >> 8),
			static_cast<unsigned char>(protocol_version), type, 0, 0, 0, 0, 0};
	write_u32(packet.data() + 8, local_game_code);
	outgoing.push_back(packet);
}

void Friend_mode_session::send_party_roster(const std::vector<uint32>& actors) {
	queue_packet(roster_begin_packet);
	for (uint32 actor : actors) {
		queue_packet(roster_actor_packet);
		write_u32(outgoing.back().data() + 8, actor);
	}
	queue_packet(roster_end_packet);
}

void Friend_mode_session::request_actor(uint32 actor) {
	queue_packet(actor_request_packet);
	write_u32(outgoing.back().data() + 8, actor);
}

bool Friend_mode_session::take_actor_request(uint32& actor) {
	if (!actor_request_pending) {
		return false;
	}
	actor_request_pending = false;
	actor = requested_actor;
	return true;
}

void Friend_mode_session::accept_actor(uint32 actor) {
	queue_packet(actor_accepted_packet);
	write_u32(outgoing.back().data() + 8, actor);
}

void Friend_mode_session::reject_actor(uint32 actor) {
	queue_packet(actor_rejected_packet);
	write_u32(outgoing.back().data() + 8, actor);
}

bool Friend_mode_session::take_actor_assignment(uint32& actor) {
	if (!actor_assignment_pending) {
		return false;
	}
	actor_assignment_pending = false;
	actor = assigned_actor;
	return true;
}

void Friend_mode_session::handle_packet() {
	const bool valid_header = receive_buffer[0] == 'U' && receive_buffer[1] == '7' && receive_buffer[2] == 'F'
						  && receive_buffer[3] == 'M';
	const unsigned short version = static_cast<unsigned short>((receive_buffer[4] << 8) | receive_buffer[5]);
	const unsigned char  type    = receive_buffer[6];
	const uint32         game    = read_u32(receive_buffer.data() + 8);
	received                     = 0;

	if (session_mode == Mode::host && type == hello_packet) {
		if (valid_header && version == protocol_version && game == local_game_code) {
			handshake_complete = true;
			queue_packet(accepted_packet);
			std::cout << "Friend Mode: guest accepted" << std::endl;
		} else {
			queue_packet(rejected_packet);
			std::cerr << "Friend Mode: guest rejected (protocol or game mismatch)" << std::endl;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == accepted_packet
			   && game == local_game_code) {
		handshake_complete = true;
		std::cout << "Friend Mode: session accepted by host" << std::endl;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == roster_begin_packet) {
		roster.clear();
		received_roster = false;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == roster_actor_packet) {
		roster.push_back(game);
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == roster_end_packet) {
		received_roster = true;
	} else if (session_mode == Mode::host && valid_header && version == protocol_version && type == actor_request_packet) {
		requested_actor = game;
		actor_request_pending = true;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == actor_accepted_packet) {
		assigned_actor = game;
		actor_assignment_pending = true;
		std::cout << "Friend Mode: companion assignment accepted" << std::endl;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == actor_rejected_packet) {
		std::cerr << "Friend Mode: selected companion is no longer available" << std::endl;
	} else {
		std::cerr << "Friend Mode: handshake rejected" << std::endl;
		close_peer();
	}
}

void Friend_mode_session::poll() {
	if (session_mode == Mode::host && peer_socket < 0) {
		peer_socket = static_cast<int>(accept(listen_socket, nullptr, nullptr));
		if (peer_socket >= 0) {
			set_nonblocking(peer_socket);
			std::cout << "Friend Mode: guest connected; validating session" << std::endl;
		} else if (!would_block()) {
			std::cerr << "Friend Mode: accept failed" << std::endl;
		}
	}
	if (peer_socket < 0) {
		return;
	}
	if (!outgoing.empty()) {
		const int count = static_cast<int>(send(peer_socket, reinterpret_cast<const char*>(outgoing.front().data() + sent),
				packet_size - sent, 0));
		if (count > 0) {
			sent += static_cast<std::size_t>(count);
			if (sent == packet_size) {
				outgoing.pop_front();
				sent = 0;
			}
		} else if (count < 0 && !would_block()) {
			close_peer();
			return;
		}
	}
	const int count = static_cast<int>(recv(peer_socket, reinterpret_cast<char*>(receive_buffer.data() + received),
			packet_size - received, 0));
	if (count > 0) {
		received += static_cast<std::size_t>(count);
		if (received == packet_size) {
			handle_packet();
		}
	} else if (count == 0 || (count < 0 && !would_block())) {
		std::cout << "Friend Mode: peer disconnected" << std::endl;
		close_peer();
	}
}

void Friend_mode_session::close_peer() {
	close_socket(peer_socket);
	peer_socket        = -1;
	received           = 0;
	sent               = 0;
	handshake_complete = false;
	outgoing.clear();
	roster.clear();
	received_roster = false;
	actor_request_pending = false;
	actor_assignment_pending = false;
}

void Friend_mode_session::stop() {
	close_peer();
	close_socket(listen_socket);
	listen_socket = -1;
	session_mode  = Mode::off;
}
