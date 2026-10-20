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
 * 'data' is whatever the game sends (SDP and candidates); the lobby only
 * passes it on.  'ice' is the STUN (and, when configured, TURN) servers to
 * use.  See docs/friend-mode/lobby.md.
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

function newCode() {
	const word = WORDS[Math.floor(Math.random() * WORDS.length)];
	const number = 10 + Math.floor(Math.random() * 90);
	return `${word}-${number}`;
}

function normalCode(code) {
	return decodeURIComponent(code || "").trim().toUpperCase().replace(/\s+/g, "-");
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
		if (url.pathname === "/host") {
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
			const code = normalCode(join[1]);
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
			if (message.type === "signal" && this.guests.has(message.to)) {
				json(this.guests.get(message.to), { type: "signal", data: message.data });
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
		json(ws, { type: "joined", id, ice: this.ice, protocol: PROTOCOL });
		json(this.host, { type: "guest", id });
		this.watch(ws, (message) => {
			if (message.type === "signal" && this.host) {
				json(this.host, { type: "signal", from: id, data: message.data });
			}
		}, () => {
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
