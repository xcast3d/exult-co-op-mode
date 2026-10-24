/*
 * Friend Mode lobby: hands out short game codes and passes WebRTC
 * connection details (offers, answers, ICE candidates) between a host and
 * its guests, then steps aside.  It never carries game traffic.
 *
 * One Durable Object per code holds the host's WebSocket and its guests'.
 *
 *   wss://<lobby>/host        A host opens a game.  First message from the
 *                             lobby: {type:"code", code, ice}.
 *   wss://<lobby>/join/CODE   A guest joins.  First: {type:"joined", id, ice}.
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
		if (request.headers.get("Upgrade") !== "websocket") {
			return new Response("Expected a WebSocket", { status: 426 });
		}
		if (request.headers.get("Origin")) {
			return new Response("Not for browsers", { status: 403 });
		}
		if (url.pathname === "/host") {
			if (!(await allowed(env.HOST_LIMITER, request))) {
				return new Response("Too many games from this address; wait a minute", { status: 429 });
			}
			// A free code: a few tries, in case one is taken.
			for (let tries = 0; tries < 8; ++tries) {
				const code = newCode();
				const lobby = env.LOBBY.get(env.LOBBY.idFromName(code));
				const probe = await lobby.fetch(`https://lobby/taken?code=${code}`);
				if ((await probe.text()) === "free") {
					return lobby.fetch(`https://lobby/host?code=${code}`, request);
				}
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

// One game: its host and guests.
export class Lobby {
	constructor(state, env) {
		this.state = state;
		this.env = env;
		this.code = null;
		this.host = null;          // WebSocket.
		this.guests = new Map();   // id -> WebSocket.
		this.nextGuest = 1;
		this.ice = null;
		this.expires = 0;
	}

	async fetch(request) {
		const url = new URL(request.url);
		if (url.pathname === "/taken") {
			return new Response(this.host && Date.now() < this.expires ? "taken" : "free");
		}
		const pair = new WebSocketPair();
		const [client, server] = Object.values(pair);
		server.accept();
		if (url.pathname === "/host") {
			await this.openGame(server, url.searchParams.get("code"));
		} else {
			this.addGuest(server);
		}
		return new Response(null, { status: 101, webSocket: client });
	}

	async openGame(ws, code) {
		this.code = code;
		this.host = ws;
		this.expires = Date.now() + GAME_HOURS * 3600 * 1000;
		this.ice = await iceServers(this.env);
		json(ws, { type: "code", code, ice: this.ice, protocol: PROTOCOL });
		this.watch(ws, (message) => {
			const data = signalData(message.data);
			if (message.type === "signal" && data && this.guests.has(message.to)) {
				json(this.guests.get(message.to), { type: "signal", data });
			} else if (message.type === "kick" && this.guests.has(message.to)) {
				this.guests.get(message.to).close(1000, "Removed by the host");
			}
		}, () => {
			// The host left: the game and its code are gone.
			this.host = null;
			for (const guest of this.guests.values()) {
				json(guest, { type: "closed" });
				guest.close(1000, "The host left");
			}
			this.guests.clear();
		});
	}

	addGuest(ws) {
		if (!this.host || Date.now() >= this.expires) {
			json(ws, { type: "error", message: "No game with that code" });
			ws.close(1000, "No such game");
			return;
		}
		if (this.guests.size >= MAX_GUESTS) {
			json(ws, { type: "error", message: "The game is full" });
			ws.close(1000, "Full");
			return;
		}
		const id = this.nextGuest++;
		this.guests.set(id, ws);
		// Long enough to connect; then the slot is free again, so codes
		// passed around can't fill a game with idle lobby connections.
		const timer = setTimeout(() => {
			json(ws, { type: "error", message: "Took too long to connect" });
			ws.close(1000, "Too long");
		}, GUEST_SECONDS * 1000);
		json(ws, { type: "joined", id, ice: this.ice, protocol: PROTOCOL });
		json(this.host, { type: "guest", id });
		this.watch(ws, (message) => {
			const data = signalData(message.data);
			if (message.type === "signal" && data && this.host) {
				json(this.host, { type: "signal", from: id, data });
			}
		}, () => {
			clearTimeout(timer);
			this.guests.delete(id);
			if (this.host) {
				json(this.host, { type: "left", id });
			}
		});
	}

	// Messages in, with size and rate limits; 'gone' once it closes.
	watch(ws, onMessage, gone) {
		let windowStart = Date.now();
		let count = 0;
		let closed = false;
		const finish = () => {
			if (!closed) {
				closed = true;
				gone();
			}
		};
		ws.addEventListener("message", (event) => {
			const now = Date.now();
			if (now - windowStart > 60000) {
				windowStart = now;
				count = 0;
			}
			if (++count > MAX_MESSAGES_PER_MINUTE || typeof event.data !== "string"
				|| event.data.length > MAX_MESSAGE_BYTES) {
				json(ws, { type: "error", message: "Too much" });
				ws.close(1008, "Too much");
				finish();
				return;
			}
			let message;
			try {
				message = JSON.parse(event.data);
			} catch {
				return;
			}
			if (message && typeof message === "object") {
				onMessage(message);
			}
		});
		ws.addEventListener("close", finish);
		ws.addEventListener("error", finish);
	}
}
