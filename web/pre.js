// Before Exult starts, the player chooses their own Ultima VII folder (see
// shell.html).  Its static files are copied into the browser's storage
// (IndexedDB, mounted at /user) and stay on this computer; saves and the
// game in progress (gamedat) are kept there too.  Exult's own data comes
// with the page, in /pkg.
Module['arguments'] = ['-c', '/pkg/exult.cfg', '--bg', '--nocrc'];

const qotc_user = '/user';
const qotc_static = qotc_user + '/blackgate/static';
// Files every Black Gate has; Serpent Isle has no endgame.dat.
const qotc_needed = ['u7chunks', 'u7map', 'shapes.vga', 'usecode', 'initgame.dat', 'endgame.dat'];

function qotc_message(text) {
	document.getElementById('message').textContent = text;
}

function qotc_have_game() {
	return qotc_needed.every(name => FS.analyzePath(qotc_static + '/' + name).exists);
}

// Copies /user to IndexedDB.  One at a time; a call while one runs is
// picked up by the next.
let qotc_saving = false;
function qotc_save(done) {
	if (qotc_saving) {
		return done && done();
	}
	qotc_saving = true;
	FS.syncfs(false, err => {
		qotc_saving = false;
		if (err) {
			console.error('Saving to browser storage failed', err);
		}
		done && done(err);
	});
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
		start.addEventListener('click', () => {
			document.getElementById('setup').hidden = true;
			Module.canvas.hidden = false;
			Module.canvas.focus();
			// Saves and the game in progress, every few seconds and when
			// the page is left.
			setInterval(qotc_save, 5000);
			addEventListener('pagehide', () => qotc_save());
			removeRunDependency('qotc-setup');
		});
	});
});

// For testing without a folder dialog.
window.qotc_use_files = qotc_use_files;
