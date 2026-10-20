/*
 * Phase 8 spike: two programs meet through the Friend Mode lobby
 * (tools/lobby) and talk over a WebRTC data channel, as Exult's host and
 * guest would.  Proves libdatachannel builds here, signalling through the
 * lobby works, and a reliable, ordered channel carries small messages and a
 * big one (like the world download) intact.
 *
 *   rtc_spike host <lobby>          prints the code, echoes what arrives
 *   rtc_spike join <lobby> <code>   connects, sends, checks the echoes
 *
 * <lobby> is ws://127.0.0.1:8787 for `npm run dev`, or
 * wss://u7-multiplayer-lobby.xcast3d.com once deployed.
 */

#include <rtc/rtc.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

std::mutex out_mutex;

void say(const std::string& line) {
	const std::lock_guard<std::mutex> lock(out_mutex);
	std::cout << line << std::endl;
}

// The lobby's 'ice' list: [{urls: [...], username?, credential?}].
rtc::Configuration Config_from(const json& ice) {
	rtc::Configuration config;
	for (const auto& server : ice) {
		const auto& urls = server["urls"];
		std::vector<std::string> list;
		if (urls.is_string()) {
			list.push_back(urls.get<std::string>());
		} else {
			for (const auto& url : urls) {
				list.push_back(url.get<std::string>());
			}
		}
		for (const auto& url : list) {
			if (url.rfind("turn", 0) == 0 && server.contains("username")) {
				// "turn:host:port?transport=udp" with the lobby's credentials.
				rtc::IceServer turn(url);
				turn.username = server["username"].get<std::string>();
				turn.password = server["credential"].get<std::string>();
				config.iceServers.push_back(turn);
			} else if (url.rfind("stun", 0) == 0) {
				config.iceServers.emplace_back(url);
			}
		}
	}
	return config;
}

// The lobby connection.  Mbed TLS has no list of trusted certificate
// authorities of its own, so wss:// needs one: RTC_CA_FILE, or macOS's
// /etc/ssl/cert.pem (Windows will need one shipped with the game).
std::shared_ptr<rtc::WebSocket> Lobby_socket() {
	rtc::WebSocketConfiguration config;
	const char*                 file = std::getenv("RTC_CA_FILE");
	std::string                 path = file ? file : "/etc/ssl/cert.pem";
	if (FILE* f = std::fopen(path.c_str(), "r")) {
		std::fclose(f);
		config.caCertificatePemFile = path;
	}
	return std::make_shared<rtc::WebSocket>(config);
}

void Send(const std::shared_ptr<rtc::WebSocket>& ws, const json& message) {
	if (ws->isOpen()) {
		ws->send(message.dump());
	}
}

// Wire a peer connection's offers, answers and candidates to the lobby.
void Signal_through(const std::shared_ptr<rtc::PeerConnection>& pc, const std::shared_ptr<rtc::WebSocket>& ws,
					json to) {
	pc->onLocalDescription([ws, to](rtc::Description description) {
		json message = to;
		message["type"] = "signal";
		message["data"] = {{"sdp", std::string(description)}, {"kind", description.typeString()}};
		Send(ws, message);
	});
	pc->onLocalCandidate([ws, to](rtc::Candidate candidate) {
		json message = to;
		message["type"] = "signal";
		message["data"] = {{"candidate", std::string(candidate)}, {"mid", candidate.mid()}};
		Send(ws, message);
	});
	pc->onStateChange([](rtc::PeerConnection::State state) {
		std::ostringstream text;
		text << "peer connection: " << state;
		say(text.str());
	});
}

void Take_signal(const std::shared_ptr<rtc::PeerConnection>& pc, const json& data) {
	if (data.contains("sdp")) {
		pc->setRemoteDescription(rtc::Description(data["sdp"].get<std::string>(), data["kind"].get<std::string>()));
	} else if (data.contains("candidate")) {
		pc->addRemoteCandidate(rtc::Candidate(data["candidate"].get<std::string>(), data["mid"].get<std::string>()));
	}
}

int Host(const std::string& lobby) {
	auto ws  = Lobby_socket();
	json ice = json::array();
	std::mutex lock;
	std::map<int, std::shared_ptr<rtc::PeerConnection>> peers;
	std::map<int, std::shared_ptr<rtc::DataChannel>>    channels;
	ws->onMessage([&](std::variant<rtc::binary, rtc::string> message) {
		if (!std::holds_alternative<rtc::string>(message)) {
			return;
		}
		const json m = json::parse(std::get<rtc::string>(message), nullptr, false);
		const std::string type = m.value("type", "");
		if (type == "code") {
			ice = m["ice"];
			say("CODE " + m["code"].get<std::string>());
		} else if (type == "guest") {
			const int id = m["id"];
			auto      pc = std::make_shared<rtc::PeerConnection>(Config_from(ice));
			Signal_through(pc, ws, {{"to", id}});
			pc->onDataChannel([&, id](std::shared_ptr<rtc::DataChannel> dc) {
				say("host: channel '" + dc->label() + "' from guest " + std::to_string(id));
				std::weak_ptr<rtc::DataChannel> weak = dc;
				dc->onMessage([weak](std::variant<rtc::binary, rtc::string> data) {
					auto channel = weak.lock();
					if (!channel) {
						return;
					}
					// Echo: strings as they are, bytes as their size and a checksum.
					if (std::holds_alternative<rtc::string>(data)) {
						channel->send("echo: " + std::get<rtc::string>(data));
					} else {
						const auto& bytes = std::get<rtc::binary>(data);
						unsigned    sum   = 0;
						for (auto b : bytes) {
							sum = sum * 31 + static_cast<unsigned>(b);
						}
						channel->send("got " + std::to_string(bytes.size()) + " bytes, sum " + std::to_string(sum));
					}
				});
				const std::lock_guard<std::mutex> guard(lock);
				channels[id] = dc;
			});
			const std::lock_guard<std::mutex> guard(lock);
			peers[id] = pc;
		} else if (type == "signal") {
			std::shared_ptr<rtc::PeerConnection> pc;
			{
				const std::lock_guard<std::mutex> guard(lock);
				auto                              found = peers.find(m["from"].get<int>());
				if (found != peers.end()) {
					pc = found->second;
				}
			}
			if (pc) {
				Take_signal(pc, m["data"]);
			}
		} else if (type == "left") {
			const std::lock_guard<std::mutex> guard(lock);
			peers.erase(m["id"].get<int>());
			channels.erase(m["id"].get<int>());
			say("host: guest left");
		} else if (type == "error") {
			say("lobby error: " + m.value("message", ""));
		}
	});
	ws->onError([](std::string error) {
		say("lobby connection error: " + error);
	});
	ws->open(lobby + "/host");
	// Serve for a while (the test's guest runs meanwhile).
	for (int i = 0; i < 120; ++i) {
		std::this_thread::sleep_for(500ms);
	}
	return 0;
}

int Join(const std::string& lobby, const std::string& code) {
	auto                                 ws = Lobby_socket();
	std::shared_ptr<rtc::PeerConnection> pc;
	std::shared_ptr<rtc::DataChannel>    dc;
	std::atomic<int>                     answers{0};
	std::atomic<bool>                    failed{false};
	const auto                           started = std::chrono::steady_clock::now();
	// A big message, like a piece of the world download.
	rtc::binary big(256 * 1024);
	unsigned    sum = 0;
	for (size_t i = 0; i < big.size(); ++i) {
		big[i] = static_cast<std::byte>((i * 7 + 3) & 0xff);
		sum    = sum * 31 + static_cast<unsigned>(big[i]);
	}
	const std::string want_big = "got " + std::to_string(big.size()) + " bytes, sum " + std::to_string(sum);
	ws->onMessage([&](std::variant<rtc::binary, rtc::string> message) {
		if (!std::holds_alternative<rtc::string>(message)) {
			return;
		}
		const json m = json::parse(std::get<rtc::string>(message), nullptr, false);
		const std::string type = m.value("type", "");
		if (type == "joined") {
			say("joined as guest " + std::to_string(m["id"].get<int>()));
			pc = std::make_shared<rtc::PeerConnection>(Config_from(m["ice"]));
			Signal_through(pc, ws, json::object());
			dc = pc->createDataChannel("game");    // Reliable and ordered, as TCP was.
			dc->onOpen([&]() {
				const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
										std::chrono::steady_clock::now() - started)
										.count();
				say("guest: channel open after " + std::to_string(ms) + " ms");
				for (int i = 1; i <= 50; ++i) {
					dc->send("step " + std::to_string(i));
				}
				dc->send(big);
			});
			dc->onMessage([&](std::variant<rtc::binary, rtc::string> data) {
				if (!std::holds_alternative<rtc::string>(data)) {
					return;
				}
				const std::string text = std::get<rtc::string>(data);
				const int         n    = ++answers;
				// In order: 50 echoes, then the big one's check.
				if (n <= 50 && text != "echo: step " + std::to_string(n)) {
					say("OUT OF ORDER: " + text);
					failed = true;
				}
				if (n == 51) {
					if (text == want_big) {
						say("guest: big message arrived intact (" + text + ")");
					} else {
						say("BIG MESSAGE WRONG: " + text);
						failed = true;
					}
				}
			});
		} else if (type == "signal" && pc) {
			Take_signal(pc, m["data"]);
		} else if (type == "error") {
			say("lobby error: " + m.value("message", ""));
			failed = true;
		} else if (type == "closed") {
			say("host left");
		}
	});
	ws->onError([&](std::string error) {
		say("lobby connection error: " + error);
		failed = true;
	});
	ws->open(lobby + "/join/" + code);
	for (int i = 0; i < 60 && answers < 51 && !failed; ++i) {
		std::this_thread::sleep_for(250ms);
	}
	const bool ok = answers == 51 && !failed;
	say(ok ? "SPIKE PASSED" : "SPIKE FAILED (" + std::to_string(answers.load()) + " of 51 answers)");
	if (dc) {
		dc->close();
	}
	if (pc) {
		pc->close();
	}
	ws->close();
	std::this_thread::sleep_for(300ms);
	return ok ? 0 : 1;
}

}    // namespace

int main(int argc, char** argv) {
	rtc::InitLogger(rtc::LogLevel::Warning);
	if (argc >= 3 && std::string(argv[1]) == "host") {
		return Host(argv[2]);
	}
	if (argc >= 4 && std::string(argv[1]) == "join") {
		return Join(argv[2], argv[3]);
	}
	std::cerr << "usage: rtc_spike host <lobby> | rtc_spike join <lobby> <code>" << std::endl;
	return 2;
}
