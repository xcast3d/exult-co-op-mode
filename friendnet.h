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
#include <deque>
#include <string>
#include <vector>

class Friend_mode_session {
public:
	enum class Mode { off, host, guest };

	Friend_mode_session() = default;
	~Friend_mode_session();

	bool start_host(int port, uint32 game_code);
	bool start_guest(const std::string& address, int port, uint32 game_code);
	void poll();
	void stop();
	void send_party_roster(const std::vector<uint32>& actors);
	void request_actor(uint32 actor);
	bool take_actor_request(uint32& actor);
	void accept_actor(uint32 actor);
	void reject_actor(uint32 actor);
	bool roster_ready() const {
		return received_roster;
	}
	const std::vector<uint32>& party_roster() const {
		return roster;
	}
	bool take_actor_assignment(uint32& actor);

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
	std::size_t                       received = 0;
	std::size_t                       sent     = 0;
	bool                              handshake_complete = false;
	std::deque<std::array<unsigned char, packet_size>> outgoing;
	std::vector<uint32>               roster;
	bool                              received_roster = false;
	bool                              actor_request_pending = false;
	uint32                            requested_actor = 0;
	bool                              actor_assignment_pending = false;
	uint32                            assigned_actor = 0;

	void close_peer();
	void queue_packet(unsigned char type);
	void handle_packet();
};

#endif
