#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendnet.h"

#include "friendconn.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace {
constexpr unsigned short protocol_version = 31;
constexpr unsigned char  hello_packet      = 1;
constexpr unsigned char  accepted_packet   = 2;
constexpr unsigned char  rejected_packet   = 3;
constexpr unsigned char  roster_begin_packet = 4;
constexpr unsigned char  roster_actor_packet = 5;
constexpr unsigned char  roster_end_packet = 6;
constexpr unsigned char  actor_request_packet = 7;
constexpr unsigned char  actor_accepted_packet = 8;
constexpr unsigned char  actor_rejected_packet = 9;
constexpr unsigned char  move_request_packet = 10;
constexpr unsigned char  actor_state_packet = 11;
constexpr unsigned char  avatar_state_packet = 12;
constexpr unsigned char  npc_state_packet = 13;
constexpr unsigned char  creature_state_packet = 14;
constexpr unsigned char  dialogue_begin_packet = 17;
constexpr unsigned char  dialogue_end_packet = 18;
constexpr unsigned char  dialogue_face_packet = 19;
constexpr unsigned char  dialogue_face_frame_packet = 20;
constexpr unsigned char  dialogue_remove_face_packet = 21;
constexpr unsigned char  dialogue_choices_begin_packet = 24;
constexpr unsigned char  dialogue_choices_show_packet = 26;
constexpr unsigned char  dialogue_clear_choices_packet = 27;
constexpr unsigned char  party_begin_packet = 32;
constexpr unsigned char  party_member_packet = 33;
constexpr unsigned char  party_end_packet = 34;
// A blob header is followed by 'length' raw bytes outside packet framing.
constexpr unsigned char  blob_packet = 35;
constexpr unsigned char  world_request_packet = 36;
constexpr unsigned char  object_frame_packet = 37;
constexpr unsigned char  door_use_packet = 38;
constexpr unsigned char  host_paused_packet = 39;
// Text of any kind (speech, chat, names, dialogue): a header with the kind
// (byte 7), length and two values, then 12 bytes per chunk.
constexpr unsigned char  text_begin_packet = 40;
constexpr unsigned char  text_chunk_packet = 41;
constexpr std::size_t    text_chunk_size   = 12;
constexpr uint32         reject_mismatch   = 1;
constexpr uint32         reject_full       = 2;
constexpr uint32         max_blob_size = 16 * 1024 * 1024;
constexpr std::size_t    blob_chunk_size = 16 * 1024;

// Keep only printable ASCII from peer-supplied text.
std::string printable(const std::string& text) {
	std::string out;
	for (const char c : text) {
		if (c >= 32 && c < 127) {
			out.push_back(c);
		}
	}
	return out;
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

uint32 pack_position(int tx, int ty, int tz) {
	return (static_cast<uint32>(tx) & 0xfffU) | ((static_cast<uint32>(ty) & 0xfffU) << 12)
		   | ((static_cast<uint32>(tz) & 0x1fU) << 24);
}

void unpack_position(uint32 packed, int& tx, int& ty, int& tz) {
	tx = packed & 0xfffU;
	ty = (packed >> 12) & 0xfffU;
	tz = (packed >> 24) & 0x1fU;
}
}    // namespace

Friend_mode_session::Friend_mode_session() = default;

Friend_mode_session::~Friend_mode_session() {
	stop();
}

bool Friend_mode_session::start_host(int port, uint32 game_code) {
	stop();
	connection_lost = false;
	local_game_code = game_code;
	listener        = Friend_tcp_listen(port);
	if (!listener) {
		std::cerr << "Friend Mode: could not listen on port " << port << std::endl;
		return false;
	}
	session_mode = Mode::host;
	std::cout << "Friend Mode: host listening on port " << port << std::endl;
	return true;
}

bool Friend_mode_session::start_guest(const std::string& address, int port, uint32 game_code) {
	stop();
	connection_lost = false;
	full_rejection  = false;
	local_game_code = game_code;
	auto link       = Friend_tcp_connect(address, port, 5000);
	if (!link) {
		std::cerr << "Friend Mode: could not connect to " << address << ':' << port << std::endl;
		return false;
	}
	auto peer    = std::make_unique<Peer>();
	peer->player = host_player;
	peer->link   = std::move(link);
	peers.push_back(std::move(peer));
	session_mode = Mode::guest;
	stage_packet(hello_packet);
	deliver(everyone);
	std::cout << "Friend Mode: connected to host; validating session" << std::endl;
	return true;
}

Friend_mode_session::Peer* Friend_mode_session::host_peer() const {
	return session_mode == Mode::guest && !peers.empty() ? peers.front().get() : nullptr;
}

const std::string& Friend_mode_session::remote_player_name_value() const {
	static const std::string none;
	const Peer*              host = host_peer();
	return host ? host->name : none;
}

std::string Friend_mode_session::player_name(int player) const {
	if ((session_mode == Mode::host && player == host_player)
		|| (session_mode == Mode::guest && player == local_player_id && player != host_player)) {
		return local_player_name;
	}
	for (const auto& peer : peers) {
		if (peer->player == player) {
			return peer->name;
		}
	}
	return std::string();
}

std::vector<int> Friend_mode_session::guest_players() const {
	std::vector<int> out;
	if (session_mode == Mode::host) {
		for (const auto& peer : peers) {
			if (peer->link && peer->handshake_complete) {
				out.push_back(peer->player);
			}
		}
	}
	return out;
}

bool Friend_mode_session::connected() const {
	for (const auto& peer : peers) {
		if (peer->link && peer->handshake_complete) {
			return true;
		}
	}
	return false;
}

bool Friend_mode_session::peer_open() const {
	const Peer* host = host_peer();
	return host && host->link;
}

unsigned char* Friend_mode_session::stage_packet(unsigned char type) {
	std::vector<unsigned char> packet = {'U', '7', 'F', 'M', static_cast<unsigned char>(protocol_version >> 8),
			static_cast<unsigned char>(protocol_version), type, 0};
	packet.resize(packet_size, 0);
	write_u32(packet.data() + 8, local_game_code);
	staged.push_back(std::move(packet));
	return staged.back().data();
}

void Friend_mode_session::stage_text(
		Text_kind kind, const std::string& text, std::size_t limit, uint32 a, uint32 b) {
	const std::size_t length = std::min(text.size(), limit);
	unsigned char*    begin  = stage_packet(text_begin_packet);
	begin[7]                 = static_cast<unsigned char>(kind);
	write_u32(begin + 8, static_cast<uint32>(length));
	write_u32(begin + 12, a);
	write_u32(begin + 16, b);
	for (std::size_t offset = 0; offset < length; offset += text_chunk_size) {
		unsigned char*    chunk = stage_packet(text_chunk_packet);
		const std::size_t count = std::min(text_chunk_size, length - offset);
		std::memcpy(chunk + 8, text.data() + offset, count);
	}
}

// Hand the staged packets to every recipient: the host's guests past the
// handshake ('to' one of them, or everyone but 'except'), or the host.
void Friend_mode_session::deliver(int to, int except) {
	for (auto& peer : peers) {
		if (!peer->link) {
			continue;
		}
		if (session_mode == Mode::host
			&& (!peer->handshake_complete || (to != everyone && peer->player != to) || peer->player == except)) {
			continue;
		}
		peer->outgoing.insert(peer->outgoing.end(), staged.begin(), staged.end());
	}
	staged.clear();
}

void Friend_mode_session::send_blob(Blob_kind kind, const std::vector<unsigned char>& data, int to, int except) {
	if (data.size() > max_blob_size) {
		std::cerr << "Friend Mode: blob too large (" << data.size() << " bytes)" << std::endl;
		return;
	}
	unsigned char* header = stage_packet(blob_packet);
	write_u32(header + 8, static_cast<uint32>(data.size()));
	write_u32(header + 12, static_cast<uint32>(kind));
	for (std::size_t offset = 0; offset < data.size(); offset += blob_chunk_size) {
		const std::size_t count = std::min(blob_chunk_size, data.size() - offset);
		staged.emplace_back(data.begin() + offset, data.begin() + offset + count);
	}
	deliver(to, except);
}

bool Friend_mode_session::take_blob(Blob_kind& kind, std::vector<unsigned char>& data, int* from) {
	if (blobs.empty()) {
		return false;
	}
	kind = blobs.front().kind;
	data = std::move(blobs.front().data);
	if (from) {
		*from = blobs.front().from;
	}
	blobs.pop_front();
	return true;
}

uint32 Friend_mode_session::blob_bytes_remaining() const {
	const Peer* host = host_peer();
	return host ? host->blob_remaining : 0;
}

void Friend_mode_session::send_object_change(
		int tx, int ty, int tz, int old_shape, int new_shape, int old_frame, int new_frame) {
	unsigned char* packet = stage_packet(object_frame_packet);
	write_u32(packet + 8, pack_position(tx, ty, tz));
	write_u32(packet + 12, static_cast<uint32>((old_shape & 0xffff) | ((new_shape & 0xffff) << 16)));
	write_u32(packet + 16, static_cast<uint32>((old_frame & 0xff) | ((new_frame & 0xff) << 8)));
	deliver(everyone);
}

bool Friend_mode_session::take_object_frame(Object_frame_event& event) {
	if (object_frames.empty()) {
		return false;
	}
	event = object_frames.front();
	object_frames.pop_front();
	return true;
}

void Friend_mode_session::send_door_use(int tx, int ty, int tz, int shape, int frame) {
	unsigned char* packet = stage_packet(door_use_packet);
	write_u32(packet + 8, pack_position(tx, ty, tz));
	write_u32(packet + 12, static_cast<uint32>(shape));
	write_u32(packet + 16, static_cast<uint32>(frame));
	deliver(everyone);
}

bool Friend_mode_session::take_door_use(Object_frame_event& event, int* from) {
	if (door_uses.empty()) {
		return false;
	}
	event = door_uses.front().event;
	if (from) {
		*from = door_uses.front().from;
	}
	door_uses.pop_front();
	return true;
}

void Friend_mode_session::send_host_paused(bool paused, int to) {
	write_u32(stage_packet(host_paused_packet) + 8, paused ? 1U : 0U);
	deliver(to);
}

bool Friend_mode_session::take_host_paused(bool& paused) {
	if (!host_paused_changed) {
		return false;
	}
	paused              = host_paused;
	host_paused_changed = false;
	return true;
}

void Friend_mode_session::request_world() {
	stage_packet(world_request_packet);
	deliver(everyone);
}

bool Friend_mode_session::take_world_request(int* from) {
	for (auto& peer : peers) {
		if (peer->link && peer->world_requested) {
			peer->world_requested = false;
			if (from) {
				*from = peer->player;
			}
			return true;
		}
	}
	return false;
}

void Friend_mode_session::send_identity(Peer& peer) {
	stage_text(Text_kind::identity, local_player_name, 20);
	peer.outgoing.insert(peer.outgoing.end(), staged.begin(), staged.end());
	staged.clear();
}

void Friend_mode_session::send_party_members(const std::vector<uint32>& members, int to) {
	stage_packet(party_begin_packet);
	for (uint32 member : members) {
		write_u32(stage_packet(party_member_packet) + 8, member);
	}
	stage_packet(party_end_packet);
	deliver(to);
}

bool Friend_mode_session::take_party_members(std::vector<uint32>& members) {
	if (!party_members_ready) {
		return false;
	}
	members             = party_members;
	party_members_ready = false;
	return true;
}

void Friend_mode_session::send_party_roster(const std::vector<uint32>& actors, int to) {
	stage_packet(roster_begin_packet);
	for (uint32 actor : actors) {
		write_u32(stage_packet(roster_actor_packet) + 8, actor);
	}
	stage_packet(roster_end_packet);
	deliver(to);
}

void Friend_mode_session::request_actor(uint32 actor) {
	write_u32(stage_packet(actor_request_packet) + 8, actor);
	deliver(everyone);
}

bool Friend_mode_session::take_actor_request(uint32& actor, int* from) {
	for (auto& peer : peers) {
		if (peer->link && peer->actor_request_pending) {
			peer->actor_request_pending = false;
			actor                       = peer->requested_actor;
			if (from) {
				*from = peer->player;
			}
			return true;
		}
	}
	return false;
}

void Friend_mode_session::accept_actor(uint32 actor, int to) {
	write_u32(stage_packet(actor_accepted_packet) + 8, actor);
	deliver(to);
}

void Friend_mode_session::reject_actor(uint32 actor, int to) {
	write_u32(stage_packet(actor_rejected_packet) + 8, actor);
	deliver(to);
}

bool Friend_mode_session::take_actor_assignment(uint32& actor) {
	if (!actor_assignment_pending) {
		return false;
	}
	actor_assignment_pending = false;
	actor = assigned_actor;
	return true;
}

void Friend_mode_session::send_move_request(int tx, int ty, int tz, int frame, uint32 seq) {
	unsigned char* packet = stage_packet(move_request_packet);
	write_u32(packet + 8, pack_position(tx, ty, tz));
	write_u32(packet + 12, static_cast<uint32>(frame));
	write_u32(packet + 16, seq);
	deliver(everyone);
}

// Every step, in order: several can arrive together over the internet, and
// keeping only the last would look like a jump the host refuses.
bool Friend_mode_session::take_move_request(int& tx, int& ty, int& tz, int& frame, uint32& seq, int* from) {
	for (auto& peer : peers) {
		if (peer->link && !peer->move_requests.empty()) {
			const Peer::Move_request request = peer->move_requests.front();
			peer->move_requests.pop_front();
			unpack_position(request.position, tx, ty, tz);
			frame = static_cast<int>(request.frame);
			seq   = request.seq;
			if (from) {
				*from = peer->player;
			}
			return true;
		}
	}
	return false;
}

void Friend_mode_session::send_actor_state(int tx, int ty, int tz, int frame, int to, uint32 seq) {
	unsigned char* packet = stage_packet(actor_state_packet);
	write_u32(packet + 8, pack_position(tx, ty, tz));
	write_u32(packet + 12, static_cast<uint32>(frame));
	write_u32(packet + 16, seq);
	deliver(to);
}

bool Friend_mode_session::take_actor_state(int& tx, int& ty, int& tz, int& frame, uint32& seq) {
	if (!actor_state_pending) {
		return false;
	}
	actor_state_pending = false;
	unpack_position(authoritative_position, tx, ty, tz);
	frame = static_cast<int>(authoritative_frame);
	seq   = authoritative_seq;
	return true;
}

void Friend_mode_session::send_avatar_state(int tx, int ty, int tz, int frame) {
	unsigned char* packet = stage_packet(avatar_state_packet);
	write_u32(packet + 8, pack_position(tx, ty, tz));
	write_u32(packet + 12, static_cast<uint32>(frame));
	deliver(everyone);
}

bool Friend_mode_session::take_avatar_state(int& tx, int& ty, int& tz, int& frame) {
	if (!avatar_state_pending) {
		return false;
	}
	avatar_state_pending = false;
	unpack_position(authoritative_avatar_position, tx, ty, tz);
	frame = static_cast<int>(authoritative_avatar_frame);
	return true;
}

void Friend_mode_session::send_npc_state(uint32 npc, int tx, int ty, int tz, int frame) {
	unsigned char* packet = stage_packet(npc_state_packet);
	write_u32(packet + 8, npc);
	write_u32(packet + 12, pack_position(tx, ty, tz));
	write_u32(packet + 16, static_cast<uint32>(frame));
	deliver(everyone);
}

bool Friend_mode_session::take_npc_state(uint32& npc, int& tx, int& ty, int& tz, int& frame) {
	if (npc_states.empty()) {
		return false;
	}
	npc = npc_states.front().npc;
	unpack_position(npc_states.front().position, tx, ty, tz);
	frame = static_cast<int>(npc_states.front().frame);
	npc_states.pop_front();
	return true;
}

void Friend_mode_session::send_creature_state(uint32 creature, int tx, int ty, int tz, int frame) {
	unsigned char* packet = stage_packet(creature_state_packet);
	write_u32(packet + 8, creature);
	write_u32(packet + 12, pack_position(tx, ty, tz));
	write_u32(packet + 16, static_cast<uint32>(frame));
	deliver(everyone);
}

bool Friend_mode_session::take_creature_state(uint32& creature, int& tx, int& ty, int& tz, int& frame) {
	if (creature_states.empty()) {
		return false;
	}
	creature = creature_states.front().npc;
	unpack_position(creature_states.front().position, tx, ty, tz);
	frame = static_cast<int>(creature_states.front().frame);
	creature_states.pop_front();
	return true;
}

void Friend_mode_session::send_speech(uint32 source, bool creature, const std::string& text) {
	stage_text(Text_kind::speech, text, 1024, source | (creature ? 0x80000000U : 0U));
	deliver(everyone);
}

bool Friend_mode_session::take_speech(uint32& source, bool& creature, std::string& text) {
	if (speech_events.empty()) {
		return false;
	}
	const uint32 encoded = speech_events.front().source;
	source = encoded & 0x7fffffffU;
	creature = (encoded & 0x80000000U) != 0;
	text = std::move(speech_events.front().text);
	speech_events.pop_front();
	return true;
}

void Friend_mode_session::send_chat(uint32 speaker, int player, const std::string& text, int to, int except) {
	stage_text(Text_kind::chat, text, 80, speaker, static_cast<uint32>(player));
	deliver(to, except);
}

void Friend_mode_session::send_whisper(int target, uint32 speaker, int player, const std::string& text, int to) {
	// To the host: the target and who we are.  From the host: the speaker
	// and who said it (to the target alone).
	if (session_mode == Mode::guest) {
		stage_text(Text_kind::whisper, text, 80, static_cast<uint32>(target), static_cast<uint32>(player));
	} else {
		stage_text(Text_kind::whisper, text, 80, speaker, static_cast<uint32>(player));
	}
	deliver(to);
}

void Friend_mode_session::send_suggestion(int player, const std::string& choice, int to) {
	stage_text(Text_kind::suggestion, choice, 80, 0, static_cast<uint32>(player));
	deliver(to);
}

bool Friend_mode_session::take_suggestion(int& player, std::string& choice) {
	if (suggestions.empty()) {
		return false;
	}
	player = suggestions.front().first;
	choice = std::move(suggestions.front().second);
	suggestions.pop_front();
	return true;
}

bool Friend_mode_session::take_chat(uint32& speaker, int& player, std::string& text, int* from, int* target) {
	if (chat_events.empty()) {
		return false;
	}
	if (target) {
		*target = chat_events.front().target;
	}
	speaker = chat_events.front().speaker;
	player  = chat_events.front().player;
	text    = std::move(chat_events.front().text);
	if (from) {
		*from = chat_events.front().from;
	}
	chat_events.pop_front();
	return true;
}

void Friend_mode_session::send_dialogue_begin() {
	stage_packet(dialogue_begin_packet);
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_end() {
	stage_packet(dialogue_end_packet);
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_face(int shape, int frame, int slot) {
	unsigned char* packet = stage_packet(dialogue_face_packet);
	write_u32(packet + 8, static_cast<uint32>(shape));
	write_u32(packet + 12, static_cast<uint32>(frame));
	write_u32(packet + 16, static_cast<uint32>(slot));
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_face_frame(int frame, int slot) {
	unsigned char* packet = stage_packet(dialogue_face_frame_packet);
	write_u32(packet + 8, static_cast<uint32>(frame));
	write_u32(packet + 12, static_cast<uint32>(slot));
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_remove_face(int shape) {
	write_u32(stage_packet(dialogue_remove_face_packet) + 8, static_cast<uint32>(shape));
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_message(const std::string& text) {
	stage_text(Text_kind::dialogue_message, text, 4096);
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_choices(const std::vector<std::string>& choices) {
	write_u32(stage_packet(dialogue_choices_begin_packet) + 8, static_cast<uint32>(choices.size()));
	for (std::size_t index = 0; index < choices.size(); ++index) {
		stage_text(Text_kind::dialogue_choice, choices[index], 1024, static_cast<uint32>(index));
	}
	stage_packet(dialogue_choices_show_packet);
	deliver(everyone);
}

void Friend_mode_session::send_dialogue_clear_choices() {
	stage_packet(dialogue_clear_choices_packet);
	deliver(everyone);
}

bool Friend_mode_session::take_dialogue_event(Dialogue_event& event) {
	if (dialogue_events.empty()) {
		return false;
	}
	event = std::move(dialogue_events.front());
	dialogue_events.pop_front();
	return true;
}

bool Friend_mode_session::take_peer_joined(int* player) {
	if (joined_players.empty()) {
		return false;
	}
	if (player) {
		*player = joined_players.front();
	}
	joined_players.pop_front();
	return true;
}

bool Friend_mode_session::take_peer_left(int& player, std::string& name) {
	if (left_players.empty()) {
		return false;
	}
	player = left_players.front().player;
	name   = std::move(left_players.front().name);
	left_players.pop_front();
	return true;
}

bool Friend_mode_session::take_connection_lost() {
	const bool lost = connection_lost;
	connection_lost = false;
	return lost;
}

// A whole text message has arrived.
void Friend_mode_session::handle_text(Peer& peer) {
	const bool host = session_mode == Mode::host;
	switch (peer.text_kind) {
	case Text_kind::identity:
		peer.name   = printable(peer.text);
		peer.joined = true;
		joined_players.push_back(peer.player);
		std::cout << "Friend Mode: connected player " << peer.name << std::endl;
		break;
	case Text_kind::chat: {
		std::string text = printable(peer.text);
		if (!text.empty()) {
			// The host knows who sent it; guests trust the host.
			chat_events.push_back({peer.player, peer.text_a, host ? peer.player : static_cast<int>(peer.text_b),
					std::move(text)});
		}
		break;
	}
	case Text_kind::whisper: {
		std::string text = printable(peer.text);
		if (text.empty()) {
			break;
		}
		if (host) {    // For another player: the host passes it on.
			chat_events.push_back({peer.player, chat_spectator, peer.player, std::move(text),
					static_cast<int>(peer.text_a)});
		} else {
			chat_events.push_back(
					{peer.player, peer.text_a, static_cast<int>(peer.text_b), std::move(text), local_player_id});
		}
		break;
	}
	case Text_kind::suggestion: {
		std::string choice = printable(peer.text);
		if (!choice.empty()) {
			// The host knows who sent it; guests trust the host.
			suggestions.emplace_back(host ? peer.player : static_cast<int>(peer.text_b), std::move(choice));
		}
		break;
	}
	case Text_kind::speech:
		if (!host) {
			speech_events.push_back({peer.text_a, std::move(peer.text)});
		}
		break;
	case Text_kind::dialogue_message:
		if (!host) {
			dialogue_events.push_back({Dialogue_event_type::message, 0, 0, 0, std::move(peer.text)});
		}
		break;
	case Text_kind::dialogue_choice:
		if (!host && peer.text_a < pending_dialogue_choices.size()) {
			pending_dialogue_choices[peer.text_a] = std::move(peer.text);
		}
		break;
	}
	peer.text.clear();
	peer.text_pending = false;
}

void Friend_mode_session::handle_packet(Peer& peer) {
	const unsigned char* buffer = peer.receive_buffer.data();
	const bool valid_header = buffer[0] == 'U' && buffer[1] == '7' && buffer[2] == 'F' && buffer[3] == 'M';
	const unsigned short version = static_cast<unsigned short>((buffer[4] << 8) | buffer[5]);
	const unsigned char  type    = buffer[6];
	const uint32         game    = read_u32(buffer + 8);
	const uint32         second  = read_u32(buffer + 12);
	const uint32         third   = read_u32(buffer + 16);
	peer.received                = 0;
	const bool host  = session_mode == Mode::host;
	const bool guest = session_mode == Mode::guest;
	const bool valid = valid_header && version == protocol_version;
	const bool ready = valid && peer.handshake_complete;

	if (host && type == hello_packet && valid_header) {
		const bool matches = version == protocol_version && game == local_game_code;
		const auto guests  = static_cast<int>(guest_players().size());
		if (matches && guests < guest_limit) {
			peer.handshake_complete = true;
			peer.player             = next_player++;
			unsigned char* accepted = stage_packet(accepted_packet);
			write_u32(accepted + 12, static_cast<uint32>(peer.player));
			peer.outgoing.insert(peer.outgoing.end(), staged.begin(), staged.end());
			staged.clear();
			send_identity(peer);
			std::cout << "Friend Mode: guest accepted as player " << peer.player << std::endl;
		} else {
			// The packet carries our protocol version: an older guest reads
			// only the type.
			write_u32(stage_packet(rejected_packet) + 12, matches ? reject_full : reject_mismatch);
			peer.outgoing.insert(peer.outgoing.end(), staged.begin(), staged.end());
			staged.clear();
			flush(peer);
			std::cerr << "Friend Mode: guest rejected (" << (matches ? "game full" : "protocol or game mismatch")
					  << ')' << std::endl;
			close_peer(peer);
		}
	} else if (guest && valid && type == accepted_packet && game == local_game_code) {
		peer.handshake_complete = true;
		local_player_id         = static_cast<int>(second);
		send_identity(peer);
		std::cout << "Friend Mode: session accepted by host as player " << local_player_id << std::endl;
	} else if (guest && valid_header && type == rejected_packet) {
		full_rejection = version == protocol_version && second == reject_full;
		std::cerr << "Friend Mode: the host refused us" << (full_rejection ? " (game full)" : "") << std::endl;
		close_peer(peer);
	} else if (ready && type == blob_packet) {
		const uint32 length = game;
		if (length > max_blob_size) {
			std::cerr << "Friend Mode: refused " << length << "-byte blob" << std::endl;
			close_peer(peer);
			return;
		}
		peer.blob_kind      = second;
		peer.blob_remaining = length;
		peer.blob_data.clear();
		peer.blob_data.reserve(length);
		if (length == 0) {
			blobs.push_back({peer.player, static_cast<Blob_kind>(peer.blob_kind), {}});
		}
	} else if (ready && type == text_begin_packet) {
		const auto kind = static_cast<Text_kind>(buffer[7]);
		const uint32 limit = kind == Text_kind::chat || kind == Text_kind::whisper
										 || kind == Text_kind::suggestion
									 ? 80
							 : kind == Text_kind::identity         ? 20
							 : kind == Text_kind::dialogue_message ? 4096
																   : 1024;
		peer.text_kind    = kind;
		peer.text_length  = std::min(game, limit);
		peer.text_a       = second;
		peer.text_b       = third;
		peer.text_pending = true;
		peer.text.clear();
		peer.text.reserve(peer.text_length);
		if (host && kind != Text_kind::chat && kind != Text_kind::whisper && kind != Text_kind::identity
			&& kind != Text_kind::suggestion) {
			peer.text_pending = false;    // Guests send only chat, whispers, suggestions and their name.
		} else if (peer.text_length == 0) {
			handle_text(peer);
		}
	} else if (ready && type == text_chunk_packet) {
		if (peer.text_pending) {
			const std::size_t remaining = peer.text_length - peer.text.size();
			peer.text.append(reinterpret_cast<const char*>(buffer + 8), std::min(text_chunk_size, remaining));
			if (peer.text.size() == peer.text_length) {
				handle_text(peer);
			}
		}
	} else if (host && ready && type == world_request_packet) {
		peer.world_requested = true;
	} else if (guest && ready && type == object_frame_packet) {
		Object_frame_event event;
		unpack_position(game, event.tx, event.ty, event.tz);
		event.shape     = static_cast<int>(second & 0xffffU);
		event.new_shape = static_cast<int>(second >> 16);
		event.old_frame = static_cast<int>(third & 0xffU);
		event.new_frame = static_cast<int>((third >> 8) & 0xffU);
		object_frames.push_back(event);
	} else if (host && ready && type == door_use_packet) {
		Object_frame_event event;
		unpack_position(game, event.tx, event.ty, event.tz);
		event.shape     = static_cast<int>(second & 0xffffU);
		event.old_frame = static_cast<int>(third & 0xffU);
		event.new_shape = event.shape;
		event.new_frame = event.old_frame;
		if (door_uses.size() < 32) {
			door_uses.push_back({peer.player, event});
		}
	} else if (guest && ready && type == host_paused_packet) {
		host_paused         = game != 0;
		host_paused_changed = true;
	} else if (guest && ready && type == party_begin_packet) {
		pending_party_members.clear();
	} else if (guest && ready && type == party_member_packet) {
		if (pending_party_members.size() < 16) {
			pending_party_members.push_back(game);
		}
	} else if (guest && ready && type == party_end_packet) {
		party_members       = std::move(pending_party_members);
		party_members_ready = true;
		pending_party_members.clear();
	} else if (guest && ready && type == roster_begin_packet) {
		roster.clear();
		received_roster = false;
	} else if (guest && ready && type == roster_actor_packet) {
		roster.push_back(game);
	} else if (guest && ready && type == roster_end_packet) {
		received_roster = true;
		roster_updated  = true;
	} else if (host && ready && type == actor_request_packet) {
		peer.requested_actor       = game;
		peer.actor_request_pending = true;
	} else if (guest && ready && type == actor_accepted_packet) {
		assigned_actor           = game;
		actor_assignment_pending = true;
		std::cout << "Friend Mode: companion assignment accepted" << std::endl;
	} else if (guest && ready && type == actor_rejected_packet) {
		actor_rejected = true;
		std::cerr << "Friend Mode: selected companion is no longer available" << std::endl;
	} else if (host && ready && type == move_request_packet) {
		if (peer.move_requests.size() < 64) {
			peer.move_requests.push_back({game, second, third});
		}
	} else if (guest && ready && type == actor_state_packet) {
		authoritative_position = game;
		authoritative_frame    = second;
		authoritative_seq      = third;
		actor_state_pending    = true;
	} else if (guest && ready && type == avatar_state_packet) {
		authoritative_avatar_position = game;
		authoritative_avatar_frame    = second;
		avatar_state_pending          = true;
	} else if (guest && ready && type == npc_state_packet) {
		npc_states.push_back({game, second, third});
	} else if (guest && ready && type == creature_state_packet) {
		creature_states.push_back({game, second, third});
	} else if (guest && ready && type == dialogue_begin_packet) {
		dialogue_events.push_back({Dialogue_event_type::begin});
	} else if (guest && ready && type == dialogue_end_packet) {
		dialogue_events.push_back({Dialogue_event_type::end});
	} else if (guest && ready && type == dialogue_face_packet) {
		dialogue_events.push_back({Dialogue_event_type::face, static_cast<int>(game), static_cast<int>(second),
				static_cast<int>(third)});
	} else if (guest && ready && type == dialogue_face_frame_packet) {
		dialogue_events.push_back({Dialogue_event_type::face_frame, static_cast<int>(game), static_cast<int>(second)});
	} else if (guest && ready && type == dialogue_remove_face_packet) {
		dialogue_events.push_back({Dialogue_event_type::remove_face, static_cast<int>(game)});
	} else if (guest && ready && type == dialogue_choices_begin_packet) {
		pending_dialogue_choices.assign(std::min<uint32>(game, 64), std::string());
	} else if (guest && ready && type == dialogue_choices_show_packet) {
		Dialogue_event event{Dialogue_event_type::choices};
		event.choices = std::move(pending_dialogue_choices);
		dialogue_events.push_back(std::move(event));
		pending_dialogue_choices.clear();
	} else if (guest && ready && type == dialogue_clear_choices_packet) {
		dialogue_events.push_back({Dialogue_event_type::clear_choices});
	} else if (!valid_header) {
		// Something other than Exult (e.g. a local port probe) connected.
		std::cerr << "Friend Mode: closed non-Friend Mode connection" << std::endl;
		close_peer(peer);
	} else {
		std::cerr << "Friend Mode: unexpected packet type " << static_cast<int>(type) << " (protocol " << version
				  << "); closing connection" << std::endl;
		close_peer(peer);
	}
}

void Friend_mode_session::flush(Peer& peer) {
	while (!peer.outgoing.empty() && peer.link) {
		const std::vector<unsigned char>& front = peer.outgoing.front();
		const int count = peer.link->write(front.data() + peer.sent, front.size() - peer.sent);
		if (count < 0) {
			std::cout << "Friend Mode: peer disconnected" << std::endl;
			close_peer(peer);
			return;
		}
		if (count == 0) {
			break;    // Full for now.
		}
		peer.sent += static_cast<std::size_t>(count);
		if (peer.sent == front.size()) {
			peer.outgoing.pop_front();
			peer.sent = 0;
		}
	}
}

void Friend_mode_session::receive(Peer& peer) {
	while (peer.link) {
		if (peer.blob_remaining > 0) {
			// Raw blob bytes follow a blob header, outside packet framing.
			unsigned char chunk[blob_chunk_size];
			const int     count
					= peer.link->read(chunk, std::min<std::size_t>(sizeof(chunk), peer.blob_remaining));
			if (count > 0) {
				peer.blob_data.insert(peer.blob_data.end(), chunk, chunk + count);
				peer.blob_remaining -= static_cast<uint32>(count);
				if (peer.blob_remaining == 0) {
					blobs.push_back({peer.player, static_cast<Blob_kind>(peer.blob_kind), std::move(peer.blob_data)});
					peer.blob_data.clear();
				}
				continue;
			}
			if (count < 0) {
				std::cout << "Friend Mode: peer disconnected" << std::endl;
				close_peer(peer);
			}
			break;
		}
		const int count = peer.link->read(peer.receive_buffer.data() + peer.received, packet_size - peer.received);
		if (count > 0) {
			peer.received += static_cast<std::size_t>(count);
			if (peer.received == packet_size) {
				handle_packet(peer);
			}
		} else {
			if (count < 0) {
				std::cout << "Friend Mode: peer disconnected" << std::endl;
				close_peer(peer);
			}
			break;
		}
	}
}

void Friend_mode_session::poll() {
	if (session_mode == Mode::host && listener) {
		while (auto link = listener->accept()) {
			// Its player number comes with the handshake: port probes that
			// never send a hello don't use one up.
			auto peer    = std::make_unique<Peer>();
			peer->player = -2;
			peer->link   = std::move(link);
			std::cout << "Friend Mode: guest connected; validating session" << std::endl;
			peers.push_back(std::move(peer));
		}
	}
	for (auto& peer : peers) {
		flush(*peer);
		receive(*peer);
	}
	// Forget closed connections, and anything they sent that is still unread.
	for (auto it = peers.begin(); it != peers.end();) {
		Peer& peer = **it;
		if (peer.link) {
			++it;
			continue;
		}
		const int player = peer.player;
		if (session_mode == Mode::host) {
			if (peer.joined) {
				left_players.push_back({player, peer.name});
			}
			auto from_player = [player](const auto& entry) {
				return entry.from == player;
			};
			blobs.erase(std::remove_if(blobs.begin(), blobs.end(), from_player), blobs.end());
			chat_events.erase(std::remove_if(chat_events.begin(), chat_events.end(), from_player), chat_events.end());
			door_uses.erase(std::remove_if(door_uses.begin(), door_uses.end(), from_player), door_uses.end());
			joined_players.erase(
					std::remove(joined_players.begin(), joined_players.end(), player), joined_players.end());
			it = peers.erase(it);
		} else {
			// A guest keeps its one Peer (closed) so it can tell the host
			// refused it from never having connected.
			connection_lost = connection_lost || peer.handshake_complete;
			peer.handshake_complete = false;
			++it;
		}
	}
}

void Friend_mode_session::close_peer(Peer& peer) {
	if (peer.link) {
		peer.link->close();
		peer.link.reset();
	}
	peer.outgoing.clear();
	peer.sent           = 0;
	peer.received       = 0;
	peer.blob_remaining = 0;
	peer.blob_data.clear();
	peer.text_pending = false;
}

void Friend_mode_session::stop() {
	for (auto& peer : peers) {
		close_peer(*peer);
	}
	peers.clear();
	listener.reset();
	staged.clear();
	suggestions.clear();
	next_player     = 1;
	local_player_id = 0;
	roster.clear();
	received_roster          = false;
	roster_updated           = false;
	actor_assignment_pending = false;
	actor_rejected           = false;
	actor_state_pending      = false;
	avatar_state_pending     = false;
	npc_states.clear();
	creature_states.clear();
	speech_events.clear();
	chat_events.clear();
	dialogue_events.clear();
	pending_dialogue_choices.clear();
	joined_players.clear();
	left_players.clear();
	party_members.clear();
	pending_party_members.clear();
	party_members_ready = false;
	blobs.clear();
	object_frames.clear();
	door_uses.clear();
	host_paused         = false;
	host_paused_changed = false;
	session_mode        = Mode::off;
}
