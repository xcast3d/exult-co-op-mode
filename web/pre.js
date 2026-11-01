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

// Fullscreen: the game's frame fills the screen and the canvas is scaled up
// to fit, whatever window size is chosen in Video Options.  The page's
// button and Alt+Enter in the game (keyactions.cc) both come here.
// Escape stays the game's key: where the browser allows (Chrome, Edge) the
// page keeps it while in fullscreen, and it shows an Exit Fullscreen button
// in the corner instead; holding Escape still leaves.  Safari and Firefox
// always leave fullscreen on Escape.
function qotc_frame() {
	return document.querySelector('.game-frame');
}

async function qotc_toggle_fullscreen() {
	try {
		if (document.fullscreenElement) {
			if (navigator.keyboard && navigator.keyboard.unlock) {
				navigator.keyboard.unlock();
			}
			await document.exitFullscreen();
		} else {
			await qotc_frame().requestFullscreen({navigationUI: 'hide'});
			if (navigator.keyboard && navigator.keyboard.lock) {
				navigator.keyboard.lock(['Escape']).catch(() => {});
			}
		}
	} catch (err) {
		console.error('Fullscreen failed', err);
	}
	Module.canvas.focus();
}

function qotc_fit_fullscreen() {
	const canvas = Module.canvas;
	const frame = qotc_frame();
	if (document.fullscreenElement === frame && canvas.offsetWidth && canvas.offsetHeight) {
		const scale = Math.min(frame.clientWidth / canvas.offsetWidth, frame.clientHeight / canvas.offsetHeight);
		canvas.style.transform = `scale(${scale})`;
	} else {
		canvas.style.transform = '';
	}
}

let qotc_exit_shown_until = 0;
function qotc_show_exit_button(ms) {
	const button = document.getElementById('exit-fullscreen');
	button.classList.add('shown');
	qotc_exit_shown_until = Math.max(qotc_exit_shown_until, performance.now() + ms);
	setTimeout(() => {
		if (performance.now() >= qotc_exit_shown_until - 10) {
			button.classList.remove('shown');
		}
	}, ms);
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

// Just before Exult starts (the data package is in by then; on a first
// visit, storage is read before it is): the settings start as the page's.
Module['onRuntimeInitialized'] = () => {
	if (!FS.analyzePath(qotc_config).exists) {
		FS.writeFile(qotc_config, FS.readFile('/pkg/exult.cfg'));
	}
};

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
		// Ask the browser to keep the storage, then show what there is to
		// keep and whether it agreed (backup.js).
		Promise.resolve(navigator.storage && navigator.storage.persist && navigator.storage.persist())
			.catch(() => {}).then(qotc_backup_setup);
		const start = document.getElementById('start');
		if (qotc_have_game()) {
			qotc_message('The Black Gate is ready. (To use other game files, choose their folder.)');
			start.disabled = false;
		}
		document.getElementById('pick').addEventListener('change', event => {
			qotc_use_files([...event.target.files].map(
				file => ({path: file.webkitRelativePath || file.name, file})));
		});
		const fullscreen = document.getElementById('fullscreen');
		fullscreen.addEventListener('click', qotc_toggle_fullscreen);
		document.getElementById('exit-fullscreen').addEventListener('click', qotc_toggle_fullscreen);
		document.addEventListener('fullscreenchange', () => {
			const active = document.fullscreenElement === qotc_frame();
			fullscreen.setAttribute('aria-pressed', active);
			fullscreen.querySelector('.fullscreen-label').textContent = active ? 'Exit fullscreen' : 'Fullscreen';
			qotc_fit_fullscreen();
			if (active) {
				qotc_show_exit_button(3000);
			} else {
				Module.canvas.focus();
			}
		});
		addEventListener('resize', qotc_fit_fullscreen);
		new ResizeObserver(qotc_fit_fullscreen).observe(Module.canvas);
		// Switching away from fullscreen (Cmd+Tab, another Space) once left a
		// black fullscreen Space that held the keyboard, and only a new
		// desktop got out of it.  So the Escape lock is let go whenever the
		// window loses focus (taken back on return), and fullscreen ends
		// when the page is hidden.
		addEventListener('blur', () => {
			if (navigator.keyboard && navigator.keyboard.unlock) {
				navigator.keyboard.unlock();
			}
		});
		addEventListener('focus', () => {
			if (document.fullscreenElement && navigator.keyboard && navigator.keyboard.lock) {
				navigator.keyboard.lock(['Escape']).catch(() => {});
			}
		});
		document.addEventListener('visibilitychange', () => {
			if (document.hidden && document.fullscreenElement) {
				if (navigator.keyboard && navigator.keyboard.unlock) {
					navigator.keyboard.unlock();
				}
				document.exitFullscreen().catch(() => {});
			}
		});
		// In fullscreen, Escape (still the game's) and the mouse in the top
		// right corner show the Exit Fullscreen button for a moment.
		addEventListener('keydown', event => {
			if (event.key === 'Escape' && document.fullscreenElement) {
				qotc_show_exit_button(3000);
			}
		}, true);
		addEventListener('mousemove', event => {
			if (document.fullscreenElement && event.clientY < 80 && event.clientX > innerWidth - 240) {
				qotc_show_exit_button(1500);
			}
		});
		const play = () => {
			document.getElementById('setup').hidden = true;
			document.getElementById('game-shell').hidden = false;
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
