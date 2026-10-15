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
	void send_move_request(int tx, int ty, int tz, int frame);
	bool take_move_request(int& tx, int& ty, int& tz, int& frame);
	void send_actor_state(int tx, int ty, int tz, int frame);
	bool take_actor_state(int& tx, int& ty, int& tz, int& frame);
	void send_avatar_state(int tx, int ty, int tz, int frame);
	bool take_avatar_state(int& tx, int& ty, int& tz, int& frame);
	void send_npc_state(uint32 npc, int tx, int ty, int tz, int frame);
	bool take_npc_state(uint32& npc, int& tx, int& ty, int& tz, int& frame);
	void send_creature_state(uint32 creature, int tx, int ty, int tz, int frame);
	bool take_creature_state(uint32& creature, int& tx, int& ty, int& tz, int& frame);
	void send_speech(uint32 source, bool creature, const std::string& text);
	bool take_speech(uint32& source, bool& creature, std::string& text);
	bool take_connection_lost();

	Mode mode() const {
		return session_mode;
	}
	bool connected() const {
		return handshake_complete;
	}

private:
	static constexpr std::size_t packet_size = 20;

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
	bool                              move_request_pending = false;
	uint32                            requested_position = 0;
	uint32                            requested_frame = 0;
	bool                              actor_state_pending = false;
	uint32                            authoritative_position = 0;
	uint32                            authoritative_frame = 0;
	bool                              avatar_state_pending = false;
	uint32                            authoritative_avatar_position = 0;
	uint32                            authoritative_avatar_frame = 0;
	struct Npc_state {
		uint32 npc;
		uint32 position;
		uint32 frame;
	};
	std::deque<Npc_state>             npc_states;
	std::deque<Npc_state>             creature_states;
	struct Speech_event {
		uint32 source;
		std::string text;
	};
	std::deque<Speech_event>             speech_events;
	bool                                 speech_pending = false;
	uint32                               pending_speech_source = 0;
	uint32                               pending_speech_length = 0;
	std::string                          pending_speech_text;
	bool                              connection_lost = false;

	void close_peer();
	void queue_packet(unsigned char type);
	void handle_packet();
};

#endif
