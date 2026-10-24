/*
 * Friend Mode lobby: hands out short game codes and passes WebRTC
 * connection details (offers, answers, ICE candidates) between a host and
 * its guests, then steps aside.  It never carries game traffic.
 *
 * One Durable Object per code holds the host's WebSocket and its guests',
 * hibernating between messages (so an open code costs almost nothing while
 * nothing is said); one more per internet address counts its open codes.
 *
 *   wss://<lobby>/host        A host opens a game.  First message from the
 *                             lobby: {type:"code", code, ice}.
 *   wss://<lobby>/join/CODE   A guest joins.  First: {type:"joined", id, ice}.
 *   https://<lobby>/ip        The caller's own internet address (plain text),
 *                             for hosts who share an IP.  The game asks over
 *                             IPv4, the address port forwarding uses.
 *
 * Then, as JSON text messages:
 *   guest -> lobby  {type:"signal", data}           Passed to the host as
 *                                                    {type:"signal", from:id, data}.
 *   host  -> lobby  {type:"signal", to:id, data}    Passed to that guest as
 *                                                    {type:"signal", data}.
 *   lobby -> host   {type:"guest", id} / {type:"left", id}
 *   lobby -> guest  {type:"closed"} when the host leaves.
 *   lobby -> anyone {type:"error", message}
 *
 * 'data' is an SDP ({sdp, kind}) or a candidate ({candidate, mid}); the
 * lobby checks its shape and passes it on.  'ice' is the STUN (and, when
 * configured, TURN) servers to use.  See docs/friend-mode/lobby.md.
 *
 * Security (anyone on the internet can reach this):
 * - Codes come from crypto.getRandomValues, a word and four digits
 *   (32 x 9000), and the rate limits below make guessing one slow.
 * - Per address: opening games and joining are rate limited (the
 *   HOST_LIMITER and JOIN_LIMITER bindings in wrangler.toml).
 * - Per address: at most 3 codes open at once (the Hosts object).
 * - Per connection: 16 KB a message, 240 a minute; a guest's lobby
 *   connection ends after 60 s (it needs a few to connect); codes end when
 *   the host leaves or after 12 hours.
 * - Browsers are turned away (the game sends no Origin header), so a web
 *   page can't use the lobby on a visitor's behalf.
 * - No secrets in the repo: TURN keys are Worker secrets.
 */

const PROTOCOL = 1;    // The lobby's own messages (not the game's).

// Britannian words for codes: easy to read out and type.
const WORDS = [
	"ANKH", "BRITAIN", "COVE", "DAGGER", "EMPATH", "FERRY", "GARGOYLE", "HONOR",
	"JHELOM", "KEG", "LYCAEUM", "MINOC", "MOONGLOW", "NICODEMUS", "ORB", "PAWS",
	"RAVEN", "SERPENT", "SKARA", "SPIRIT", "TRINSIC", "VESPER", "VIRTUE", "WISP",
	"YEW", "BUCCANEER", "DESTARD", "HYTHLOTH", "LOCK", "LUTE", "MANDRAKE", "SHRINE",
];

const MAX_GUESTS = 8;               // One per companion, and a few watching.
const GUEST_SECONDS = 60;           // A guest's lobby connection, at most.
const MAX_CODE_LENGTH = 32;
const CODE_PATTERN = /^[A-Z]{2,12}-[0-9]{2,4}$/;   // Four digits now; two in older codes.
const MAX_MESSAGE_BYTES = 16 * 1024;  // An SDP is a few kilobytes.
const MAX_MESSAGES_PER_MINUTE = 240;  // Per socket: candidates come in bursts.
const GAME_HOURS = 12;              // A code lasts this long at most.
const CODES_PER_ADDRESS = 3;        // Open at once, from one internet address.
// The host's keep-alive, answered without waking the object (see
// friendrtc.cc: the game sends exactly this).
const KEEPALIVE = '{"type":"keepalive"}';
const KEEPALIVE_ANSWER = '{"type":"keepalive_ok"}';

function json(ws, message) {
	try {
		ws.send(JSON.stringify(message));
	} catch {
		// Closed meanwhile.
	}
}

// A random number below 'n', from the system's secure random source.
function randomBelow(n) {
	const values = new Uint32Array(1);
	const limit = Math.floor(0x100000000 / n) * n;    // No bias toward low numbers.
	do {
		crypto.getRandomValues(values);
	} while (values[0] >= limit);
	return values[0] % n;
}

function newCode() {
	const word = WORDS[randomBelow(WORDS.length)];
	const number = 1000 + randomBelow(9000);
	return `${word}-${number}`;
}

// The code as typed (any case, a space or a dash), or null if it can't be one.
function normalCode(code) {
	let text;
	try {
		text = decodeURIComponent(code || "");
	} catch {
		return null;
	}
	if (text.length > MAX_CODE_LENGTH) {
		return null;
	}
	text = text.trim().toUpperCase().replace(/[\s-]+/g, "-");
	return CODE_PATTERN.test(text) ? text : null;
}

// True when this address may go on; false when it is over its rate limit.
// Without the binding (wrangler dev, older wrangler) there is no limit.
async function allowed(limiter, request) {
	if (!limiter) {
		return true;
	}
	const address = request.headers.get("CF-Connecting-IP") || "unknown";
	try {
		const { success } = await limiter.limit({ key: address });
		return success;
	} catch {
		return true;
	}
}

// A signal's 'data', as the game sends it, or null.
function signalData(data) {
	if (!data || typeof data !== "object" || Array.isArray(data)) {
		return null;
	}
	const text = (value) => typeof value === "string" && value.length <= MAX_MESSAGE_BYTES;
	if (text(data.sdp) && (data.kind === "offer" || data.kind === "answer")) {
		return { sdp: data.sdp, kind: data.kind };
	}
	if (text(data.candidate) && text(data.mid)) {
		return { candidate: data.candidate, mid: data.mid };
	}
	return null;
}

// STUN always; TURN when the Worker has a TURN key (secrets TURN_KEY_ID and
// TURN_KEY_API_TOKEN), with short-lived credentials made per game.
async function iceServers(env) {
	const servers = [{ urls: ["stun:stun.cloudflare.com:3478"] }];
	if (!env.TURN_KEY_ID || !env.TURN_KEY_API_TOKEN) {
		return servers;
	}
	try {
		const response = await fetch(
			`https://rtc.live.cloudflare.com/v1/turn/keys/${env.TURN_KEY_ID}/credentials/generate-ice-servers`,
			{
				method: "POST",
				headers: {
					Authorization: `Bearer ${env.TURN_KEY_API_TOKEN}`,
					"Content-Type": "application/json",
				},
				body: JSON.stringify({ ttl: GAME_HOURS * 3600 }),
			},
		);
		if (response.ok) {
			const body = await response.json();
			if (Array.isArray(body.iceServers)) {
				return body.iceServers;
			}
		}
	} catch {
		// STUN alone, then.
	}
	return servers;
}

// "No game with that code", without waking a Durable Object for it.
function noGame() {
	const [client, server] = Object.values(new WebSocketPair());
	server.accept();
	json(server, { type: "error", message: "No game with that code" });
	server.close(1000, "No such game");
	return new Response(null, { status: 101, webSocket: client });
}

export default {
	async fetch(request, env) {
		const url = new URL(request.url);
		if (url.pathname === "/" || url.pathname === "/health") {
			return new Response(`Friend Mode lobby, protocol ${PROTOCOL}\n`, {
				headers: { "Content-Type": "text/plain" },
			});
		}
		if (request.headers.get("Origin")) {
			return new Response("Not for browsers", { status: 403 });
		}
		if (url.pathname === "/ip" && request.method === "GET") {
			// Only ever the caller's own address, which Cloudflare already
			// knows; no Durable Object involved.
			if (!(await allowed(env.JOIN_LIMITER, request))) {
				return new Response("Too many tries from this address; wait a minute", { status: 429 });
			}
			return new Response(`${request.headers.get("CF-Connecting-IP") || ""}\n`, {
				headers: { "Content-Type": "text/plain", "Cache-Control": "no-store" },
			});
		}
		if (request.headers.get("Upgrade") !== "websocket") {
			return new Response("Expected a WebSocket", { status: 426 });
		}
		if (url.pathname === "/host") {
			if (!(await allowed(env.HOST_LIMITER, request))) {
				return new Response("Too many games from this address; wait a minute", { status: 429 });
			}
			const address = request.headers.get("CF-Connecting-IP") || "unknown";
			// A free code: a few tries, in case one is taken.
			for (let tries = 0; tries < 8; ++tries) {
				const code = newCode();
				const lobby = env.LOBBY.get(env.LOBBY.idFromName(code));
				const probe = await lobby.fetch(`https://lobby/taken?code=${code}`);
				if ((await probe.text()) !== "free") {
					continue;
				}
				// Count it against this address (at most CODES_PER_ADDRESS).
				const hosts = env.HOSTS.get(env.HOSTS.idFromName(address));
				const room = await hosts.fetch(`https://hosts/open?code=${code}`);
				if ((await room.text()) !== "ok") {
					return new Response("Too many games open from this address", { status: 429 });
				}
				const params = new URLSearchParams({ code, address });
				return lobby.fetch(`https://lobby/host?${params}`, request);
			}
			return new Response("No free code; try again", { status: 503 });
		}
		const join = url.pathname.match(/^\/join\/([^/]+)$/);
		if (join) {
			if (!(await allowed(env.JOIN_LIMITER, request))) {
				return new Response("Too many tries from this address; wait a minute", { status: 429 });
			}
			const code = normalCode(join[1]);
			if (!code) {
				return noGame();
			}
			const lobby = env.LOBBY.get(env.LOBBY.idFromName(code));
			return lobby.fetch(`https://lobby/join?code=${encodeURIComponent(code)}`, request);
		}
		return new Response("Not found", { status: 404 });
	},
};

// One game: its host and guests.  Hibernating: the object sleeps between
// messages and forgets its memory, so what it needs is kept in storage
// (the code, the ICE servers, when it ends) and with each connection (who
// it is, its message count).  A sleeping object wakes in milliseconds when
// a message comes.
export class Lobby {
	constructor(state, env) {
		this.state = state;
		this.env = env;
		this.state.setWebSocketAutoResponse(new WebSocketRequestResponsePair(KEEPALIVE, KEEPALIVE_ANSWER));
	}

	host() {
		return this.state.getWebSockets("host")[0] || null;
	}

	guest(id) {
		return this.state.getWebSockets(`guest-${id}`)[0] || null;
	}

	guests() {
		return this.state.getWebSockets("guest");
	}

	async game() {
		return (await this.state.storage.get("game")) || null;
	}

	async fetch(request) {
		const url = new URL(request.url);
		const game = await this.game();
		if (url.pathname === "/taken") {
			return new Response(this.host() && game && Date.now() < game.expires ? "taken" : "free");
		}
		const [client, server] = Object.values(new WebSocketPair());
		if (url.pathname === "/host") {
			await this.openGame(server, url.searchParams.get("code"), url.searchParams.get("address"));
		} else {
			await this.addGuest(server, game);
		}
		return new Response(null, { status: 101, webSocket: client });
	}

	async openGame(ws, code, address) {
		const game = {
			code,
			address,
			expires: Date.now() + GAME_HOURS * 3600 * 1000,
			ice: await iceServers(this.env),
			nextGuest: 1,
		};
		await this.state.storage.put("game", game);
		this.state.acceptWebSocket(ws, ["host"]);
		ws.serializeAttachment({ role: "host", windowStart: Date.now(), count: 0 });
		json(ws, { type: "code", code, ice: game.ice, protocol: PROTOCOL });
	}

	async addGuest(ws, game) {
		const refuse = (message, reason) => {
			ws.accept();
			json(ws, { type: "error", message });
			ws.close(1000, reason);
		};
		const host = this.host();
		if (!host || !game || Date.now() >= game.expires) {
			refuse("No game with that code", "No such game");
			return;
		}
		if (this.guests().length >= MAX_GUESTS) {
			refuse("The game is full", "Full");
			return;
		}
		const id = game.nextGuest++;
		await this.state.storage.put("game", game);
		this.state.acceptWebSocket(ws, ["guest", `guest-${id}`]);
		const until = Date.now() + GUEST_SECONDS * 1000;
		ws.serializeAttachment({ role: "guest", id, until, windowStart: Date.now(), count: 0 });
		// Long enough to connect; then the slot is free again, so codes
		// passed around can't fill a game with idle lobby connections.
		const alarm = await this.state.storage.getAlarm();
		if (!alarm || alarm > until) {
			await this.state.storage.setAlarm(until);
		}
		json(ws, { type: "joined", id, ice: game.ice, protocol: PROTOCOL });
		json(host, { type: "guest", id });
	}

	// Guests that took too long: closed, and the next alarm set.
	async alarm() {
		const now = Date.now();
		let next = 0;
		for (const ws of this.guests()) {
			const who = ws.deserializeAttachment() || {};
			if (who.until <= now) {
				json(ws, { type: "error", message: "Took too long to connect" });
				this.closeGuest(ws, who, "Too long");
			} else if (!next || who.until < next) {
				next = who.until;
			}
		}
		if (next) {
			await this.state.storage.setAlarm(next);
		}
	}

	// Messages in, with size and rate limits.
	async webSocketMessage(ws, data) {
		const who = ws.deserializeAttachment() || {};
		const now = Date.now();
		if (now - who.windowStart > 60000) {
			who.windowStart = now;
			who.count = 0;
		}
		who.count = (who.count || 0) + 1;
		ws.serializeAttachment(who);
		if (who.count > MAX_MESSAGES_PER_MINUTE || typeof data !== "string" || data.length > MAX_MESSAGE_BYTES) {
			json(ws, { type: "error", message: "Too much" });
			ws.close(1008, "Too much");
			await this.gone(ws, who);
			return;
		}
		let message;
		try {
			message = JSON.parse(data);
		} catch {
			return;
		}
		if (!message || typeof message !== "object") {
			return;
		}
		const signal = signalData(message.data);
		if (who.role === "host") {
			const guest = Number.isInteger(message.to) ? this.guest(message.to) : null;
			if (message.type === "signal" && signal && guest) {
				json(guest, { type: "signal", data: signal });
			} else if (message.type === "kick" && guest) {
				this.closeGuest(guest, guest.deserializeAttachment() || {}, "Removed by the host");
			}
		} else if (who.role === "guest") {
			const host = this.host();
			if (message.type === "signal" && signal && host) {
				json(host, { type: "signal", from: who.id, data: signal });
			}
		}
	}

	async webSocketClose(ws, code) {
		try {
			ws.close(code === 1005 ? 1000 : code, "Closed");    // Finish the closing handshake.
		} catch {
			// Already closed.
		}
		await this.gone(ws, ws.deserializeAttachment() || {});
	}

	async webSocketError(ws) {
		await this.gone(ws, ws.deserializeAttachment() || {});
	}

	// Close a guest from here, telling the host once.
	closeGuest(ws, who, reason) {
		if (who.gone) {
			return;
		}
		who.gone = true;
		ws.serializeAttachment(who);
		ws.close(1000, reason);
		this.guestGone(who.id);
	}

	guestGone(id) {
		const host = this.host();
		if (host) {
			json(host, { type: "left", id });
		}
	}

	// A connection ended.  The host leaving ends the game and its code.
	async gone(ws, who) {
		if (who.gone) {
			return;    // Already handled (closed by us, then the close event).
		}
		who.gone = true;
		try {
			ws.serializeAttachment(who);
		} catch {
			// Closed already.
		}
		if (who.role === "guest") {
			this.guestGone(who.id);
			return;
		}
		if (who.role !== "host") {
			return;
		}
		for (const guest of this.guests()) {
			json(guest, { type: "closed" });
			guest.close(1000, "The host left");
		}
		const game = await this.game();
		await this.state.storage.deleteAll();
		if (game && game.address) {
			const hosts = this.env.HOSTS.get(this.env.HOSTS.idFromName(game.address));
			await hosts.fetch(`https://hosts/close?code=${encodeURIComponent(game.code)}`);
		}
	}
}

// One internet address: its open codes (and when each would end anyway),
// so one address can't hold more than CODES_PER_ADDRESS at once.
export class Hosts {
	constructor(state, env) {
		this.state = state;
		this.env = env;
	}

	async fetch(request) {
		const url = new URL(request.url);
		const code = url.searchParams.get("code") || "";
		const now = Date.now();
		const open = (await this.state.storage.get("open")) || {};
		for (const [each, expires] of Object.entries(open)) {
			if (expires <= now) {
				delete open[each];
			}
		}
		if (url.pathname === "/open") {
			if (Object.keys(open).length >= CODES_PER_ADDRESS) {
				// Codes whose host went without a goodbye (a lobby
				// redeploy, say) don't count: ask each.
				for (const each of Object.keys(open)) {
					const lobby = this.env.LOBBY.get(this.env.LOBBY.idFromName(each));
					const probe = await lobby.fetch(`https://lobby/taken?code=${each}`);
					if ((await probe.text()) === "free") {
						delete open[each];
					}
				}
			}
			if (Object.keys(open).length >= CODES_PER_ADDRESS) {
				await this.state.storage.put("open", open);
				return new Response("full");
			}
			open[code] = now + GAME_HOURS * 3600 * 1000;
		} else if (url.pathname === "/close") {
			delete open[code];
		}
		if (Object.keys(open).length) {
			await this.state.storage.put("open", open);
		} else {
			await this.state.storage.deleteAll();
		}
		return new Response("ok");
	}
}
