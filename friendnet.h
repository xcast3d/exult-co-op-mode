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
	enum class Dialogue_event_type { begin, end, face, face_frame, remove_face, message, choices, clear_choices };
	struct Dialogue_event {
		Dialogue_event_type type;
		int a = 0;
		int b = 0;
		int c = 0;
		std::string text;
		std::vector<std::string> choices;
	};

	Friend_mode_session() = default;
	~Friend_mode_session();

	bool start_host(int port, uint32 game_code);
	bool start_guest(const std::string& address, int port, uint32 game_code);
	void set_local_player_name(const std::string& name) {
		local_player_name = name;
	}
	const std::string& remote_player_name_value() const {
		return remote_player_name;
	}
	void poll();
	void stop();
	// Host's full party (NPC numbers, in order) so guests can mirror it.
	void send_party_members(const std::vector<uint32>& members);
	bool take_party_members(std::vector<uint32>& members);
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
	// Speaker is the sender's controlled NPC number, or chat_spectator.
	static constexpr uint32 chat_spectator = 0xffffffffU;
	void send_chat(uint32 speaker, const std::string& text);
	bool take_chat(uint32& speaker, std::string& text);
	void send_dialogue_begin();
	void send_dialogue_end();
	void send_dialogue_face(int shape, int frame, int slot);
	void send_dialogue_face_frame(int frame, int slot);
	void send_dialogue_remove_face(int shape);
	void send_dialogue_message(const std::string& text);
	void send_dialogue_choices(const std::vector<std::string>& choices);
	void send_dialogue_clear_choices();
	bool take_dialogue_event(Dialogue_event& event);
	// True once per connection, after the peer's player name arrives.
	bool take_peer_joined();
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
	struct Chat_event {
		uint32      speaker;
		std::string text;
	};
	std::deque<Chat_event>               chat_events;
	bool                                 chat_pending = false;
	uint32                               pending_chat_speaker = 0;
	uint32                               pending_chat_length = 0;
	uint32                               pending_chat_received = 0;
	std::string                          pending_chat_text;
	std::deque<Dialogue_event>           dialogue_events;
	bool                                 dialogue_text_pending = false;
	Dialogue_event_type                  pending_dialogue_text_type = Dialogue_event_type::message;
	uint32                               pending_dialogue_text_index = 0;
	uint32                               pending_dialogue_text_length = 0;
	std::string                          pending_dialogue_text;
	std::vector<std::string>             pending_dialogue_choices;
	bool                              connection_lost = false;
	std::string                       local_player_name;
	std::string                       remote_player_name;
	std::string                       pending_identity;
	uint32                            pending_identity_length = 0;
	uint32                            pending_identity_received = 0;
	bool                              identity_pending = false;
	bool                              peer_joined      = false;
	std::vector<uint32>               party_members;
	std::vector<uint32>               pending_party_members;
	bool                              party_members_ready = false;

	void close_peer();
	void queue_packet(unsigned char type);
	void send_identity();
	void handle_packet();
};

#endif
