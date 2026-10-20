/*
 * Friend Mode listen-server session.
 *
 * The host accepts several guests, each a Peer with its own connection,
 * framing and reassembly; a guest has one Peer, the host.  Messages carry no
 * transport details: the bytes go through Friend_connection (friendconn.h).
 */
#ifndef FRIENDNET_H
#define FRIENDNET_H

#include "common_types.h"

#include <array>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

class Friend_connection;
class Friend_listener;

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
	// Players: the host is 0, guests 1 and up (never reused in a session).
	static constexpr int host_player = 0;
	// Recipient for every send: every guest (host), or the host (guest).
	static constexpr int everyone = -1;

	Friend_mode_session();
	~Friend_mode_session();

	bool start_host(int port, uint32 game_code);
	bool start_guest(const std::string& address, int port, uint32 game_code);
	void set_local_player_name(const std::string& name) {
		local_player_name = name;
	}
	// Host: guests beyond this many are turned away ("full").
	void set_guest_limit(int limit) {
		guest_limit = limit;
	}
	// Guest: the host's player name.
	const std::string& remote_player_name_value() const;
	// A player's name: this machine's own, the host's, or a guest's.
	std::string player_name(int player) const;
	// Guest: our player number, from the host (0 until accepted).
	int local_player() const {
		return local_player_id;
	}
	// Host: every guest past the handshake.
	std::vector<int> guest_players() const;
	void poll();
	void stop();
	// Variable-length data sent as raw bytes after a header packet.
	enum class Blob_kind : uint32 {
		world_save    = 1,
		chunk_objects   = 2,    // A map chunk's movable objects (item sync).
		actor_inventory = 3,    // A party member's inventory (item sync).
		guest_move      = 4,    // Guest drops the item it is holding.
		guest_lift      = 5,    // Guest picks an item up (drag start).
		guest_put_back  = 6,    // Guest cancels a drag, or returns the rest.
		guest_use_item  = 7,    // Guest uses an item (food, potion, key...).
		object_speech   = 8,    // Text spoken by a door, chest or other object.
		party_status    = 9,    // Party members' status flags, health, food.
		game_clock      = 10,   // Day, time and weather.
		creature_list   = 11,   // Creatures near the players, by network ID.
		roster_request  = 12,   // Guest wants the companion picker again.
		guest_attack    = 13,   // Guest's companion attacks a foe.
		journal         = 14,   // Host: every shared journal note.
		journal_edit    = 15,   // Guest: a finished note (new or changed).
		journal_lock    = 16,   // Guest: start or stop writing in a note.
		journal_locks   = 17,   // Host: who is writing in which note.
		journal_open    = 18,   // Either side: its journal opened or closed.
		music           = 19,   // Host: a music track started or stopped.
		projectile      = 20,   // Host: a missile in flight (picture only).
		sprite_effect   = 21,   // Host: a spell, explosion or blood sprite.
		actor_health    = 22,   // Host: health of creatures and NPCs near players.
		actor_hit       = 23,   // Host: someone was hit (damage, health left).
		world_sound     = 24,   // Host: a sound from the world (scripts, hits).
		guest_trade     = 25,   // Guest: a purchase from a merchant.
		story_flags     = 26,   // Host: the global (story) flags.
		resync_request  = 27,   // Guest: resend the party's packs and nearby items.
		player_list     = 28,   // Host: every player, their name and companion.
		notice          = 29,   // Host: a session notice (joined, left, switched).
		guest_follow    = 30,   // Guest: follow the Avatar (1) or not; host: the result.
		class_use       = 31,   // Guest: use a skill (Class System).
		class_state     = 32,   // Host: the Class System's rules and a guest's cooldowns.
		class_event     = 33,   // Host: someone used a skill (for its range circle).
		class_refused   = 34    // Host: why a guest's skill was refused.
	};
	void send_blob(Blob_kind kind, const std::vector<unsigned char>& data, int to = everyone, int except = -1);
	// 'from' (optional) is the sending player; on a guest always the host.
	bool take_blob(Blob_kind& kind, std::vector<unsigned char>& data, int* from = nullptr);
	// Guest: bytes of the blob currently arriving (0 when none).
	uint32 blob_bytes_remaining() const;
	// A world object (identified by tile, old shape and old frame) changed
	// shape and/or frame.
	struct Object_frame_event {
		int tx = 0, ty = 0, tz = 0;
		int shape     = 0;    // Old (or current) shape.
		int new_shape = 0;
		int old_frame = 0;
		int new_frame = 0;
	};
	void send_object_change(
			int tx, int ty, int tz, int old_shape, int new_shape, int old_frame, int new_frame);
	bool take_object_frame(Object_frame_event& event);
	// Guest asks the host to use a door or other usable object.
	void send_door_use(int tx, int ty, int tz, int shape, int frame);
	bool take_door_use(Object_frame_event& event, int* from = nullptr);
	// Host tells guests when it pauses (menus, dialogs) and resumes.
	void send_host_paused(bool paused, int to = everyone);
	bool take_host_paused(bool& paused);
	// Guest asks for the host's live world, sent as a world_save blob.
	void request_world();
	bool take_world_request(int* from = nullptr);
	// Host's full party (NPC numbers, in order) so guests can mirror it.
	void send_party_members(const std::vector<uint32>& members, int to = everyone);
	bool take_party_members(std::vector<uint32>& members);
	void send_party_roster(const std::vector<uint32>& actors, int to = everyone);
	void request_actor(uint32 actor);
	bool take_actor_request(uint32& actor, int* from = nullptr);
	void accept_actor(uint32 actor, int to);
	void reject_actor(uint32 actor, int to);
	bool roster_ready() const {
		return received_roster;
	}
	const std::vector<uint32>& party_roster() const {
		return roster;
	}
	// True once each time a complete roster arrives.
	bool take_roster_update() {
		const bool updated = roster_updated;
		roster_updated     = false;
		return updated;
	}
	bool take_actor_assignment(uint32& actor);
	// Guest: the companion we asked for was taken (true once).
	bool take_actor_rejected() {
		const bool rejected = actor_rejected;
		actor_rejected      = false;
		return rejected;
	}
	// Moves carry the guest's step number; the host's actor state carries
	// the number of the last step it handled, so the guest can tell an echo
	// of a step it already took from a correction.
	void send_move_request(int tx, int ty, int tz, int frame, uint32 seq);
	bool take_move_request(int& tx, int& ty, int& tz, int& frame, uint32& seq, int* from = nullptr);
	void send_actor_state(int tx, int ty, int tz, int frame, int to, uint32 seq = 0);
	bool take_actor_state(int& tx, int& ty, int& tz, int& frame, uint32& seq);
	void send_avatar_state(int tx, int ty, int tz, int frame);
	bool take_avatar_state(int& tx, int& ty, int& tz, int& frame);
	void send_npc_state(uint32 npc, int tx, int ty, int tz, int frame);
	bool take_npc_state(uint32& npc, int& tx, int& ty, int& tz, int& frame);
	void send_creature_state(uint32 creature, int tx, int ty, int tz, int frame);
	bool take_creature_state(uint32& creature, int& tx, int& ty, int& tz, int& frame);
	void send_speech(uint32 source, bool creature, const std::string& text);
	bool take_speech(uint32& source, bool& creature, std::string& text);
	// Speaker is the sender's controlled NPC number, or chat_spectator;
	// 'player' is who said it.  The host relays with 'except' the sender.
	static constexpr uint32 chat_spectator = 0xffffffffU;
	// Asked for instead of a companion: give it up and spectate.
	static constexpr uint32 spectate_actor = 0xffffffffU;
	void send_chat(uint32 speaker, int player, const std::string& text, int to = everyone, int except = -1);
	// 'target' (optional) is the player a whisper is for, or -1 for chat.
	bool take_chat(uint32& speaker, int& player, std::string& text, int* from = nullptr, int* target = nullptr);
	// A whisper: a guest sends it to the host for 'target'; the host passes
	// it on to that player alone.
	void send_whisper(int target, uint32 speaker, int player, const std::string& text, int to = everyone);
	// Suggest a reply: a guest names one of the Avatar's choices; the host
	// passes it on to everyone with who suggested it.
	void send_suggestion(int player, const std::string& choice, int to = everyone);
	bool take_suggestion(int& player, std::string& choice);
	void send_dialogue_begin();
	void send_dialogue_end();
	void send_dialogue_face(int shape, int frame, int slot);
	void send_dialogue_face_frame(int frame, int slot);
	void send_dialogue_remove_face(int shape);
	void send_dialogue_message(const std::string& text);
	void send_dialogue_choices(const std::vector<std::string>& choices);
	void send_dialogue_clear_choices();
	bool take_dialogue_event(Dialogue_event& event);
	// Once per connection, after the peer's player name arrives (on the
	// host, once per guest).
	bool take_peer_joined(int* player = nullptr);
	// Host: a guest that had joined is gone.
	bool take_peer_left(int& player, std::string& name);
	// Guest: the host is gone.
	bool take_connection_lost();
	// Guest: the host turned us away because its game is full.
	bool rejected_full() const {
		return full_rejection;
	}

	Mode mode() const {
		return session_mode;
	}
	// Host: some guest is past the handshake.  Guest: the host accepted us.
	bool connected() const;
	// Guest: a connection to the host is open (possibly still handshaking).
	bool peer_open() const;

private:
	static constexpr std::size_t packet_size = 20;
	enum class Text_kind : unsigned char {
		speech = 1,
		chat,
		identity,
		dialogue_message,
		dialogue_choice,
		whisper,
		suggestion
	};

	// One connection: the host's link to a guest, or a guest's to the host.
	struct Peer {
		int                                    player = 0;
		std::unique_ptr<Friend_connection>     link;
		std::array<unsigned char, packet_size> receive_buffer{};
		std::size_t                            received = 0;
		std::size_t                            sent     = 0;
		std::deque<std::vector<unsigned char>> outgoing;
		bool                                   handshake_complete = false;
		bool                                   joined             = false;    // Name arrived.
		std::string                            name;
		// Text arriving in chunks.
		bool        text_pending = false;
		Text_kind   text_kind    = Text_kind::speech;
		uint32      text_length  = 0;
		uint32      text_a       = 0;
		uint32      text_b       = 0;
		std::string text;
		// Blob arriving as raw bytes.
		uint32                     blob_kind      = 0;
		uint32                     blob_remaining = 0;
		std::vector<unsigned char> blob_data;
		// Host: the latest request of each kind from this guest.
		bool   world_requested       = false;
		bool   actor_request_pending = false;
		uint32 requested_actor       = 0;
		struct Move_request {
			uint32 position;
			uint32 frame;
			uint32 seq;
		};
		std::deque<Move_request> move_requests;
	};

	Mode                              session_mode = Mode::off;
	std::unique_ptr<Friend_listener>  listener;
	std::vector<std::unique_ptr<Peer>> peers;
	int                               next_player = 1;
	int                               guest_limit = 12;
	int                               local_player_id = 0;
	bool                              full_rejection  = false;
	uint32                            local_game_code = 0;
	// Packets of the message being sent, before they go to each recipient.
	std::deque<std::vector<unsigned char>> staged;
	std::vector<uint32>               roster;
	bool                              received_roster = false;
	bool                              roster_updated  = false;
	bool                              actor_assignment_pending = false;
	uint32                            assigned_actor = 0;
	bool                              actor_rejected = false;
	bool                              actor_state_pending = false;
	uint32                            authoritative_position = 0;
	uint32                            authoritative_frame = 0;
	uint32                            authoritative_seq   = 0;
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
	struct Chat_event {
		int         from;
		uint32      speaker;
		int         player;
		std::string text;
		int         target = -1;    // A whisper's player.
	};
	std::deque<Chat_event>               chat_events;
	std::deque<std::pair<int, std::string>> suggestions;    // Player, choice.
	std::deque<Dialogue_event>           dialogue_events;
	std::vector<std::string>             pending_dialogue_choices;
	bool                              connection_lost = false;
	std::string                       local_player_name;
	std::deque<int>                   joined_players;
	struct Left_player {
		int         player;
		std::string name;
	};
	std::deque<Left_player>           left_players;
	std::vector<uint32>               party_members;
	std::vector<uint32>               pending_party_members;
	bool                              party_members_ready = false;
	struct Blob {
		int                        from;
		Blob_kind                  kind;
		std::vector<unsigned char> data;
	};
	std::deque<Blob>                  blobs;
	std::deque<Object_frame_event>    object_frames;
	struct Door_use {
		int                from;
		Object_frame_event event;
	};
	std::deque<Door_use>              door_uses;
	bool                              host_paused         = false;
	bool                              host_paused_changed = false;

	unsigned char* stage_packet(unsigned char type);
	void           stage_text(Text_kind kind, const std::string& text, std::size_t limit, uint32 a = 0, uint32 b = 0);
	void           deliver(int to, int except = -1);
	void           send_identity(Peer& peer);
	void           handle_packet(Peer& peer);
	void           handle_text(Peer& peer);
	void           flush(Peer& peer);
	void           receive(Peer& peer);
	void           close_peer(Peer& peer);
	Peer*          host_peer() const;
};

#endif
