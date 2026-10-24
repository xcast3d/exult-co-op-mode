/*
 * Friend Mode over WebRTC: lobby codes, the data channel behind
 * Friend_connection.
 *
 * The host asks the lobby (tools/lobby, a Cloudflare Worker) for a short
 * code; a guest gives the lobby that code.  The lobby passes each side's
 * offer, answer and candidates to the other, then the two games talk over a
 * reliable, ordered data channel of their own: the lobby never carries the
 * game.  See docs/friend-mode/lobby.md.
 *
 * libdatachannel calls back on its own threads; everything they touch sits
 * behind a mutex, and the game only polls.  Built when configure finds the
 * libraries tools/friend-mode/build-rtc.sh makes (FRIEND_RTC); otherwise
 * lobby codes report that this build has none, and TCP still works.
 */
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendconn.h"

#include <cctype>

namespace {
// Lobby codes are a word and a number ("MOONGLOW-42", "moonglow 42");
// addresses have dots or colons, or no number after a word.
bool looks_like_code(const std::string& text) {
	std::size_t i = 0;
	while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i]))) {
		++i;
	}
	if (i < 2) {
		return false;
	}
	if (i < text.size() && (text[i] == '-' || text[i] == ' ')) {
		++i;
	}
	const std::size_t digits = i;
	while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
		++i;
	}
	return i == text.size() && i > digits && i - digits <= 4;
}
}    // namespace

bool Friend_is_lobby_code(const std::string& text) {
	std::string trimmed = text;
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
		trimmed.pop_back();
	}
	const std::size_t start = trimmed.find_first_not_of(" \t");
	return start != std::string::npos && looks_like_code(trimmed.substr(start));
}

#ifndef FRIEND_RTC

void Friend_ask_public_ip(const std::string& /*lobby_url*/) {}

std::string Friend_public_ip() {
	return {};
}

bool Friend_public_ip_done() {
	return true;
}

std::unique_ptr<Friend_lobby_host> Friend_rtc_host(const std::string& /*lobby_url*/, std::string& problem) {
	problem = "This build has no lobby codes";
	return nullptr;
}

std::unique_ptr<Friend_connection> Friend_rtc_connect(
		const std::string& /*lobby_url*/, const std::string& /*code*/, int /*timeout_ms*/, std::string& problem) {
	problem = "This build has no lobby codes; join by address";
	return nullptr;
}

#else

#	include <rtc/rtc.hpp>

#	include <nlohmann/json.hpp>

#	include <algorithm>
#	include <chrono>
#	include <cstdio>
#	include <cstdlib>
#	include <deque>
#	include <iostream>
#	include <map>
#	include <mutex>
#	include <thread>
#	include <variant>
#	include <vector>

#	include <mbedtls/ctr_drbg.h>
#	include <mbedtls/entropy.h>
#	include <mbedtls/ssl.h>
#	include <psa/crypto.h>

#	include "utils.h"

#	ifdef _WIN32
#		include <winsock2.h>
#		include <ws2tcpip.h>
#	else
#		include <netdb.h>
#		include <sys/socket.h>
#		include <sys/time.h>
#		include <unistd.h>
#	endif

namespace {
using json  = nlohmann::json;
using Clock = std::chrono::steady_clock;

// Each write goes out as messages of at most this size, so a big blob (the
// world download) travels in pieces any WebRTC peer accepts.
constexpr std::size_t piece_size = 16 * 1024;
// Writes wait (return 0) while this much is still queued in the channel.
constexpr std::size_t buffer_limit = 1024 * 1024;
// Received but not yet read: a peer sending more than this is cut off
// rather than let it fill memory (the world download is about 200 KB).
constexpr std::size_t incoming_limit = 64 * 1024 * 1024;
// The lobby's messages are a few kilobytes (it refuses over 16 KB itself).
constexpr int lobby_message_limit = 64 * 1024;

const char* const fallback_stun = "stun:stun.cloudflare.com:3478";

void start_logging() {
	static bool started = false;
	if (!started) {
		rtc::InitLogger(rtc::LogLevel::Warning);
		started = true;
	}
}

// "wss://lobby/" -> "wss://lobby"; a bare host name gets wss://.  Only
// wss:// (encrypted, and the lobby's certificate checked), except plain
// ws:// to a lobby on this machine (wrangler dev); empty when refused.
std::string lobby_base(std::string url, std::string& problem) {
	while (!url.empty() && (url.back() == '/' || std::isspace(static_cast<unsigned char>(url.back())))) {
		url.pop_back();
	}
	if (url.find("://") == std::string::npos) {
		url = "wss://" + url;
	}
	if (url.rfind("wss://", 0) == 0) {
		return url;
	}
	for (const char* local : {"ws://127.0.0.1", "ws://localhost", "ws://[::1]"}) {
		const std::string prefix = local;
		if (url.rfind(prefix, 0) == 0 && (url.size() == prefix.size() || url[prefix.size()] == ':')) {
			return url;
		}
	}
	problem = "The lobby address must start with wss://";
	return {};
}

// The lobby's 'ice' list ([{urls, username?, credential?}]) as WebRTC's
// servers; Cloudflare's STUN when the list has none.
rtc::Configuration config_from(const json& ice) {
	rtc::Configuration config;
	bool               stun = false;
	if (ice.is_array()) {
		for (const auto& server : ice) {
			if (!server.is_object() || !server.contains("urls")) {
				continue;
			}
			std::vector<std::string> urls;
			if (server["urls"].is_string()) {
				urls.push_back(server["urls"].get<std::string>());
			} else if (server["urls"].is_array()) {
				for (const auto& url : server["urls"]) {
					if (url.is_string()) {
						urls.push_back(url.get<std::string>());
					}
				}
			}
			for (const auto& url : urls) {
				try {
					if (url.rfind("turn", 0) == 0 && server.contains("username")) {
						rtc::IceServer turn(url);
						turn.username = server.value("username", "");
						turn.password = server.value("credential", "");
						config.iceServers.push_back(turn);
					} else if (url.rfind("stun", 0) == 0) {
						config.iceServers.emplace_back(url);
						stun = true;
					}
				} catch (const std::exception& error) {
					std::cerr << "Friend Mode: skipped ICE server " << url << " (" << error.what() << ")" << std::endl;
				}
			}
		}
	}
	if (!stun) {
		config.iceServers.emplace_back(fallback_stun);
	}
	return config;
}

// The certificate authorities to trust: RTC_CA_FILE, the list shipped with
// the game (<DATA>/friend/cacert.pem, Windows), or the system's list.  Not
// the Unix paths on Windows: there "/etc/ssl" is C:\etc\ssl, which any user
// can create.
std::string ca_certificate_file() {
	std::vector<std::string> files;
	if (const char* file = std::getenv("RTC_CA_FILE")) {
		files.emplace_back(file);
	}
	files.emplace_back(get_system_path("<DATA>/friend/cacert.pem"));
#	ifndef _WIN32
	files.emplace_back("/etc/ssl/cert.pem");
	files.emplace_back("/etc/ssl/certs/ca-certificates.crt");
#	endif
	for (const auto& path : files) {
		if (FILE* file = std::fopen(path.c_str(), "r")) {
			std::fclose(file);
			return path;
		}
	}
	return {};
}

// Mbed TLS has no trusted certificate authorities of its own, so wss://
// needs a list: RTC_CA_FILE, the system's (macOS, most Linux), or on
// Windows the one shipped with the game.  Without one the lobby's
// certificate can't be checked, and the connection is refused (never made
// unchecked).
std::shared_ptr<rtc::WebSocket> lobby_socket() {
	rtc::WebSocketConfiguration config;
	config.disableTlsVerification = false;
	config.maxMessageSize         = lobby_message_limit;
	config.connectionTimeout      = std::chrono::seconds(10);
	// No WebSocket pings: with Mbed TLS, libdatachannel's ping (sent when
	// nothing arrived for 10 s) closes the connection ("TLS closed").  The
	// host sends its own keep-alive instead (Rtc_host::accept).
	config.pingInterval = std::chrono::milliseconds(0);
	const std::string ca_file = ca_certificate_file();
	if (!ca_file.empty()) {
		config.caCertificatePemFile = ca_file;
	}
	return std::make_shared<rtc::WebSocket>(config);
}

void send_json(const std::shared_ptr<rtc::WebSocket>& ws, const json& message) {
	try {
		if (ws && ws->isOpen()) {
			ws->send(message.dump());
		}
	} catch (const std::exception& error) {
		std::cerr << "Friend Mode: lobby send failed (" << error.what() << ")" << std::endl;
	}
}

// Offers, answers and candidates go to the lobby; 'to' adds the guest's id
// on the host.
void signal_through(const std::shared_ptr<rtc::PeerConnection>& pc, const std::weak_ptr<rtc::WebSocket>& lobby,
					const json& to) {
	pc->onLocalDescription([lobby, to](rtc::Description description) {
		json message    = to;
		message["type"] = "signal";
		message["data"] = {{"sdp", std::string(description)}, {"kind", description.typeString()}};
		send_json(lobby.lock(), message);
	});
	pc->onLocalCandidate([lobby, to](rtc::Candidate candidate) {
		json message    = to;
		message["type"] = "signal";
		message["data"] = {{"candidate", std::string(candidate)}, {"mid", candidate.mid()}};
		send_json(lobby.lock(), message);
	});
}

void take_signal(const std::shared_ptr<rtc::PeerConnection>& pc, const json& data) {
	try {
		if (data.contains("sdp") && data.contains("kind")) {
			pc->setRemoteDescription(
					rtc::Description(data["sdp"].get<std::string>(), data["kind"].get<std::string>()));
		} else if (data.contains("candidate") && data.contains("mid")) {
			pc->addRemoteCandidate(
					rtc::Candidate(data["candidate"].get<std::string>(), data["mid"].get<std::string>()));
		}
	} catch (const std::exception& error) {
		std::cerr << "Friend Mode: bad signal from the lobby (" << error.what() << ")" << std::endl;
	}
}

// Text from the lobby shown to the player ("No game with that code"):
// printable, and short.
std::string lobby_text(const json& message, const char* fallback) {
	std::string text = message.contains("message") && message["message"].is_string()
							   ? message["message"].get<std::string>()
							   : std::string(fallback);
	std::string out;
	for (const char ch : text) {
		if (ch >= 32 && ch < 127 && out.size() < 80) {
			out.push_back(ch);
		}
	}
	return out.empty() ? std::string(fallback) : out;
}

json parse_lobby(const std::variant<rtc::binary, rtc::string>& message) {
	if (!std::holds_alternative<rtc::string>(message)) {
		return json();
	}
	return json::parse(std::get<rtc::string>(message), nullptr, false);
}

// What a channel's callbacks share with the connection the game holds.
struct Channel_state {
	std::mutex                             mutex;
	std::deque<unsigned char>              incoming;
	bool                                   open   = false;
	bool                                   closed = false;
	std::string                            problem;
	std::shared_ptr<rtc::WebSocket>        lobby;    // Guest, until the channel opens.
	std::shared_ptr<rtc::PeerConnection>   pc;
	std::shared_ptr<rtc::DataChannel>      dc;

	void fail(const std::string& why) {
		const std::lock_guard<std::mutex> lock(mutex);
		if (!open && problem.empty()) {
			problem = why;
		}
		closed = true;
	}

	// The lobby went away: only a problem before the channel opened (the
	// guest closes it itself then).
	void lobby_lost(const std::string& why) {
		const std::lock_guard<std::mutex> lock(mutex);
		if (!open) {
			if (problem.empty()) {
				problem = why;
			}
			closed = true;
		}
	}
};

void watch_channel(const std::shared_ptr<rtc::DataChannel>& dc, const std::weak_ptr<Channel_state>& weak) {
	dc->onOpen([weak]() {
		if (auto state = weak.lock()) {
			std::shared_ptr<rtc::WebSocket> lobby;
			{
				const std::lock_guard<std::mutex> lock(state->mutex);
				state->open = true;
				lobby       = std::move(state->lobby);    // Done with the lobby.
			}
			if (lobby) {
				lobby->close();
			}
		}
	});
	dc->onClosed([weak]() {
		if (auto state = weak.lock()) {
			state->fail("The connection closed");
		}
	});
	dc->onError([weak](std::string error) {
		if (auto state = weak.lock()) {
			state->fail("Connection error: " + error);
		}
	});
	dc->onMessage([weak](std::variant<rtc::binary, rtc::string> message) {
		auto state = weak.lock();
		if (!state) {
			return;
		}
		const std::lock_guard<std::mutex> lock(state->mutex);
		const std::size_t size = std::holds_alternative<rtc::binary>(message)
										 ? std::get<rtc::binary>(message).size()
										 : std::get<rtc::string>(message).size();
		if (state->incoming.size() + size > incoming_limit) {
			std::cerr << "Friend Mode: peer sent too much; closing" << std::endl;
			state->incoming.clear();
			state->closed = true;
			return;
		}
		if (std::holds_alternative<rtc::binary>(message)) {
			const auto& bytes = std::get<rtc::binary>(message);
			for (const std::byte b : bytes) {
				state->incoming.push_back(static_cast<unsigned char>(b));
			}
		} else {
			const auto& text = std::get<rtc::string>(message);
			state->incoming.insert(state->incoming.end(), text.begin(), text.end());
		}
	});
}

class Rtc_connection : public Friend_connection {
public:
	Rtc_connection(std::shared_ptr<Channel_state> state, Clock::time_point deadline)
			: state(std::move(state)), deadline(deadline) {}

	~Rtc_connection() override {
		Rtc_connection::close();
	}

	int write(const unsigned char* data, std::size_t size) override {
		std::shared_ptr<rtc::DataChannel> dc;
		const int                         status = ready(dc);
		if (status <= 0) {
			return status;
		}
		try {
			if (dc->bufferedAmount() > buffer_limit) {
				return 0;    // Full for now.
			}
			const std::size_t count = std::min(size, std::min(piece_size, dc->maxMessageSize()));
			dc->send(reinterpret_cast<const std::byte*>(data), count);
			return static_cast<int>(count);
		} catch (const std::exception& error) {
			state->fail(std::string("Send failed: ") + error.what());
			return -1;
		}
	}

	int read(unsigned char* data, std::size_t size) override {
		const std::lock_guard<std::mutex> lock(state->mutex);
		if (!state->incoming.empty()) {
			const std::size_t count = std::min(size, state->incoming.size());
			std::copy_n(state->incoming.begin(), count, data);
			state->incoming.erase(state->incoming.begin(), state->incoming.begin() + count);
			return static_cast<int>(count);
		}
		if (state->closed) {
			return -1;
		}
		if (!state->open && Clock::now() > deadline) {
			state->problem = "Could not connect through the lobby (timed out)";
			state->closed  = true;
			return -1;
		}
		return 0;
	}

	void close() override {
		std::shared_ptr<rtc::WebSocket>      lobby;
		std::shared_ptr<rtc::PeerConnection> pc;
		std::shared_ptr<rtc::DataChannel>    dc;
		{
			const std::lock_guard<std::mutex> lock(state->mutex);
			state->closed = true;
			lobby         = std::move(state->lobby);
			pc            = std::move(state->pc);
			dc            = std::move(state->dc);
		}
		// Closed and let go here, on the game's thread, never in a callback.
		try {
			if (dc) {
				dc->close();
			}
			if (pc) {
				pc->close();
			}
			if (lobby) {
				lobby->close();
			}
		} catch (const std::exception& error) {
			std::cerr << "Friend Mode: closing the connection: " << error.what() << std::endl;
		}
	}

	std::string problem() const override {
		const std::lock_guard<std::mutex> lock(state->mutex);
		return state->problem;
	}

private:
	std::shared_ptr<Channel_state> state;
	Clock::time_point              deadline;

	// 1 with the channel once it is open, 0 while still connecting, -1
	// once closed (or given up).
	int ready(std::shared_ptr<rtc::DataChannel>& dc) {
		const std::lock_guard<std::mutex> lock(state->mutex);
		if (state->closed) {
			return -1;
		}
		if (!state->open && Clock::now() > deadline) {
			state->problem = "Could not connect through the lobby (timed out)";
			state->closed  = true;
			return -1;
		}
		dc = state->dc;
		return state->open && dc ? 1 : 0;
	}
};

// The host: one lobby connection holding the code, a peer connection per
// guest still connecting, and the channels that opened, waiting for accept().
struct Host_state {
	std::mutex                                          mutex;
	std::string                                         code;
	std::string                                         problem;
	json                                                ice = json::array();
	std::map<int, std::shared_ptr<rtc::PeerConnection>> connecting;
	std::deque<std::shared_ptr<Channel_state>>          ready;
};

class Rtc_host : public Friend_lobby_host {
public:
	explicit Rtc_host(const std::string& url) : state(std::make_shared<Host_state>()), lobby(lobby_socket()) {
		std::weak_ptr<Host_state>    weak = state;
		std::weak_ptr<rtc::WebSocket> ws  = lobby;
		lobby->onMessage([weak, ws](std::variant<rtc::binary, rtc::string> message) {
			if (auto host = weak.lock()) {
				try {
					take(host, ws, parse_lobby(message));
				} catch (const std::exception& error) {    // A field of the wrong type.
					std::cerr << "Friend Mode: ignored a bad lobby message (" << error.what() << ")" << std::endl;
				}
			}
		});
		lobby->onError([weak](std::string error) {
			if (auto host = weak.lock()) {
				const std::lock_guard<std::mutex> lock(host->mutex);
				host->problem = "Lobby error: " + error;
			}
		});
		lobby->onClosed([weak]() {
			if (auto host = weak.lock()) {
				const std::lock_guard<std::mutex> lock(host->mutex);
				if (host->problem.empty()) {
					host->problem = host->code.empty() ? "Could not reach the lobby" : "The lobby closed the code";
				}
				host->code.clear();
			}
		});
		lobby->open(url + "/host");
	}

	~Rtc_host() override {
		std::map<int, std::shared_ptr<rtc::PeerConnection>> connecting;
		std::deque<std::shared_ptr<Channel_state>>          ready;
		{
			const std::lock_guard<std::mutex> lock(state->mutex);
			connecting.swap(state->connecting);
			ready.swap(state->ready);
		}
		try {
			lobby->close();
			for (auto& entry : connecting) {
				entry.second->close();
			}
		} catch (const std::exception& error) {
			std::cerr << "Friend Mode: closing the lobby: " << error.what() << std::endl;
		}
		for (auto& channel : ready) {
			Rtc_connection(channel, Clock::now()).close();
		}
	}

	std::unique_ptr<Friend_connection> accept() override {
		// A small message now and then, so nothing between here and the
		// lobby takes the quiet connection for a dead one; the lobby
		// ignores it.  The game polls this every frame.
		const auto now = Clock::now();
		if (now - last_keepalive > std::chrono::seconds(30)) {
			last_keepalive = now;
			send_json(lobby, {{"type", "keepalive"}});
		}
		std::shared_ptr<Channel_state> channel;
		{
			const std::lock_guard<std::mutex> lock(state->mutex);
			if (state->ready.empty()) {
				return nullptr;
			}
			channel = std::move(state->ready.front());
			state->ready.pop_front();
		}
		return std::make_unique<Rtc_connection>(channel, Clock::now() + std::chrono::seconds(20));
	}

	std::string code() const override {
		const std::lock_guard<std::mutex> lock(state->mutex);
		return state->code;
	}

	std::string problem() const override {
		const std::lock_guard<std::mutex> lock(state->mutex);
		return state->problem;
	}

private:
	std::shared_ptr<Host_state>     state;
	std::shared_ptr<rtc::WebSocket> lobby;
	Clock::time_point               last_keepalive = Clock::now();

	static void take(const std::shared_ptr<Host_state>& host, const std::weak_ptr<rtc::WebSocket>& ws, const json& m) {
		if (!m.is_object()) {
			return;
		}
		const std::string type = m.value("type", "");
		if (type == "code") {
			const std::lock_guard<std::mutex> lock(host->mutex);
			const std::string code = m.value("code", "");
			host->code             = Friend_is_lobby_code(code) ? code : std::string();    // Shown as it is.
			host->ice     = m.contains("ice") ? m["ice"] : json::array();
			host->problem.clear();
		} else if (type == "guest" && m.contains("id")) {
			const int id = m["id"].get<int>();
			json      ice;
			{
				const std::lock_guard<std::mutex> lock(host->mutex);
				ice = host->ice;
			}
			auto pc = std::make_shared<rtc::PeerConnection>(config_from(ice));
			signal_through(pc, ws, {{"to", id}});
			std::weak_ptr<Host_state>          weak_host = host;
			std::weak_ptr<rtc::PeerConnection> weak_pc   = pc;
			pc->onDataChannel([weak_host, weak_pc, id](std::shared_ptr<rtc::DataChannel> dc) {
				auto host = weak_host.lock();
				auto pc   = weak_pc.lock();
				if (!host || !pc) {
					return;
				}
				auto channel = std::make_shared<Channel_state>();
				channel->pc  = pc;
				channel->dc  = dc;
				watch_channel(dc, channel);
				if (dc->isOpen()) {
					channel->open = true;
				}
				// Kept by the channel now: the lobby's list no longer needs it.
				const std::lock_guard<std::mutex> lock(host->mutex);
				host->ready.push_back(channel);
				host->connecting.erase(id);
				std::cout << "Friend Mode: lobby guest " << id << " connected directly" << std::endl;
			});
			const std::lock_guard<std::mutex> lock(host->mutex);
			host->connecting[id] = pc;
		} else if (type == "signal" && m.contains("from") && m.contains("data")) {
			std::shared_ptr<rtc::PeerConnection> pc;
			{
				const std::lock_guard<std::mutex> lock(host->mutex);
				const auto found = host->connecting.find(m["from"].get<int>());
				if (found != host->connecting.end()) {
					pc = found->second;
				}
			}
			if (pc) {
				take_signal(pc, m["data"]);
			}
		} else if (type == "left" && m.contains("id")) {
			// A guest that never connected gave up.  One that did keeps its
			// channel; leaving the lobby is what guests do once connected.
			std::shared_ptr<rtc::PeerConnection> pc;
			{
				const std::lock_guard<std::mutex> lock(host->mutex);
				const auto found = host->connecting.find(m["id"].get<int>());
				if (found != host->connecting.end()) {
					pc = std::move(found->second);
					host->connecting.erase(found);
				}
			}
			if (pc) {
				pc->close();
			}
		} else if (type == "error") {
			const std::lock_guard<std::mutex> lock(host->mutex);
			host->problem = lobby_text(m, "Lobby error");
		}
	}
};

void take_guest(const std::shared_ptr<Channel_state>& state, const std::weak_ptr<rtc::WebSocket>& ws, const json& m) {
	if (!m.is_object()) {
		return;
	}
	const std::string type = m.value("type", "");
	if (type == "joined") {
		auto pc = std::make_shared<rtc::PeerConnection>(config_from(m.contains("ice") ? m["ice"] : json::array()));
		signal_through(pc, ws, json::object());
		std::weak_ptr<Channel_state> weak = state;
		pc->onStateChange([weak](rtc::PeerConnection::State change) {
			if (change == rtc::PeerConnection::State::Failed) {
				if (auto channel = weak.lock()) {
					channel->fail("No direct connection could be made to the host");
				}
			}
		});
		// Reliable and ordered, as TCP is.
		auto dc = pc->createDataChannel("game");
		watch_channel(dc, state);
		const std::lock_guard<std::mutex> lock(state->mutex);
		if (state->closed) {
			return;    // Given up meanwhile; the game's close() lets these go.
		}
		state->pc = pc;
		state->dc = dc;
		std::cout << "Friend Mode: joined the lobby as guest " << m.value("id", 0) << "; connecting" << std::endl;
	} else if (type == "signal" && m.contains("data")) {
		std::shared_ptr<rtc::PeerConnection> pc;
		{
			const std::lock_guard<std::mutex> lock(state->mutex);
			pc = state->pc;
		}
		if (pc) {
			take_signal(pc, m["data"]);
		}
	} else if (type == "error") {
		state->fail(lobby_text(m, "Lobby error"));
	} else if (type == "closed") {
		state->fail("The host stopped hosting");
	}
}
}    // namespace

namespace {
std::mutex  public_ip_mutex;
std::string public_ip;
bool        public_ip_asked = false;
bool        public_ip_done  = false;

// GET https://<host>/ip over one address family only, so the lobby sees
// that address (the system may prefer IPv6 otherwise): a socket of that
// family, with Mbed TLS checking the lobby's certificate as for wss://.
// The answer, or empty.
std::string ask_ip_over(const std::string& host, int family) {
	addrinfo hints{};
	hints.ai_family     = family;
	hints.ai_socktype   = SOCK_STREAM;
	addrinfo* addresses = nullptr;
	if (getaddrinfo(host.c_str(), "443", &hints, &addresses) != 0 || !addresses) {
		return {};    // No address of this family (e.g. no IPv6 here).
	}
	int sock = -1;
	for (addrinfo* current = addresses; current && sock < 0; current = current->ai_next) {
		sock = static_cast<int>(socket(current->ai_family, current->ai_socktype, current->ai_protocol));
		if (sock < 0) {
			continue;
		}
#	ifdef _WIN32
		DWORD wait = 5000;
#	else
		timeval wait{5, 0};
#	endif
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
		if (connect(sock, current->ai_addr, static_cast<socklen_t>(current->ai_addrlen)) != 0) {
#	ifdef _WIN32
			closesocket(sock);
#	else
			::close(sock);
#	endif
			sock = -1;
		}
	}
	freeaddrinfo(addresses);
	if (sock < 0) {
		return {};
	}

	std::string             answer;
	mbedtls_ssl_context     ssl;
	mbedtls_ssl_config      conf;
	mbedtls_x509_crt        cas;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context random;
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
	mbedtls_x509_crt_init(&cas);
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&random);
	auto send_some = [](void* context, const unsigned char* data, size_t size) -> int {
		const int sent = static_cast<int>(send(*static_cast<int*>(context), reinterpret_cast<const char*>(data),
											   static_cast<int>(size), 0));
		return sent < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : sent;
	};
	auto receive_some = [](void* context, unsigned char* data, size_t size) -> int {
		const int got = static_cast<int>(recv(*static_cast<int*>(context), reinterpret_cast<char*>(data),
											  static_cast<int>(size), 0));
		return got < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : got;
	};
	const std::string ca_file = ca_certificate_file();
	const bool ready = psa_crypto_init() == PSA_SUCCESS && !ca_file.empty()
					   && mbedtls_x509_crt_parse_file(&cas, ca_file.c_str()) >= 0
					   && mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, nullptr, 0) == 0
					   && mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
														 MBEDTLS_SSL_PRESET_DEFAULT)
								  == 0;
	if (ready) {
		mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);    // Always checked.
		mbedtls_ssl_conf_ca_chain(&conf, &cas, nullptr);
		mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &random);
	}
	if (ready && mbedtls_ssl_setup(&ssl, &conf) == 0 && mbedtls_ssl_set_hostname(&ssl, host.c_str()) == 0) {
		mbedtls_ssl_set_bio(&ssl, &sock, send_some, receive_some, nullptr);
		int result;
		while ((result = mbedtls_ssl_handshake(&ssl)) == MBEDTLS_ERR_SSL_WANT_READ
			   || result == MBEDTLS_ERR_SSL_WANT_WRITE) {}
		const std::string request = "GET /ip HTTP/1.1\r\nHost: " + host
									+ "\r\nUser-Agent: Exult-Friend-Mode\r\nConnection: close\r\n\r\n";
		if (result == 0
			&& mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char*>(request.data()), request.size())
					   == static_cast<int>(request.size())) {
			std::string   response;
			unsigned char buffer[1024];
			for (;;) {
				const int got = mbedtls_ssl_read(&ssl, buffer, sizeof(buffer));
				if (got == MBEDTLS_ERR_SSL_WANT_READ || got == MBEDTLS_ERR_SSL_WANT_WRITE
					|| got == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
					continue;
				}
				if (got <= 0 || response.size() > 4096) {
					break;
				}
				response.append(reinterpret_cast<const char*>(buffer), static_cast<std::size_t>(got));
			}
			// "HTTP/1.1 200 ...", headers, a blank line, then the address.
			const auto body = response.find("\r\n\r\n");
			if (response.rfind("HTTP/1.1 200", 0) == 0 && body != std::string::npos) {
				std::string ip = response.substr(body + 4);
				while (!ip.empty() && std::isspace(static_cast<unsigned char>(ip.back()))) {
					ip.pop_back();
				}
				// An address is digits, dots, colons and hex letters only.
				const bool ok = !ip.empty() && ip.size() <= 45
								&& std::all_of(ip.begin(), ip.end(), [](char ch) {
									   return std::isxdigit(static_cast<unsigned char>(ch)) || ch == '.' || ch == ':';
								   });
				if (ok && (ip.find(':') != std::string::npos) == (family == AF_INET6)) {
					answer = ip;
				}
			}
		}
		mbedtls_ssl_close_notify(&ssl);
	}
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
	mbedtls_x509_crt_free(&cas);
	mbedtls_ctr_drbg_free(&random);
	mbedtls_entropy_free(&entropy);
#	ifdef _WIN32
	closesocket(sock);
#	else
	::close(sock);
#	endif
	return answer;
}
}    // namespace

void Friend_ask_public_ip(const std::string& lobby_url) {
	std::string problem;
	std::string url = lobby_base(lobby_url, problem);
	if (url.rfind("wss://", 0) != 0) {
		return;    // Only from a real (wss://) lobby.
	}
	{
		const std::lock_guard<std::mutex> lock(public_ip_mutex);
		if (public_ip_asked) {
			return;
		}
		public_ip_asked = true;
	}
	std::string host = url.substr(6);
	host             = host.substr(0, host.find_first_of("/:"));
	// In the background: a few seconds at most, then gone.  IPv4: the
	// address port forwarding uses.
	std::thread([host]() {
		const std::string ipv4 = ask_ip_over(host, AF_INET);
		const std::lock_guard<std::mutex> lock(public_ip_mutex);
		public_ip      = ipv4;
		public_ip_done = true;
	}).detach();
}

bool Friend_public_ip_done() {
	const std::lock_guard<std::mutex> lock(public_ip_mutex);
	return public_ip_done;
}

std::string Friend_public_ip() {
	const std::lock_guard<std::mutex> lock(public_ip_mutex);
	return public_ip;
}

std::unique_ptr<Friend_lobby_host> Friend_rtc_host(const std::string& lobby_url, std::string& problem) {
	try {
		start_logging();
		const auto url = lobby_base(lobby_url, problem);
		if (url.empty()) {
			std::cerr << "Friend Mode: " << problem << " (config/friend/lobby_url)" << std::endl;
			return nullptr;
		}
		return std::make_unique<Rtc_host>(url);
	} catch (const std::exception& error) {
		problem = std::string("Could not open the lobby (") + error.what() + ")";
		std::cerr << "Friend Mode: " << problem << std::endl;
		return nullptr;
	}
}

std::unique_ptr<Friend_connection> Friend_rtc_connect(
		const std::string& lobby_url, const std::string& code, int timeout_ms, std::string& problem) {
	try {
		start_logging();
		const auto url = lobby_base(lobby_url, problem);
		if (url.empty()) {
			return nullptr;
		}
		auto state   = std::make_shared<Channel_state>();
		auto ws      = lobby_socket();
		state->lobby = ws;
		std::weak_ptr<Channel_state>  weak    = state;
		std::weak_ptr<rtc::WebSocket> weak_ws = ws;
		ws->onMessage([weak, weak_ws](std::variant<rtc::binary, rtc::string> message) {
			if (auto channel = weak.lock()) {
				try {
					take_guest(channel, weak_ws, parse_lobby(message));
				} catch (const std::exception& error) {    // A field of the wrong type.
					std::cerr << "Friend Mode: ignored a bad lobby message (" << error.what() << ")" << std::endl;
				}
			}
		});
		ws->onError([weak](std::string error) {
			if (auto channel = weak.lock()) {
				channel->lobby_lost("Could not reach the lobby (" + error + ")");
			}
		});
		ws->onClosed([weak]() {
			if (auto channel = weak.lock()) {
				channel->lobby_lost("The lobby closed the connection");
			}
		});
		// The code as typed: the lobby takes any case, a space or a dash.
		std::string path;
		for (const char ch : code) {
			if (std::isalnum(static_cast<unsigned char>(ch))) {
				path.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
			} else if ((ch == ' ' || ch == '-') && !path.empty() && path.back() != '-') {
				path.push_back('-');
			}
		}
		if (path.size() > 32) {
			problem = "That code is too long";
			return nullptr;
		}
		ws->open(url + "/join/" + path);
		return std::make_unique<Rtc_connection>(state, Clock::now() + std::chrono::milliseconds(timeout_ms));
	} catch (const std::exception& error) {
		problem = std::string("Could not open the lobby (") + error.what() + ")";
		return nullptr;
	}
}

#endif
