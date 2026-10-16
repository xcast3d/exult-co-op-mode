#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendnet.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#else
#	include <csignal>
#	include <fcntl.h>
#	include <netdb.h>
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

constexpr unsigned short protocol_version = 13;
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
constexpr unsigned char  speech_begin_packet = 15;
constexpr unsigned char  speech_chunk_packet = 16;
constexpr unsigned char  dialogue_begin_packet = 17;
constexpr unsigned char  dialogue_end_packet = 18;
constexpr unsigned char  dialogue_face_packet = 19;
constexpr unsigned char  dialogue_face_frame_packet = 20;
constexpr unsigned char  dialogue_remove_face_packet = 21;
constexpr unsigned char  dialogue_message_begin_packet = 22;
constexpr unsigned char  dialogue_text_chunk_packet = 23;
constexpr unsigned char  dialogue_choices_begin_packet = 24;
constexpr unsigned char  dialogue_choice_begin_packet = 25;
constexpr unsigned char  dialogue_choices_show_packet = 26;
constexpr unsigned char  dialogue_clear_choices_packet = 27;
constexpr unsigned char  identity_begin_packet = 28;
constexpr unsigned char  identity_chunk_packet = 29;
constexpr unsigned char  chat_begin_packet = 30;
constexpr unsigned char  chat_chunk_packet = 31;
constexpr unsigned char  party_begin_packet = 32;
constexpr unsigned char  party_member_packet = 33;
constexpr unsigned char  party_end_packet = 34;
// A blob header is followed by 'length' raw bytes outside packet framing.
constexpr unsigned char  blob_packet = 35;
constexpr unsigned char  world_request_packet = 36;
constexpr unsigned char  object_frame_packet = 37;
constexpr unsigned char  door_use_packet = 38;
constexpr unsigned char  host_paused_packet = 39;
constexpr uint32         max_blob_size = 16 * 1024 * 1024;
constexpr std::size_t    blob_chunk_size = 16 * 1024;

// Keep only printable ASCII from peer-supplied text.
void append_printable(std::string& out, const unsigned char* data, std::size_t count) {
	for (std::size_t i = 0; i < count; ++i) {
		if (data[i] >= 32 && data[i] < 127) {
			out.push_back(static_cast<char>(data[i]));
		}
	}
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

Friend_mode_session::~Friend_mode_session() {
	stop();
}

bool Friend_mode_session::start_host(int port, uint32 game_code) {
	ignore_broken_pipe();
	stop();
	connection_lost = false;
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
	ignore_broken_pipe();
	stop();
	connection_lost = false;
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
	std::vector<unsigned char> packet = {'U', '7', 'F', 'M', static_cast<unsigned char>(protocol_version >> 8),
			static_cast<unsigned char>(protocol_version), type, 0};
	packet.resize(packet_size, 0);
	write_u32(packet.data() + 8, local_game_code);
	outgoing.push_back(std::move(packet));
}

void Friend_mode_session::send_blob(Blob_kind kind, const std::vector<unsigned char>& data) {
	if (data.size() > max_blob_size) {
		std::cerr << "Friend Mode: blob too large (" << data.size() << " bytes)" << std::endl;
		return;
	}
	queue_packet(blob_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(data.size()));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(kind));
	for (std::size_t offset = 0; offset < data.size(); offset += blob_chunk_size) {
		const std::size_t count = std::min(blob_chunk_size, data.size() - offset);
		outgoing.emplace_back(data.begin() + offset, data.begin() + offset + count);
	}
}

bool Friend_mode_session::take_blob(Blob_kind& kind, std::vector<unsigned char>& data) {
	if (blobs.empty()) {
		return false;
	}
	kind = blobs.front().kind;
	data = std::move(blobs.front().data);
	blobs.pop_front();
	return true;
}

void Friend_mode_session::send_object_change(
		int tx, int ty, int tz, int old_shape, int new_shape, int old_frame, int new_frame) {
	queue_packet(object_frame_packet);
	write_u32(outgoing.back().data() + 8, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 12,
			  static_cast<uint32>((old_shape & 0xffff) | ((new_shape & 0xffff) << 16)));
	write_u32(outgoing.back().data() + 16, static_cast<uint32>((old_frame & 0xff) | ((new_frame & 0xff) << 8)));
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
	queue_packet(door_use_packet);
	write_u32(outgoing.back().data() + 8, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(shape));
	write_u32(outgoing.back().data() + 16, static_cast<uint32>(frame));
}

bool Friend_mode_session::take_door_use(Object_frame_event& event) {
	if (door_uses.empty()) {
		return false;
	}
	event = door_uses.front();
	door_uses.pop_front();
	return true;
}

void Friend_mode_session::send_host_paused(bool paused) {
	queue_packet(host_paused_packet);
	write_u32(outgoing.back().data() + 8, paused ? 1U : 0U);
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
	queue_packet(world_request_packet);
}

bool Friend_mode_session::take_world_request() {
	const bool requested = world_requested;
	world_requested      = false;
	return requested;
}

void Friend_mode_session::send_identity() {
	const std::size_t length = std::min<std::size_t>(local_player_name.size(), 20);
	queue_packet(identity_begin_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(length));
	for (std::size_t offset = 0; offset < length; offset += 12) {
		queue_packet(identity_chunk_packet);
		const std::size_t count = std::min<std::size_t>(12, length - offset);
		std::memcpy(outgoing.back().data() + 8, local_player_name.data() + offset, count);
	}
}

void Friend_mode_session::send_party_members(const std::vector<uint32>& members) {
	queue_packet(party_begin_packet);
	for (uint32 member : members) {
		queue_packet(party_member_packet);
		write_u32(outgoing.back().data() + 8, member);
	}
	queue_packet(party_end_packet);
}

bool Friend_mode_session::take_party_members(std::vector<uint32>& members) {
	if (!party_members_ready) {
		return false;
	}
	members             = party_members;
	party_members_ready = false;
	return true;
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

void Friend_mode_session::send_move_request(int tx, int ty, int tz, int frame) {
	queue_packet(move_request_packet);
	write_u32(outgoing.back().data() + 8, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(frame));
}

bool Friend_mode_session::take_move_request(int& tx, int& ty, int& tz, int& frame) {
	if (!move_request_pending) {
		return false;
	}
	move_request_pending = false;
	unpack_position(requested_position, tx, ty, tz);
	frame = static_cast<int>(requested_frame);
	return true;
}

void Friend_mode_session::send_actor_state(int tx, int ty, int tz, int frame) {
	queue_packet(actor_state_packet);
	write_u32(outgoing.back().data() + 8, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(frame));
}

bool Friend_mode_session::take_actor_state(int& tx, int& ty, int& tz, int& frame) {
	if (!actor_state_pending) {
		return false;
	}
	actor_state_pending = false;
	unpack_position(authoritative_position, tx, ty, tz);
	frame = static_cast<int>(authoritative_frame);
	return true;
}

void Friend_mode_session::send_avatar_state(int tx, int ty, int tz, int frame) {
	queue_packet(avatar_state_packet);
	write_u32(outgoing.back().data() + 8, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(frame));
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
	queue_packet(npc_state_packet);
	write_u32(outgoing.back().data() + 8, npc);
	write_u32(outgoing.back().data() + 12, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 16, static_cast<uint32>(frame));
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
	queue_packet(creature_state_packet);
	write_u32(outgoing.back().data() + 8, creature);
	write_u32(outgoing.back().data() + 12, pack_position(tx, ty, tz));
	write_u32(outgoing.back().data() + 16, static_cast<uint32>(frame));
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
	const std::size_t length = std::min<std::size_t>(text.size(), 1024);
	queue_packet(speech_begin_packet);
	write_u32(outgoing.back().data() + 8, source | (creature ? 0x80000000U : 0U));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(length));
	for (std::size_t offset = 0; offset < length; offset += 12) {
		queue_packet(speech_chunk_packet);
		const std::size_t count = std::min<std::size_t>(12, length - offset);
		std::memcpy(outgoing.back().data() + 8, text.data() + offset, count);
	}
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

void Friend_mode_session::send_chat(uint32 speaker, const std::string& text) {
	const std::size_t length = std::min<std::size_t>(text.size(), 80);
	queue_packet(chat_begin_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(length));
	write_u32(outgoing.back().data() + 12, speaker);
	for (std::size_t offset = 0; offset < length; offset += 12) {
		queue_packet(chat_chunk_packet);
		const std::size_t count = std::min<std::size_t>(12, length - offset);
		std::memcpy(outgoing.back().data() + 8, text.data() + offset, count);
	}
}

bool Friend_mode_session::take_chat(uint32& speaker, std::string& text) {
	if (chat_events.empty()) {
		return false;
	}
	speaker = chat_events.front().speaker;
	text    = std::move(chat_events.front().text);
	chat_events.pop_front();
	return true;
}

void Friend_mode_session::send_dialogue_begin() {
	queue_packet(dialogue_begin_packet);
}

void Friend_mode_session::send_dialogue_end() {
	queue_packet(dialogue_end_packet);
}

void Friend_mode_session::send_dialogue_face(int shape, int frame, int slot) {
	queue_packet(dialogue_face_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(shape));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(frame));
	write_u32(outgoing.back().data() + 16, static_cast<uint32>(slot));
}

void Friend_mode_session::send_dialogue_face_frame(int frame, int slot) {
	queue_packet(dialogue_face_frame_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(frame));
	write_u32(outgoing.back().data() + 12, static_cast<uint32>(slot));
}

void Friend_mode_session::send_dialogue_remove_face(int shape) {
	queue_packet(dialogue_remove_face_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(shape));
}

void Friend_mode_session::send_dialogue_message(const std::string& text) {
	const std::size_t length = std::min<std::size_t>(text.size(), 4096);
	queue_packet(dialogue_message_begin_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(length));
	for (std::size_t offset = 0; offset < length; offset += 12) {
		queue_packet(dialogue_text_chunk_packet);
		const std::size_t count = std::min<std::size_t>(12, length - offset);
		std::memcpy(outgoing.back().data() + 8, text.data() + offset, count);
	}
}

void Friend_mode_session::send_dialogue_choices(const std::vector<std::string>& choices) {
	queue_packet(dialogue_choices_begin_packet);
	write_u32(outgoing.back().data() + 8, static_cast<uint32>(choices.size()));
	for (std::size_t index = 0; index < choices.size(); ++index) {
		const std::size_t length = std::min<std::size_t>(choices[index].size(), 1024);
		queue_packet(dialogue_choice_begin_packet);
		write_u32(outgoing.back().data() + 8, static_cast<uint32>(index));
		write_u32(outgoing.back().data() + 12, static_cast<uint32>(length));
		for (std::size_t offset = 0; offset < length; offset += 12) {
			queue_packet(dialogue_text_chunk_packet);
			const std::size_t count = std::min<std::size_t>(12, length - offset);
			std::memcpy(outgoing.back().data() + 8, choices[index].data() + offset, count);
		}
	}
	queue_packet(dialogue_choices_show_packet);
}

void Friend_mode_session::send_dialogue_clear_choices() {
	queue_packet(dialogue_clear_choices_packet);
}

bool Friend_mode_session::take_dialogue_event(Dialogue_event& event) {
	if (dialogue_events.empty()) {
		return false;
	}
	event = std::move(dialogue_events.front());
	dialogue_events.pop_front();
	return true;
}

bool Friend_mode_session::take_peer_joined() {
	const bool joined = peer_joined;
	peer_joined       = false;
	return joined;
}

bool Friend_mode_session::take_connection_lost() {
	const bool lost = connection_lost;
	connection_lost = false;
	return lost;
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
			send_identity();
			std::cout << "Friend Mode: guest accepted" << std::endl;
		} else {
			queue_packet(rejected_packet);
			std::cerr << "Friend Mode: guest rejected (protocol or game mismatch)" << std::endl;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == accepted_packet
			   && game == local_game_code) {
		handshake_complete = true;
		send_identity();
		std::cout << "Friend Mode: session accepted by host" << std::endl;
	} else if (handshake_complete && valid_header && version == protocol_version && type == blob_packet) {
		const uint32 length = game;
		if (length > max_blob_size) {
			std::cerr << "Friend Mode: refused " << length << "-byte blob" << std::endl;
			close_peer();
			return;
		}
		blob_kind      = static_cast<Blob_kind>(read_u32(receive_buffer.data() + 12));
		blob_remaining = length;
		blob_data.clear();
		blob_data.reserve(length);
		if (length == 0) {
			blobs.push_back({blob_kind, {}});
		}
	} else if (session_mode == Mode::host && handshake_complete && valid_header && version == protocol_version
			   && type == world_request_packet) {
		world_requested = true;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == object_frame_packet) {
		Object_frame_event event;
		unpack_position(game, event.tx, event.ty, event.tz);
		const uint32 shapes   = read_u32(receive_buffer.data() + 12);
		event.shape           = static_cast<int>(shapes & 0xffffU);
		event.new_shape       = static_cast<int>(shapes >> 16);
		const uint32 frames   = read_u32(receive_buffer.data() + 16);
		event.old_frame       = static_cast<int>(frames & 0xffU);
		event.new_frame       = static_cast<int>((frames >> 8) & 0xffU);
		object_frames.push_back(event);
	} else if (session_mode == Mode::host && handshake_complete && valid_header && version == protocol_version
			   && type == door_use_packet) {
		Object_frame_event event;
		unpack_position(game, event.tx, event.ty, event.tz);
		event.shape     = static_cast<int>(read_u32(receive_buffer.data() + 12) & 0xffffU);
		event.old_frame = static_cast<int>(read_u32(receive_buffer.data() + 16) & 0xffU);
		event.new_shape = event.shape;
		event.new_frame = event.old_frame;
		if (door_uses.size() < 8) {
			door_uses.push_back(event);
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == host_paused_packet) {
		host_paused         = game != 0;
		host_paused_changed = true;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == party_begin_packet) {
		pending_party_members.clear();
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == party_member_packet) {
		if (pending_party_members.size() < 16) {
			pending_party_members.push_back(game);
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == party_end_packet) {
		party_members       = std::move(pending_party_members);
		party_members_ready = true;
		pending_party_members.clear();
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
	} else if (session_mode == Mode::host && valid_header && version == protocol_version && type == move_request_packet) {
		requested_position = game;
		requested_frame = read_u32(receive_buffer.data() + 12);
		move_request_pending = true;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == actor_state_packet) {
		authoritative_position = game;
		authoritative_frame = read_u32(receive_buffer.data() + 12);
		actor_state_pending = true;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == avatar_state_packet) {
		authoritative_avatar_position = game;
		authoritative_avatar_frame = read_u32(receive_buffer.data() + 12);
		avatar_state_pending = true;
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == npc_state_packet) {
		npc_states.push_back({game, read_u32(receive_buffer.data() + 12), read_u32(receive_buffer.data() + 16)});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == creature_state_packet) {
		creature_states.push_back({game, read_u32(receive_buffer.data() + 12), read_u32(receive_buffer.data() + 16)});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == speech_begin_packet) {
		pending_speech_source = game;
		pending_speech_length = std::min<uint32>(read_u32(receive_buffer.data() + 12), 1024);
		pending_speech_text.clear();
		pending_speech_text.reserve(pending_speech_length);
		speech_pending = true;
		if (pending_speech_length == 0) {
			speech_events.push_back({pending_speech_source, std::string()});
			speech_pending = false;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == speech_chunk_packet
			   && speech_pending) {
		const std::size_t remaining = pending_speech_length - pending_speech_text.size();
		pending_speech_text.append(
				reinterpret_cast<const char*>(receive_buffer.data() + 8), std::min<std::size_t>(12, remaining));
		if (pending_speech_text.size() == pending_speech_length) {
			speech_events.push_back({pending_speech_source, std::move(pending_speech_text)});
			pending_speech_text.clear();
			speech_pending = false;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == dialogue_begin_packet) {
		dialogue_events.push_back({Dialogue_event_type::begin});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == dialogue_end_packet) {
		dialogue_events.push_back({Dialogue_event_type::end});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == dialogue_face_packet) {
		dialogue_events.push_back({Dialogue_event_type::face, static_cast<int>(game),
				static_cast<int>(read_u32(receive_buffer.data() + 12)),
				static_cast<int>(read_u32(receive_buffer.data() + 16))});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == dialogue_face_frame_packet) {
		dialogue_events.push_back({Dialogue_event_type::face_frame, static_cast<int>(game),
				static_cast<int>(read_u32(receive_buffer.data() + 12))});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version && type == dialogue_remove_face_packet) {
		dialogue_events.push_back({Dialogue_event_type::remove_face, static_cast<int>(game)});
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == dialogue_message_begin_packet) {
		pending_dialogue_text_type = Dialogue_event_type::message;
		pending_dialogue_text_index = 0;
		pending_dialogue_text_length = std::min<uint32>(game, 4096);
		pending_dialogue_text.clear();
		pending_dialogue_text.reserve(pending_dialogue_text_length);
		dialogue_text_pending = true;
		if (pending_dialogue_text_length == 0) {
			dialogue_events.push_back({Dialogue_event_type::message, 0, 0, 0, std::string()});
			dialogue_text_pending = false;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == dialogue_choices_begin_packet) {
		pending_dialogue_choices.assign(std::min<uint32>(game, 64), std::string());
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == dialogue_choice_begin_packet) {
		pending_dialogue_text_type = Dialogue_event_type::choices;
		pending_dialogue_text_index = game;
		pending_dialogue_text_length = std::min<uint32>(read_u32(receive_buffer.data() + 12), 1024);
		pending_dialogue_text.clear();
		pending_dialogue_text.reserve(pending_dialogue_text_length);
		dialogue_text_pending = true;
		if (pending_dialogue_text_length == 0) {
			if (pending_dialogue_text_index < pending_dialogue_choices.size()) {
				pending_dialogue_choices[pending_dialogue_text_index].clear();
			}
			dialogue_text_pending = false;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == dialogue_text_chunk_packet && dialogue_text_pending) {
		const std::size_t remaining = pending_dialogue_text_length - pending_dialogue_text.size();
		pending_dialogue_text.append(
				reinterpret_cast<const char*>(receive_buffer.data() + 8), std::min<std::size_t>(12, remaining));
		if (pending_dialogue_text.size() == pending_dialogue_text_length) {
			if (pending_dialogue_text_type == Dialogue_event_type::message) {
				dialogue_events.push_back({Dialogue_event_type::message, 0, 0, 0, std::move(pending_dialogue_text)});
			} else if (pending_dialogue_text_index < pending_dialogue_choices.size()) {
				pending_dialogue_choices[pending_dialogue_text_index] = std::move(pending_dialogue_text);
			}
			pending_dialogue_text.clear();
			dialogue_text_pending = false;
		}
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == dialogue_choices_show_packet) {
		Dialogue_event event{Dialogue_event_type::choices};
		event.choices = std::move(pending_dialogue_choices);
		dialogue_events.push_back(std::move(event));
		pending_dialogue_choices.clear();
	} else if (session_mode == Mode::guest && valid_header && version == protocol_version
			   && type == dialogue_clear_choices_packet) {
		dialogue_events.push_back({Dialogue_event_type::clear_choices});
	} else if (valid_header && version == protocol_version && type == identity_begin_packet) {
		pending_identity_length = std::min<uint32>(game, 20);
		pending_identity.clear();
		pending_identity.reserve(pending_identity_length);
		pending_identity_received = 0;
		identity_pending          = true;
		if (pending_identity_length == 0) {
			remote_player_name.clear();
			identity_pending = false;
		}
	} else if (valid_header && version == protocol_version && type == identity_chunk_packet && identity_pending) {
		const std::size_t remaining = pending_identity_length - pending_identity_received;
		const std::size_t count     = std::min<std::size_t>(12, remaining);
		append_printable(pending_identity, receive_buffer.data() + 8, count);
		pending_identity_received += count;
		if (pending_identity_received == pending_identity_length) {
			remote_player_name = std::move(pending_identity);
			pending_identity.clear();
			identity_pending = false;
			std::cout << "Friend Mode: connected player " << remote_player_name << std::endl;
			peer_joined = true;
		}
	} else if (valid_header && version == protocol_version && type == chat_begin_packet) {
		pending_chat_length   = std::min<uint32>(game, 80);
		pending_chat_speaker  = read_u32(receive_buffer.data() + 12);
		pending_chat_received = 0;
		pending_chat_text.clear();
		pending_chat_text.reserve(pending_chat_length);
		chat_pending = true;
		if (pending_chat_length == 0) {
			chat_pending = false;
		}
	} else if (valid_header && version == protocol_version && type == chat_chunk_packet && chat_pending) {
		const std::size_t remaining = pending_chat_length - pending_chat_received;
		const std::size_t count     = std::min<std::size_t>(12, remaining);
		append_printable(pending_chat_text, receive_buffer.data() + 8, count);
		pending_chat_received += count;
		if (pending_chat_received == pending_chat_length) {
			if (!pending_chat_text.empty()) {
				chat_events.push_back({pending_chat_speaker, std::move(pending_chat_text)});
			}
			pending_chat_text.clear();
			chat_pending = false;
		}
	} else if (!valid_header) {
		// Something other than Exult (e.g. a local port probe) connected.
		std::cerr << "Friend Mode: closed non-Friend Mode connection" << std::endl;
		close_peer();
	} else {
		std::cerr << "Friend Mode: unexpected packet type " << static_cast<int>(type) << " (protocol " << version
				  << "); closing connection" << std::endl;
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
	while (!outgoing.empty() && peer_socket >= 0) {
		const std::size_t size  = outgoing.front().size();
		const int count = static_cast<int>(send(peer_socket, reinterpret_cast<const char*>(outgoing.front().data() + sent),
				size - sent, 0));
		if (count > 0) {
			sent += static_cast<std::size_t>(count);
			if (sent == size) {
				outgoing.pop_front();
				sent = 0;
			}
		} else if (count < 0) {
			if (!would_block()) {
				close_peer();
				return;
			}
			break;
		}
	}
	while (peer_socket >= 0) {
		if (blob_remaining > 0) {
			// Raw blob bytes follow a blob header, outside packet framing.
			unsigned char chunk[blob_chunk_size];
			const int     count = static_cast<int>(recv(peer_socket, reinterpret_cast<char*>(chunk),
                    std::min<std::size_t>(sizeof(chunk), blob_remaining), 0));
			if (count > 0) {
				blob_data.insert(blob_data.end(), chunk, chunk + count);
				blob_remaining -= static_cast<uint32>(count);
				if (blob_remaining == 0) {
					blobs.push_back({blob_kind, std::move(blob_data)});
					blob_data.clear();
				}
				continue;
			}
			if (count == 0 || !would_block()) {
				std::cout << "Friend Mode: peer disconnected" << std::endl;
				connection_lost = handshake_complete;
				close_peer();
			}
			break;
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
			connection_lost = handshake_complete;
			close_peer();
			break;
		} else {
			break;
		}
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
	move_request_pending = false;
	actor_state_pending = false;
	avatar_state_pending = false;
	npc_states.clear();
	creature_states.clear();
	speech_events.clear();
	speech_pending = false;
	pending_speech_text.clear();
	chat_events.clear();
	chat_pending = false;
	pending_chat_text.clear();
	pending_chat_length = 0;
	dialogue_events.clear();
	dialogue_text_pending = false;
	pending_dialogue_text.clear();
	pending_dialogue_choices.clear();
	remote_player_name.clear();
	peer_joined = false;
	party_members.clear();
	pending_party_members.clear();
	party_members_ready = false;
	object_frames.clear();
	door_uses.clear();
	host_paused         = false;
	host_paused_changed = false;
	blob_remaining  = 0;
	blob_data.clear();
	blobs.clear();
	world_requested = false;
	pending_identity.clear();
	pending_identity_length = 0;
	identity_pending = false;
}

void Friend_mode_session::stop() {
	close_peer();
	close_socket(listen_socket);
	listen_socket = -1;
	session_mode  = Mode::off;
}
