// Before Exult starts, the player chooses their own Ultima VII folder (see
// shell.html).  Its static files are copied into the browser's storage
// (IndexedDB, mounted at /user) and stay on this computer; saves and the
// game in progress (gamedat) are kept there too.  Exult's own data comes
// with the page, in /pkg.
const qotc_user = '/user';
// Exult's settings, kept with the player's files.  The page's own copy
// (/pkg) is the starting point, copied over the first time.
const qotc_config = qotc_user + '/exult.cfg';
Module['arguments'] = ['-c', qotc_config, '--bg', '--nocrc'];
// Set while the page reloads itself back to the title menu.
const qotc_to_menu = 'qotc-to-menu';
const qotc_static = qotc_user + '/blackgate/static';
// Files every Black Gate has; Serpent Isle has no endgame.dat.
const qotc_needed = ['u7chunks', 'u7map', 'shapes.vga', 'usecode', 'initgame.dat', 'endgame.dat'];

function qotc_message(text) {
	document.getElementById('message').textContent = text;
}

function qotc_have_game() {
	return qotc_needed.every(name => FS.analyzePath(qotc_static + '/' + name).exists);
}

// Copies /user to IndexedDB.  One at a time: a call while one runs waits
// for it, then copies again, so nothing written meanwhile is missed.
let qotc_saving = false;
let qotc_waiting = [];
function qotc_save(done) {
	if (qotc_saving) {
		qotc_waiting.push(done);
		return;
	}
	qotc_saving = true;
	FS.syncfs(false, err => {
		qotc_saving = false;
		if (err) {
			console.error('Saving to browser storage failed', err);
		}
		done && done(err);
		if (qotc_waiting.length) {
			const waiting = qotc_waiting;
			qotc_waiting = [];
			qotc_save(err => waiting.forEach(then => then && then(err)));
		}
	});
}

// Main Menu, and Quit, end the game; Exult can't start again in the same
// page, so the page reloads, straight back to the title menu, or (to_menu
// false: Quit on the title menu) to the setup screen.
function qotc_game_ended(to_menu) {
	removeEventListener('beforeunload', qotc_ask_before_leaving);
	try {
		if (to_menu) {
			sessionStorage.setItem(qotc_to_menu, '1');
		}
	} catch (e) {
	}
	qotc_save(() => location.reload());
}

function qotc_ask_before_leaving(event) {
	qotc_save();
	event.preventDefault();
	event.returnValue = '';
}

// files: [{path, file}], path relative to the chosen folder.  Finds the
// Black Gate static folder anywhere in it, then copies that folder.
async function qotc_use_files(files) {
	const folders = new Map();
	for (const {path, file} of files) {
		const parts = path.split('/');
		if (parts.length < 2 || parts[parts.length - 2].toLowerCase() !== 'static') {
			continue;
		}
		const folder = parts.slice(0, -1).join('/');
		if (!folders.has(folder)) {
			folders.set(folder, []);
		}
		folders.get(folder).push(file);
	}
	const names = list => list.map(file => file.name.toLowerCase());
	const game = [...folders.values()].find(
		list => qotc_needed.every(name => names(list).includes(name)));
	if (!game) {
		const si = [...folders.values()].some(list => names(list).includes('sispeech.spc'));
		qotc_message(si ? 'That looks like Serpent Isle. For now only The Black Gate can be played here.'
		                : 'No Black Gate game files there. Choose the folder that has a STATIC folder inside it.');
		return;
	}
	document.getElementById('start').disabled = true;
	for (const name of FS.readdir(qotc_static)) {
		if (name !== '.' && name !== '..') {
			FS.unlink(qotc_static + '/' + name);
		}
	}
	for (let i = 0; i < game.length; i++) {
		qotc_message(`Copying the game files (${i + 1} of ${game.length})...`);
		const data = new Uint8Array(await game[i].arrayBuffer());
		FS.writeFile(qotc_static + '/' + game[i].name.toLowerCase(), data);
	}
	qotc_message('Saving them in this browser...');
	qotc_save(err => {
		qotc_message(err ? 'Copied, but the browser wouldn\'t keep them: you\'ll need to choose the folder again next time.'
		                 : 'The Black Gate is ready.');
		document.getElementById('start').disabled = false;
	});
}

// Added to, not replaced: the data package has its own step there.
Module['preRun'] = [].concat(Module['preRun'] || [], () => {
	addRunDependency('qotc-setup');    // Exult waits for Start.
	FS.mkdir(qotc_user);
	FS.mount(IDBFS, {}, qotc_user);
	FS.syncfs(true, err => {
		if (err) {
			console.error('Reading browser storage failed', err);
		}
		for (const dir of [qotc_static, qotc_user + '/saves', qotc_user + '/gamedat']) {
			FS.mkdirTree(dir);
		}
		if (!FS.analyzePath(qotc_config).exists) {
			FS.writeFile(qotc_config, FS.readFile('/pkg/exult.cfg'));
		}
		navigator.storage && navigator.storage.persist && navigator.storage.persist();
		const start = document.getElementById('start');
		if (qotc_have_game()) {
			qotc_message('The Black Gate is ready. (To use other game files, choose their folder.)');
			start.disabled = false;
		}
		document.getElementById('pick').addEventListener('change', event => {
			qotc_use_files([...event.target.files].map(
				file => ({path: file.webkitRelativePath || file.name, file})));
		});
		const play = () => {
			document.getElementById('setup').hidden = true;
			Module.canvas.hidden = false;
			Module.canvas.focus();
			// Saves and the game in progress, every few seconds and when
			// the page is left.
			setInterval(qotc_save, 5000);
			addEventListener('pagehide', () => qotc_save());
			// Leaving the page (a closed tab, Cmd+W, reload) asks first.
			addEventListener('beforeunload', qotc_ask_before_leaving);
			// SDL keeps function keys, Tab, arrows and Ctrl combinations
			// from the browser, but not Cmd combinations while text is
			// being typed (chat, names).  Cut, copy, paste, select all and
			// undo still work; the browser never gives up Cmd+W, Q, T, N.
			addEventListener('keydown', event => {
				if (event.metaKey && !['a', 'c', 'v', 'x', 'z'].includes(event.key.toLowerCase())) {
					event.preventDefault();
				}
			}, true);
			document.body.style.overflow = 'hidden';    // Space and arrows don't scroll.
			removeRunDependency('qotc-setup');
		};
		start.addEventListener('click', play);
		let to_menu = false;
		try {
			to_menu = sessionStorage.getItem(qotc_to_menu) === '1';
			sessionStorage.removeItem(qotc_to_menu);
		} catch (e) {
		}
		if (to_menu && qotc_have_game()) {
			Module['arguments'].push('--skip-intro');
			play();
		}
	});
});

// For testing without a folder dialog.
window.qotc_use_files = qotc_use_files;
