// Friend Mode lobby smoke test: a host and two guests against a running
// lobby (by default `npm run dev` on this machine).
//
//   node test/smoke.mjs [ws://127.0.0.1:8787]

const base = process.argv[2] || "ws://127.0.0.1:8787";

let failures = 0;
function check(what, ok) {
	console.log(`${ok ? "ok  " : "FAIL"} ${what}`);
	if (!ok) {
		failures++;
	}
}

// A socket with a queue of the messages it got, and a way to wait for one.
function open(path) {
	return new Promise((resolve, reject) => {
		const ws = new WebSocket(base + path);
		const queue = [];
		const waiters = [];
		ws.addEventListener("message", (event) => {
			const message = JSON.parse(event.data);
			const waiter = waiters.findIndex((w) => w.type === message.type);
			if (waiter >= 0) {
				waiters.splice(waiter, 1)[0].resolve(message);
			} else {
				queue.push(message);
			}
		});
		ws.next = (type, ms = 3000) => {
			const found = queue.findIndex((m) => m.type === type);
			if (found >= 0) {
				return Promise.resolve(queue.splice(found, 1)[0]);
			}
			return new Promise((res, rej) => {
				const w = { type, resolve: res };
				waiters.push(w);
				setTimeout(() => {
					const i = waiters.indexOf(w);
					if (i >= 0) {
						waiters.splice(i, 1);
						rej(new Error(`no "${type}" within ${ms} ms`));
					}
				}, ms);
			});
		};
		ws.closed = new Promise((res) => ws.addEventListener("close", res));
		ws.addEventListener("open", () => resolve(ws));
		ws.addEventListener("error", () => reject(new Error(`cannot open ${base + path}`)));
	});
}

const send = (ws, message) => ws.send(JSON.stringify(message));

try {
	const health = await fetch(base.replace(/^ws/, "http") + "/health");
	check("health page answers", health.ok && (await health.text()).includes("Friend Mode lobby"));

	// The host opens a game and gets a code.
	const host = await open("/host");
	const { code, ice } = await host.next("code");
	check(`host gets a code (${code})`, /^[A-Z]+-\d{4}$/.test(code));
	check("host gets STUN servers", Array.isArray(ice) && JSON.stringify(ice).includes("stun:"));

	// A guest joins with the code, typed in lower case with a space.
	const typed = code.toLowerCase().replace("-", " ");
	const guest = await open("/join/" + encodeURIComponent(typed));
	const joined = await guest.next("joined");
	check("guest joins with the code typed loosely", joined.id === 1);
	const told = await host.next("guest");
	check("host is told of the guest", told.id === joined.id);

	// Offer, answer and a candidate pass through, as the game sends them.
	const offer = { sdp: "v=0 offer from guest", kind: "offer" };
	send(guest, { type: "signal", data: offer });
	const atHost = await host.next("signal");
	check("guest's offer reaches the host", atHost.from === 1 && atHost.data.sdp === offer.sdp);
	send(host, { type: "signal", to: 1, data: { sdp: "v=0 answer", kind: "answer" } });
	const atGuest = await guest.next("signal");
	check("host's answer reaches the guest", atGuest.data.kind === "answer");
	send(host, { type: "signal", to: 1, data: { candidate: "candidate:1 1 udp 1 1.2.3.4 5 typ host", mid: "0" } });
	check("candidates pass too", (await guest.next("signal")).data.candidate.startsWith("candidate:"));

	// Anything else is dropped, and extra fields are stripped.
	send(guest, { type: "signal", data: { note: "not a signal" } });
	send(guest, { type: "signal", data: { sdp: "v=0", kind: "offer", extra: "dropped" } });
	const cleaned = await host.next("signal");
	check("other signals are dropped, extra fields stripped", cleaned.data.sdp === "v=0" && !("extra" in cleaned.data)
		&& !("note" in cleaned.data));

	// A second guest gets its own number; the first never hears its signals.
	const guest2 = await open("/join/" + code);
	const joined2 = await guest2.next("joined");
	check("second guest gets its own id", joined2.id === 2);
	await host.next("guest");
	send(host, { type: "signal", to: 2, data: { sdp: "for two", kind: "answer" } });
	check("signals go to the right guest", (await guest2.next("signal")).data.sdp === "for two");
	let leaked = false;
	try {
		await guest.next("signal", 300);
		leaked = true;
	} catch {
		// Good: nothing for guest 1.
	}
	check("the other guest doesn't get it", !leaked);

	// A wrong code.
	const lost = await open("/join/NOWHERE-0000");
	check("a wrong code is refused", (await lost.next("error")).message.includes("No game"));
	const odd = await open("/join/" + encodeURIComponent("%E0%A4%A"));
	check("a code that can't be one is refused", (await odd.next("error")).message.includes("No game"));
	// A browser's handshake carries an Origin; fetch won't send Upgrade, so
	// by hand.
	const { request } = await import(base.startsWith("wss") ? "node:https" : "node:http");
	const status = await new Promise((resolve) => {
		const req = request(base.replace(/^ws/, "http") + "/host", {
			headers: { Connection: "Upgrade", Upgrade: "websocket", Origin: "https://example.com",
				"Sec-WebSocket-Version": "13", "Sec-WebSocket-Key": "dGhlIHNhbXBsZSBub25jZQ==" },
		});
		req.on("response", (res) => resolve(res.statusCode));
		req.on("upgrade", () => resolve(101));
		req.on("error", () => resolve(0));
		req.end();
	});
	check("browsers are turned away", status === 403);
	const asked = await fetch(base.replace(/^ws/, "http") + "/ip");
	check("the caller's own address is given", asked.ok && (await asked.text()).endsWith("\n"));

	// The host's keep-alive is answered (without waking the game).
	host.send('{"type":"keepalive"}');
	check("the keep-alive is answered", (await host.next("keepalive_ok")).type === "keepalive_ok");

	// At most three codes open from one address: this host and two more,
	// then a fourth is refused until one closes.
	const more = [await open("/host"), await open("/host")];
	await Promise.all(more.map((ws) => ws.next("code")));
	const fourth = await new Promise((resolve) => {
		const ws = new WebSocket(base + "/host");
		ws.addEventListener("open", () => resolve(ws));
		ws.addEventListener("error", () => resolve(null));
	});
	check("a fourth code from one address is refused", fourth === null);
	more[0].close();
	await more[0].closed;
	await new Promise((r) => setTimeout(r, 300));
	const again = await open("/host");
	check("closing one makes room again", typeof (await again.next("code")).code === "string");
	again.close();
	more[1].close();

	// A guest leaving is told to the host.
	guest2.close();
	check("host is told when a guest leaves", (await host.next("left")).id === 2);

	// The host leaving ends the game for its guests, and frees the code.
	host.close();
	await guest.next("closed");
	check("guests are told when the host leaves", true);
	const late = await open("/join/" + code);
	check("the code is gone after the host leaves", (await late.next("error")).message.includes("No game"));
} catch (error) {
	check(`no errors (${error.message})`, false);
}

console.log(failures ? `\n${failures} failed` : "\nall passed");
process.exit(failures ? 1 : 0);
