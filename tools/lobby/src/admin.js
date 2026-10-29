/*
 * The lobby's admin page: how many games were opened and how many guests
 * joined them, per day.  Nothing else is counted; no addresses or codes are
 * kept (see the Stats object in index.js).
 *
 * It lives on its own hostname (ADMIN_HOST in wrangler.toml), which
 * Cloudflare Access gates as a whole, like Kitty Launcher's and Squishify's
 * admin pages.  The page itself holds no data; its one call, /admin/stats,
 * answers only with a Cloudflare Access login, checked here from the JWT
 * Access forwards (anyone can set a header, but only Access can sign it).
 * Without CF_ACCESS_TEAM_DOMAIN and CF_ACCESS_AUD it answers 503, so a
 * missing setting can never leave the numbers open.
 *
 * For `npm run dev:admin` only: ADMIN_LOCAL_DEV=1 serves the page on
 * localhost without Access.  It is never set in wrangler.toml, and the
 * deployed Worker is only ever reached by its own hostnames.
 */

const LOCAL_HOSTS = ["localhost", "127.0.0.1"];

function localDev(url, env) {
	return env.ADMIN_LOCAL_DEV === "1" && LOCAL_HOSTS.includes(url.hostname);
}

// True when this request is for the admin page rather than the lobby.
export function isAdmin(url, env) {
	if (localDev(url, env)) {
		return url.pathname === "/admin" || url.pathname.startsWith("/admin/");
	}
	return !!env.ADMIN_HOST && url.hostname === env.ADMIN_HOST;
}

export async function admin(request, env) {
	const url = new URL(request.url);
	const plain = (text, status) => new Response(text, {
		status,
		headers: { "Content-Type": "text/plain", "Cache-Control": "no-store" },
	});
	if (request.method !== "GET") {
		return plain("Not allowed", 405);
	}
	if (url.pathname === "/") {
		return Response.redirect(`${url.origin}/admin`, 302);
	}
	if (url.pathname === "/admin") {
		return new Response(ADMIN_PAGE, {
			headers: {
				"Content-Type": "text/html; charset=utf-8",
				"Cache-Control": "no-store",
				"X-Frame-Options": "DENY",
				"Content-Security-Policy": "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'",
			},
		});
	}
	if (url.pathname === "/admin/stats") {
		if (!localDev(url, env)) {
			if (!env.CF_ACCESS_TEAM_DOMAIN || !env.CF_ACCESS_AUD) {
				return plain("Admin isn't configured", 503);
			}
			const jwt = request.headers.get("Cf-Access-Jwt-Assertion");
			if (!jwt || !(await verifyAccessJwt(jwt, env))) {
				return plain("Unauthorized", 401);
			}
		}
		const stats = env.STATS.get(env.STATS.idFromName("stats"));
		const days = await (await stats.fetch("https://stats/days")).json();
		return new Response(JSON.stringify({ days }), {
			headers: { "Content-Type": "application/json", "Cache-Control": "no-store" },
		});
	}
	return plain("Not found", 404);
}

// A Cloudflare Access JWT: RS256 signature against Access's own keys, plus
// its expiry, audience (this Access application) and issuer (the team).
const ACCESS_KEYS_MS = 60 * 60 * 1000;    // Access rotates its keys rarely.
let accessKeys = null;    // {keys, fetched}, while the isolate is warm.

async function fetchAccessKeys(env) {
	if (accessKeys && Date.now() - accessKeys.fetched < ACCESS_KEYS_MS) {
		return accessKeys.keys;
	}
	try {
		const response = await fetch(`https://${env.CF_ACCESS_TEAM_DOMAIN}.cloudflareaccess.com/cdn-cgi/access/certs`);
		if (response.ok) {
			const { keys } = await response.json();
			accessKeys = { keys, fetched: Date.now() };
			return keys;
		}
	} catch {
		// The last keys we had, if any.
	}
	return accessKeys ? accessKeys.keys : null;
}

function base64UrlBytes(text) {
	const base64 = text.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(text.length / 4) * 4, "=");
	return Uint8Array.from(atob(base64), (c) => c.charCodeAt(0));
}

async function verifyAccessJwt(jwt, env) {
	const parts = jwt.split(".");
	if (parts.length !== 3) {
		return false;
	}
	const [headerPart, payloadPart, signaturePart] = parts;
	let header, payload;
	try {
		header = JSON.parse(new TextDecoder().decode(base64UrlBytes(headerPart)));
		payload = JSON.parse(new TextDecoder().decode(base64UrlBytes(payloadPart)));
	} catch {
		return false;
	}
	if (header.alg !== "RS256") {
		return false;
	}
	if (!payload.exp || payload.exp < Math.floor(Date.now() / 1000)) {
		return false;
	}
	const audience = Array.isArray(payload.aud) ? payload.aud : [payload.aud];
	if (!audience.includes(env.CF_ACCESS_AUD)) {
		return false;
	}
	if (payload.iss !== `https://${env.CF_ACCESS_TEAM_DOMAIN}.cloudflareaccess.com`) {
		return false;
	}
	const keys = await fetchAccessKeys(env);
	const jwk = keys && keys.find((k) => k.kid === header.kid);
	if (!jwk) {
		return false;
	}
	try {
		const key = await crypto.subtle.importKey("jwk", jwk, { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["verify"]);
		return await crypto.subtle.verify("RSASSA-PKCS1-v1_5", key, base64UrlBytes(signaturePart),
			new TextEncoder().encode(`${headerPart}.${payloadPart}`));
	} catch {
		return false;
	}
}

const ADMIN_PAGE = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex">
<title>Lobby Stats</title>
<style>
	:root { --bg:#f6f3ec; --card:#fff; --ink:#1f1a12; --muted:#6b6255; --line:#e3dccd; --accent:#8a5a14;
		font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif; color:var(--ink); background:var(--bg); }
	@media (prefers-color-scheme: dark) { :root { --bg:#14120e; --card:#1e1b16; --ink:#ece6da; --muted:#a39a8a; --line:#332e25; --accent:#e0a84a; } }
	body { margin:0; background:var(--bg); }
	main { max-width:900px; margin:0 auto; padding:24px 16px 64px; }
	header { display:flex; align-items:center; gap:12px; margin-bottom:24px; flex-wrap:wrap; }
	header h1 { font-size:20px; margin:0; flex:1; }
	button { font:inherit; border:1px solid var(--line); background:var(--card); color:var(--ink); border-radius:8px; padding:8px 14px; cursor:pointer; }
	.tiles { display:grid; grid-template-columns:repeat(auto-fit,minmax(170px,1fr)); gap:12px; margin-bottom:20px; }
	.tile { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:16px; }
	.tile .label { color:var(--muted); font-size:13px; }
	.tile .value { font-size:26px; font-weight:700; margin-top:4px; font-variant-numeric:tabular-nums; }
	.tile .sub { color:var(--muted); font-size:12px; margin-top:2px; }
	.card { background:var(--card); border:1px solid var(--line); border-radius:12px; overflow:auto; }
	table { width:100%; border-collapse:collapse; font-size:14px; }
	th, td { text-align:right; padding:10px 14px; border-bottom:1px solid var(--line); white-space:nowrap; font-variant-numeric:tabular-nums; }
	th:first-child, td:first-child { text-align:left; }
	th { color:var(--muted); font-weight:600; font-size:12px; text-transform:uppercase; letter-spacing:.05em; }
	tr:last-child td { border-bottom:0; }
	.note { color:var(--muted); font-size:13px; margin-top:12px; }
	.empty, .error { padding:32px; text-align:center; color:var(--muted); }
	.error { color:var(--accent); }
</style>
</head>
<body>
<main>
	<header>
		<h1>Friend Mode lobby · usage</h1>
		<button id="refresh">Refresh</button>
		<button id="signout">Sign out</button>
	</header>
	<div class="tiles">
		<div class="tile"><div class="label">Games opened</div><div class="value" id="games">—</div><div class="sub" id="since"></div></div>
		<div class="tile"><div class="label">Games guests joined</div><div class="value" id="used">—</div><div class="sub" id="usedShare"></div></div>
		<div class="tile"><div class="label">Guest joins</div><div class="value" id="guests">—</div></div>
		<div class="tile"><div class="label">Today (UTC)</div><div class="value" id="today">—</div><div class="sub" id="todaySub"></div></div>
	</div>
	<div class="card">
		<table>
			<thead><tr><th>Day (UTC)</th><th>Games opened</th><th>Games guests joined</th><th>Guest joins</th></tr></thead>
			<tbody id="rows"></tbody>
		</table>
		<div id="empty" class="empty" hidden>No games yet.</div>
		<div id="error" class="error" hidden></div>
	</div>
	<p class="note">A game is a code handed out. Guest joins count each time a guest got into a game with its code, so one person reconnecting counts again. No addresses or codes are kept.</p>
</main>
<script>
	const $ = (id) => document.getElementById(id);
	const number = (n) => n.toLocaleString();
	function cell(text) { const td = document.createElement("td"); td.textContent = text; return td; }

	async function load() {
		$("error").hidden = true;
		let days;
		try {
			const response = await fetch("/admin/stats", { cache: "no-store" });
			if (!response.ok) {
				throw new Error(response.status === 503 ? "Admin isn't configured on the Worker." : "Couldn't load (" + response.status + ").");
			}
			days = (await response.json()).days;
		} catch (error) {
			$("error").textContent = error.message;
			$("error").hidden = false;
			return;
		}
		const total = { games: 0, used: 0, guests: 0 };
		for (const d of days) { total.games += d.games; total.used += d.used; total.guests += d.guests; }
		$("games").textContent = number(total.games);
		$("since").textContent = days.length ? "since " + days[days.length - 1].day : "";
		$("used").textContent = number(total.used);
		$("usedShare").textContent = total.games ? Math.round(100 * total.used / total.games) + "% of games" : "";
		$("guests").textContent = number(total.guests);
		const todayKey = new Date().toISOString().slice(0, 10);
		const today = days.find((d) => d.day === todayKey) || { games: 0, used: 0, guests: 0 };
		$("today").textContent = number(today.games);
		$("todaySub").textContent = number(today.guests) + " guest join" + (today.guests === 1 ? "" : "s");
		const rows = $("rows");
		rows.replaceChildren();
		for (const d of days) {
			const tr = document.createElement("tr");
			tr.append(cell(d.day), cell(number(d.games)), cell(number(d.used)), cell(number(d.guests)));
			rows.append(tr);
		}
		$("empty").hidden = days.length > 0;
	}

	$("refresh").addEventListener("click", load);
	// Ends the Cloudflare Access session (its own logout clears the cookie).
	$("signout").addEventListener("click", () => { location.href = "/cdn-cgi/access/logout"; });
	load();
</script>
</body>
</html>
`;
