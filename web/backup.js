// Backups of the player's progress (docs/friend-mode/web-browser.md, "Save
// files").  Browsers may delete a page's storage (Safari after 7 days
// unused, any of them when the player clears browsing data), so Back up
// downloads one .zip with the saves, the game in progress (gamedat) and the
// settings, and Restore puts one back, in this browser or another.  Nothing
// leaves the computer.  Used by pre.js; a second --pre-js (web/build.sh).

// When the last backup was made, kept with what it backs up: if the
// browser deletes the storage, this goes too.
const qotc_backup_note = qotc_user + '/backup.json';
// A backup older than this (with something to back up) is due again.
const qotc_backup_due_days = 7;
// A restored archive bigger than this, or with more files, is refused.
const qotc_restore_limit = 256 * 1024 * 1024;
const qotc_restore_files = 5000;

// --- Zip files: written stored (the saves are zipped already), read stored
// or deflated (an archive re-zipped by Finder or Windows).

const qotc_crc_table = (() => {
	const table = new Uint32Array(256);
	for (let n = 0; n < 256; n++) {
		let c = n;
		for (let k = 0; k < 8; k++) {
			c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
		}
		table[n] = c >>> 0;
	}
	return table;
})();

function qotc_crc32(data) {
	let crc = 0xffffffff;
	for (let i = 0; i < data.length; i++) {
		crc = qotc_crc_table[(crc ^ data[i]) & 0xff] ^ (crc >>> 8);
	}
	return (crc ^ 0xffffffff) >>> 0;
}

// files: [{name, data: Uint8Array}] -> the .zip's bytes.
function qotc_zip(files) {
	const encoder = new TextEncoder();
	const now = new Date();
	const time = (now.getHours() << 11) | (now.getMinutes() << 5) | (now.getSeconds() >> 1);
	const date = ((now.getFullYear() - 1980) << 9) | ((now.getMonth() + 1) << 5) | now.getDate();
	const locals = [];
	const centrals = [];
	let offset = 0;
	for (const file of files) {
		const name = encoder.encode(file.name);
		const crc = qotc_crc32(file.data);
		const local = new DataView(new ArrayBuffer(30));
		local.setUint32(0, 0x04034b50, true);
		local.setUint16(4, 20, true);
		local.setUint16(6, 0x0800, true);    // UTF-8 names.
		local.setUint16(8, 0, true);         // Stored.
		local.setUint16(10, time, true);
		local.setUint16(12, date, true);
		local.setUint32(14, crc, true);
		local.setUint32(18, file.data.length, true);
		local.setUint32(22, file.data.length, true);
		local.setUint16(26, name.length, true);
		locals.push(new Uint8Array(local.buffer), name, file.data);
		const central = new DataView(new ArrayBuffer(46));
		central.setUint32(0, 0x02014b50, true);
		central.setUint16(4, 20, true);
		central.setUint16(6, 20, true);
		central.setUint16(8, 0x0800, true);
		central.setUint16(12, time, true);
		central.setUint16(14, date, true);
		central.setUint32(16, crc, true);
		central.setUint32(20, file.data.length, true);
		central.setUint32(24, file.data.length, true);
		central.setUint16(28, name.length, true);
		central.setUint32(42, offset, true);
		centrals.push(new Uint8Array(central.buffer), name);
		offset += 30 + name.length + file.data.length;
	}
	const central_size = centrals.reduce((sum, part) => sum + part.length, 0);
	const end = new DataView(new ArrayBuffer(22));
	end.setUint32(0, 0x06054b50, true);
	end.setUint16(8, files.length, true);
	end.setUint16(10, files.length, true);
	end.setUint32(12, central_size, true);
	end.setUint32(16, offset, true);
	return new Blob([...locals, ...centrals, new Uint8Array(end.buffer)], {type: 'application/zip'});
}

// The .zip's bytes -> [{name, data}]; throws with a message for the player.
async function qotc_unzip(bytes) {
	const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
	let end = -1;
	for (let i = bytes.length - 22; i >= Math.max(0, bytes.length - 22 - 65535); i--) {
		if (view.getUint32(i, true) === 0x06054b50) {
			end = i;
			break;
		}
	}
	if (end < 0) {
		throw new Error('That file isn\'t a .zip backup.');
	}
	const count = view.getUint16(end + 10, true);
	let at = view.getUint32(end + 16, true);
	if (count > qotc_restore_files) {
		throw new Error('That backup has too many files.');
	}
	const decoder = new TextDecoder();
	const files = [];
	let total = 0;
	for (let n = 0; n < count; n++) {
		if (at + 46 > bytes.length || view.getUint32(at, true) !== 0x02014b50) {
			throw new Error('That backup is damaged.');
		}
		const flags = view.getUint16(at + 8, true);
		const crc = view.getUint32(at + 16, true);
		const method = view.getUint16(at + 10, true);
		const packed = view.getUint32(at + 20, true);
		const size = view.getUint32(at + 24, true);
		const name_length = view.getUint16(at + 28, true);
		const extra_length = view.getUint16(at + 30, true);
		const comment_length = view.getUint16(at + 32, true);
		const local = view.getUint32(at + 42, true);
		const name = decoder.decode(bytes.subarray(at + 46, at + 46 + name_length));
		at += 46 + name_length + extra_length + comment_length;
		if (name.endsWith('/')) {
			continue;    // A folder.
		}
		if (flags & 1) {
			throw new Error('That backup is password protected.');
		}
		total += size;
		if (total > qotc_restore_limit) {
			throw new Error('That backup is too big to be one of ours.');
		}
		if (local + 30 > bytes.length || view.getUint32(local, true) !== 0x04034b50) {
			throw new Error('That backup is damaged.');
		}
		const start = local + 30 + view.getUint16(local + 26, true) + view.getUint16(local + 28, true);
		const raw = bytes.subarray(start, start + packed);
		let data;
		if (method === 0) {
			data = raw.slice();
		} else if (method === 8 && typeof DecompressionStream === 'function') {
			const stream = new Blob([raw]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
			data = new Uint8Array(await new Response(stream).arrayBuffer());
		} else {
			throw new Error('That backup was zipped in a way this browser can\'t read.');
		}
		if (data.length !== size || qotc_crc32(data) !== crc) {
			throw new Error('That backup is damaged.');
		}
		files.push({name, data});
	}
	return files;
}

// --- What there is to back up.

// The paths of every file under 'dir', relative to it.
function qotc_paths_in(dir) {
	const paths = [];
	if (!FS.analyzePath(dir).exists) {
		return paths;
	}
	for (const name of FS.readdir(dir)) {
		if (name === '.' || name === '..') {
			continue;
		}
		if (FS.isDir(FS.stat(dir + '/' + name).mode)) {
			paths.push(...qotc_paths_in(dir + '/' + name).map(path => name + '/' + path));
		} else {
			paths.push(name);
		}
	}
	return paths;
}

// Every file under 'dir' as {name: prefix + its path, data}.
function qotc_files_in(dir, prefix) {
	return qotc_paths_in(dir).map(path => ({name: prefix + path, data: FS.readFile(dir + '/' + path)}));
}

function qotc_empty(dir) {
	for (const name of FS.readdir(dir)) {
		if (name === '.' || name === '..') {
			continue;
		}
		const path = dir + '/' + name;
		if (FS.isDir(FS.stat(path).mode)) {
			qotc_empty(path);
			FS.rmdir(path);
		} else {
			FS.unlink(path);
		}
	}
}

// How many saved games, and whether a game is in progress.
function qotc_progress() {
	const saves = qotc_paths_in(qotc_user + '/saves').filter(path => path.endsWith('.sav')).length;
	const in_progress = qotc_paths_in(qotc_user + '/gamedat').length > 0;
	return {saves, in_progress, any: saves > 0 || in_progress};
}

function qotc_last_backup() {
	try {
		return JSON.parse(FS.readFile(qotc_backup_note, {encoding: 'utf8'})).last || 0;
	} catch (e) {
		return 0;
	}
}

function qotc_backup_due() {
	const last = qotc_last_backup();
	return qotc_progress().any && (!last || Date.now() - last > qotc_backup_due_days * 86400000);
}

function qotc_when(time) {
	if (!time) {
		return 'never';
	}
	const days = Math.floor((Date.now() - time) / 86400000);
	return days <= 0 ? 'today' : days === 1 ? 'yesterday' : `${days} days ago`;
}

function qotc_is_safari() {
	return /^((?!chrome|chromium|android|crios|fxios).)*safari/i.test(navigator.userAgent);
}

// --- Back up and Restore.

// Downloads the backup.  While playing, it holds the game as last written
// (Exult writes the game in progress when it saves, and on quitting).
function qotc_backup() {
	const stamp = new Date();
	const day = stamp.toISOString().slice(0, 10);
	const files = [
		...qotc_files_in(qotc_user + '/saves', 'saves/'),
		...qotc_files_in(qotc_user + '/gamedat', 'gamedat/'),
	];
	if (FS.analyzePath(qotc_config).exists) {
		files.push({name: 'exult.cfg', data: FS.readFile(qotc_config)});
	}
	files.unshift({name: 'README.txt', data: new TextEncoder().encode(
		`Quest of the Companions backup, made ${stamp.toString()}.\n\n` +
		'Your Ultima VII: The Black Gate saves, the game in progress and your settings.\n' +
		'To put them back, open the game\'s page and choose "Restore a backup".\n')});
	const url = URL.createObjectURL(qotc_zip(files));
	const link = document.createElement('a');
	link.href = url;
	link.download = `quest-of-the-companions-backup-${day}.zip`;
	document.body.append(link);
	link.click();
	link.remove();
	setTimeout(() => URL.revokeObjectURL(url), 60000);
	FS.writeFile(qotc_backup_note, JSON.stringify({last: stamp.getTime()}));
	qotc_save();
	qotc_show_backup_state();
}

// Where a file from a backup goes in /user, or null to leave it out: the
// saves and gamedat folders (wherever they sit in the archive, as a re-zip
// may add a folder above them) and exult.cfg.  Names are checked, so a
// crafted archive can't write anywhere else.
function qotc_restore_path(name) {
	const parts = name.split('/').filter(part => part !== '');
	if (parts.some(part => part.startsWith('.') || part === '__MACOSX')) {
		return null;
	}
	const top = parts.findIndex(part => part === 'saves' || part === 'gamedat');
	if (top >= 0) {
		const rest = parts.slice(top + 1);
		if (rest.length === 0 || !rest.every(part => /^[A-Za-z0-9_.-]+$/.test(part))) {
			return null;
		}
		return qotc_user + '/' + parts[top] + '/' + rest.join('/');
	}
	return parts[parts.length - 1] === 'exult.cfg' ? qotc_config : null;
}

async function qotc_restore(file) {
	let files;
	try {
		files = await qotc_unzip(new Uint8Array(await file.arrayBuffer()));
	} catch (err) {
		qotc_saves_message(err.message);
		return;
	}
	const placed = files.map(entry => ({path: qotc_restore_path(entry.name), data: entry.data}))
	                    .filter(entry => entry.path);
	const saves = placed.filter(entry => entry.path.startsWith(qotc_user + '/saves/') && entry.path.endsWith('.sav')).length;
	const game = placed.some(entry => entry.path.startsWith(qotc_user + '/gamedat/'));
	if (!saves && !game) {
		qotc_saves_message('That .zip isn\'t a Quest of the Companions backup.');
		return;
	}
	if (qotc_progress().any && !confirm(
			'Restoring replaces the saves and the game in progress in this browser with the backup\'s. Go on?')) {
		return;
	}
	// The backup is the whole picture: what isn't in it goes.
	qotc_empty(qotc_user + '/saves');
	qotc_empty(qotc_user + '/gamedat');
	for (const entry of placed) {
		FS.mkdirTree(entry.path.slice(0, entry.path.lastIndexOf('/')));
		FS.writeFile(entry.path, entry.data);
	}
	qotc_save(err => {
		const what = [saves ? `${saves} saved game${saves === 1 ? '' : 's'}` : '', game ? 'the game in progress' : '']
			.filter(part => part).join(' and ');
		qotc_saves_message(err ? 'Restored, but the browser wouldn\'t keep it: back up again after playing.'
		                       : `Restored ${what}.`);
		qotc_show_backup_state();
		qotc_folder_push();    // The saves folder too, if there is one.
	});
}

// --- What the player sees.

function qotc_saves_message(text) {
	document.getElementById('saves-message').textContent = text;
}

// The setup page's "Your saves" box and the game page's Back up button.
async function qotc_show_backup_state() {
	const progress = qotc_progress();
	const last = qotc_last_backup();
	const due = qotc_backup_due();
	const status = document.getElementById('saves-status');
	if (progress.any) {
		const what = progress.saves ? `${progress.saves} saved game${progress.saves === 1 ? '' : 's'}` : 'A game in progress';
		status.textContent = `${what} in this browser. Last backup: ${qotc_when(last)}.`;
	} else {
		status.textContent = 'No saves in this browser yet. Played somewhere else? Restore that backup here.';
	}
	document.getElementById('backup').hidden = !progress.any;
	document.getElementById('saves').classList.toggle('due', due);
	const game_button = document.getElementById('backup-game');
	game_button.classList.toggle('due', due);
	game_button.title = due ? `Last backup: ${qotc_when(last)}. Browsers may delete saves; a backup keeps them.`
	                        : `Last backup: ${qotc_when(last)}.`;
	// Whether the browser promised to keep the storage.
	let kept = false;
	try {
		kept = navigator.storage && navigator.storage.persisted && await navigator.storage.persisted();
	} catch (e) {
	}
	document.getElementById('saves-storage').textContent = kept
		? 'This browser has agreed to keep them.'
		: qotc_is_safari()
			? 'Safari deletes a page\'s saves after 7 days without a visit. Back up, or add this page to the Dock (File, Add to Dock) to keep them.'
			: 'This browser may delete them if it runs short of space, or when browsing data is cleared. Back up now and then.';
}

function qotc_backup_setup() {
	document.getElementById('saves').hidden = false;
	document.getElementById('backup').addEventListener('click', qotc_backup);
	const game_button = document.getElementById('backup-game');
	game_button.addEventListener('click', () => {
		qotc_backup();
		game_button.querySelector('.backup-label').textContent = 'Backed up';
		setTimeout(() => {
			game_button.querySelector('.backup-label').textContent = 'Back up';
		}, 3000);
		Module.canvas.focus();
	});
	const restore = document.getElementById('restore');
	restore.addEventListener('change', () => {
		if (restore.files.length) {
			qotc_restore(restore.files[0]);
		}
		restore.value = '';
	});
	qotc_show_backup_state();
	qotc_folder_setup();    // savefolder.js
	// New saves while playing: the game page's button notices.
	setInterval(qotc_show_backup_state, 60000);
}
