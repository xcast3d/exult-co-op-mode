/*
 * Phase 8 check without the game's windows: Friend Mode's own session code
 * (friendnet.cc) as a host and two guests in one program.  One guest joins
 * by lobby code over WebRTC, the other by address over TCP; each gets a
 * world-sized blob intact, and chat and a move go both ways.
 *
 *   tools/friend-mode/build-rtc.sh
 *   tools/friend-mode/rtc-session-test/build.sh
 *   /tmp/rtc_session_test [lobby]     (default: the deployed lobby)
 *
 * For a lobby on this machine: /tmp/rtc_session_test ws://127.0.0.1:8787
 */

#include "friendconn.h"
#include "friendnet.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
int failures = 0;

void check(const std::string& what, bool ok) {
	std::cout << (ok ? "ok   " : "FAIL ") << what << std::endl;
	if (!ok) {
		++failures;
	}
}

// Poll every session until 'done' or 'seconds' pass.
bool run_until(std::vector<Friend_mode_session*> sessions, const std::function<bool()>& done, int seconds) {
	const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
	while (std::chrono::steady_clock::now() < end) {
		for (auto* session : sessions) {
			session->poll();
		}
		if (done()) {
			return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

// One guest: joins, gets a big blob, and chats and moves both ways.
void try_guest(Friend_mode_session& host, Friend_mode_session& guest, const std::string& how) {
	const auto started = std::chrono::steady_clock::now();
	int        player  = 0;
	check(how + ": guest is accepted",
		  run_until({&host, &guest}, [&] { return guest.connected() && host.take_peer_joined(&player); }, 25));
	if (!guest.connected()) {
		std::cout << "     problem: " << guest.connection_problem() << std::endl;
		return;
	}
	const auto ms
			= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
	std::cout << "     connected in " << ms << " ms, as player " << player << std::endl;

	// A blob like the world download (about 190 KB in a real game).
	std::vector<unsigned char> world(300 * 1024);
	for (std::size_t i = 0; i < world.size(); ++i) {
		world[i] = static_cast<unsigned char>((i * 7 + i / 251) & 0xff);
	}
	host.send_blob(Friend_mode_session::Blob_kind::world_save, world, player);
	Friend_mode_session::Blob_kind kind{};
	std::vector<unsigned char>     got;
	const bool arrived = run_until({&host, &guest}, [&] { return guest.take_blob(kind, got); }, 20);
	check(how + ": world-sized blob arrives intact",
		  arrived && kind == Friend_mode_session::Blob_kind::world_save && got == world);

	host.send_chat(Friend_mode_session::chat_spectator, Friend_mode_session::host_player, "Hail from the host", player);
	uint32      speaker = 0;
	int         from    = 0;
	std::string text;
	check(how + ": chat host to guest",
		  run_until({&host, &guest}, [&] { return guest.take_chat(speaker, from, text); }, 10)
				  && text == "Hail from the host");
	guest.send_chat(Friend_mode_session::chat_spectator, player, "Hail from the guest");
	check(how + ": chat guest to host",
		  run_until({&host, &guest}, [&] { return host.take_chat(speaker, from, text); }, 10)
				  && text == "Hail from the guest");

	guest.send_move_request(1000, 2000, 0, 3, 7);
	int    tx = 0, ty = 0, tz = 0, frame = 0;
	uint32 seq = 0;
	check(how + ": a step reaches the host",
		  run_until({&host, &guest}, [&] { return host.take_move_request(tx, ty, tz, frame, seq); }, 10) && tx == 1000
				  && ty == 2000 && seq == 7);
}
}    // namespace

int main(int argc, char** argv) {
	const std::string lobby     = argc > 1 ? argv[1] : "wss://u7-multiplayer-lobby.xcast3d.com";
	const uint32      game_code = 0x1234abcd;
	const int         port      = 19920;

	check("codes and addresses are told apart",
		  Friend_is_lobby_code("MOONGLOW-4271") && Friend_is_lobby_code("moonglow 42")
				  && !Friend_is_lobby_code("127.0.0.1") && !Friend_is_lobby_code("my-mac.local")
				  && !Friend_is_lobby_code("100.64.0.1") && !Friend_is_lobby_code("localhost"));

	Friend_mode_session host;
	host.set_local_player_name("Host");
	check("host starts (TCP and lobby)", host.start_host(port, game_code, lobby));
	std::string code;
	run_until({&host}, [&] { return !(code = host.lobby_code()).empty() || !host.lobby_problem().empty(); }, 15);
	check("host gets a code from " + lobby + " (" + (code.empty() ? host.lobby_problem() : code) + ")", !code.empty());

	if (!code.empty()) {
		Friend_mode_session guest;
		guest.set_local_player_name("Iolo's player");
		check("guest starts joining by code", guest.start_guest(code, port, game_code, lobby));
		try_guest(host, guest, "by code");
	}

	Friend_mode_session tcp_guest;
	tcp_guest.set_local_player_name("Shamino's player");
	check("guest connects by address", tcp_guest.start_guest("127.0.0.1", port, game_code, lobby));
	try_guest(host, tcp_guest, "by address");

	Friend_mode_session lost;
	lost.start_guest("NOWHERE-0000", port, game_code, lobby);
	run_until({&lost}, [&] { return !lost.peer_open(); }, 15);
	check("a wrong code says so (" + lost.connection_problem() + ")",
		  lost.connection_problem().find("No game") != std::string::npos);

	// A password: no answer and a wrong one are refused, the right one
	// gets in (over TCP, where a password matters most).
	host.set_password("moonglow");
	for (const std::string& tried : {std::string(), std::string("wrong")}) {
		Friend_mode_session refused;
		refused.set_password(tried);
		refused.start_guest("127.0.0.1", port, game_code, lobby);
		run_until({&host, &refused}, [&] { return !refused.peer_open(); }, 10);
		check("password " + (tried.empty() ? std::string("missing") : tried) + " is refused",
			  refused.rejected_password() && !refused.connected());
	}
	Friend_mode_session with_password;
	with_password.set_local_player_name("Dupre's player");
	with_password.set_password("moonglow");
	with_password.start_guest("127.0.0.1", port, game_code, lobby);
	int player = 0;
	check("the right password gets in",
		  run_until({&host, &with_password},
					[&] { return with_password.connected() && host.take_peer_joined(&player); }, 10));

	// Kick: told, closed, and the same name can't come back.
	host.kick(player);
	run_until({&host, &with_password}, [&] { return !with_password.peer_open(); }, 10);
	check("a kicked guest is told", with_password.was_kicked());
	Friend_mode_session again;
	again.set_local_player_name("Dupre's player");
	again.set_password("moonglow");
	again.start_guest("127.0.0.1", port, game_code, lobby);
	run_until({&host, &again}, [&] { return !again.peer_open(); }, 10);
	check("a kicked name is turned away", again.was_kicked());

	// Guessing: after five wrong passwords in a minute, even the right one
	// waits.
	for (int i = 0; i < 5; ++i) {
		Friend_mode_session guess;
		guess.set_password("guess" + std::to_string(i));
		guess.start_guest("127.0.0.1", port, game_code, lobby);
		run_until({&host, &guess}, [&] { return !guess.peer_open(); }, 10);
	}
	Friend_mode_session right_too_soon;
	right_too_soon.set_local_player_name("Spark's player");
	right_too_soon.set_password("moonglow");
	right_too_soon.start_guest("127.0.0.1", port, game_code, lobby);
	run_until({&host, &right_too_soon}, [&] { return !right_too_soon.peer_open() || right_too_soon.connected(); }, 10);
	check("after five wrong passwords, guessing waits a minute",
		  right_too_soon.rejected_password() && !right_too_soon.connected());

	// Connections that never say hello: at most four held, then closed.
	std::vector<std::unique_ptr<Friend_connection>> silent;
	for (int i = 0; i < 6; ++i) {
		silent.push_back(Friend_tcp_connect("127.0.0.1", port, 2000));
	}
	run_until({&host}, [] { return false; }, 1);
	auto dropped = [&] {
		int count = 0;
		for (auto& each : silent) {
			unsigned char byte;
			count += each && each->read(&byte, 1) < 0 ? 1 : 0;
		}
		return count;
	};
	check("only four silent connections are held", dropped() == 2);
	run_until({&host}, [] { return false; }, 16);
	check("silent connections close after 15 seconds", dropped() == 6);

	// Closing the lobby ends the code.
	host.close_lobby();
	check("closing the lobby ends the code", host.lobby_code().empty() && !host.lobby_open());

	// Stopping hosting: guests are told, and the host is left with none.
	// (No password now: guessing still waits from above.)
	host.set_password("");
	Friend_mode_session last;
	last.set_local_player_name("Jaana's player");
	last.set_password("moonglow");
	last.start_guest("127.0.0.1", port, game_code, lobby);
	run_until({&host, &last}, [&] { return last.connected(); }, 10);
	host.end_hosting();
	run_until({&host, &last}, [&] { return !last.peer_open() && !host.has_peers(); }, 10);
	check("stopping hosting tells guests and closes", last.host_stopped() && !host.has_peers() && !host.tcp_open());

	std::cout << (failures ? std::to_string(failures) + " failed" : "all passed") << std::endl;
	host.stop();
	return failures ? 1 : 0;
}
