/*
 * Minimal Friend Mode listen-server transport.
 *
 * This first slice performs only a versioned game-compatibility handshake.
 * Gameplay messages will be added after controller authorization is defined.
 */
#ifndef FRIENDNET_H
#define FRIENDNET_H

#include "common_types.h"

#include <array>
#include <cstddef>
#include <string>

class Friend_mode_session {
public:
	enum class Mode { off, host, guest };

	Friend_mode_session() = default;
	~Friend_mode_session();

	bool start_host(int port, uint32 game_code);
	bool start_guest(const std::string& address, int port, uint32 game_code);
	void poll();
	void stop();

	Mode mode() const {
		return session_mode;
	}
	bool connected() const {
		return handshake_complete;
	}

private:
	static constexpr std::size_t packet_size = 12;

	Mode                              session_mode = Mode::off;
	int                               listen_socket = -1;
	int                               peer_socket   = -1;
	uint32                            local_game_code = 0;
	std::array<unsigned char, packet_size> receive_buffer{};
	std::array<unsigned char, packet_size> send_buffer{};
	std::size_t                       received = 0;
	std::size_t                       sent     = packet_size;
	bool                              handshake_complete = false;

	void close_peer();
	void queue_packet(unsigned char type);
	void handle_packet();
};

#endif
