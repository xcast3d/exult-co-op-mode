// A saves folder on disk (Chrome and Edge: the File System Access API;
// docs/friend-mode/web-browser.md, "Save files").  The player chooses a
// folder once, and every saved game is then also written there as an
// ordinary .sav file, as the desktop app keeps them, so the saves outlive
// the browser's storage.  When the folder is allowed again (Chrome asks
// after a restart: one click), saves the browser is missing, or has older,
// are copied back from it.  The game in progress isn't kept there: it is
// in the saves once the player saves.  A third --pre-js (web/build.sh).

const qotc_folder_supported = typeof showDirectoryPicker === 'function';
let qotc_folder = null;           // The chosen folder, while allowed.
let qotc_folder_handle = null;    // The chosen folder, allowed or not.
// The modification time of each save last written to the folder, so a
// save is copied once, after it changes.
const qotc_folder_written = new Map();
const qotc_save_name = /^[A-Za-z0-9_.-]+\.sav$/;

// The folder's handle is kept between visits in the browser's IndexedDB
// (a handle can't be written to a file).  run(store) gives a request or
// nothing; resolves to its result.
function qotc_folder_db(run) {
	return new Promise((resolve, reject) => {
		const open = indexedDB.open('qotc-saves-folder', 1);
		open.onupgradeneeded = () => open.result.createObjectStore('handles');
		open.onerror = () => reject(open.error);
		open.onsuccess = () => {
			const db = open.result;
			const tx = db.transaction('handles', 'readwrite');
			const request = run(tx.objectStore('handles'));
			tx.oncomplete = () => {
				db.close();
				resolve(request ? request.result : undefined);
			};
			tx.onerror = () => {
				db.close();
				reject(tx.error);
			};
		};
	});
}

// Copies the browser's saves into the folder: those changed since last
// written.  Losing the permission (or the folder) stops it until the player
// reconnects.
let qotc_folder_busy = Promise.resolve();
function qotc_folder_push() {
	if (!qotc_folder) {
		return qotc_folder_busy;
	}
	qotc_folder_busy = qotc_folder_busy.then(async () => {
		const dir = qotc_user + '/saves';
		for (const name of qotc_paths_in(dir)) {
			if (!qotc_save_name.test(name)) {
				continue;
			}
			const time = FS.stat(dir + '/' + name).mtime.getTime();
			if (qotc_folder_written.get(name) === time) {
				continue;
			}
			try {
				const file = await qotc_folder.getFileHandle(name, {create: true});
				const out = await file.createWritable();
				await out.write(FS.readFile(dir + '/' + name));
				await out.close();
				qotc_folder_written.set(name, time);
			} catch (err) {
				console.error('Writing to the saves folder failed', err);
				qotc_folder = null;    // Until reconnected.
				break;
			}
		}
		qotc_show_folder_state();
	});
	return qotc_folder_busy;
}

// Copies saves from the folder that the browser is missing, or has an
// older copy of (the browser's storage was deleted, or the save was made
// in another browser using the same folder).  Returns how many.
async function qotc_folder_pull() {
	const dir = qotc_user + '/saves';
	let copied = 0;
	for await (const [name, entry] of qotc_folder.entries()) {
		if (entry.kind !== 'file' || !qotc_save_name.test(name)) {
			continue;
		}
		const file = await entry.getFile();
		if (file.size > 64 * 1024 * 1024) {
			continue;    // No save is anywhere near this.
		}
		const path = dir + '/' + name;
		const ours = FS.analyzePath(path).exists ? FS.stat(path).mtime.getTime() : 0;
		// The folder's copy is written after the browser's, so only one
		// clearly newer than the browser's is a different save.
		if (ours && file.lastModified <= ours + 60000) {
			continue;
		}
		FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
		qotc_folder_written.set(name, FS.stat(path).mtime.getTime());
		copied++;
	}
	return copied;
}

// The folder is allowed: bring missing saves in, then write ours out.
async function qotc_folder_use(handle) {
	qotc_folder = qotc_folder_handle = handle;
	let copied = 0;
	try {
		copied = await qotc_folder_pull();
	} catch (err) {
		console.error('Reading the saves folder failed', err);
		qotc_folder = null;
	}
	if (copied) {
		qotc_saves_message(`Copied ${copied} save${copied === 1 ? '' : 's'} back from your saves folder.`);
		await new Promise(resolve => qotc_save(resolve));
		qotc_show_backup_state();
	}
	await qotc_folder_push();
}

function qotc_show_folder_state() {
	const status = document.getElementById('folder-status');
	const choose = document.getElementById('folder-choose');
	const reconnect = document.getElementById('folder-reconnect');
	const forget = document.getElementById('folder-forget');
	if (!qotc_folder_supported) {
		status.hidden = choose.hidden = reconnect.hidden = forget.hidden = true;
		return;
	}
	status.hidden = false;
	choose.hidden = !!qotc_folder_handle;
	reconnect.hidden = !qotc_folder_handle || !!qotc_folder;
	forget.hidden = !qotc_folder_handle;
	if (qotc_folder) {
		status.textContent = `Every save is also written to your folder "${qotc_folder.name}", so it stays on this computer.`;
	} else if (qotc_folder_handle) {
		status.textContent = `Your saves folder "${qotc_folder_handle.name}" needs a click to allow it again.`;
	} else {
		status.textContent = 'Or keep every save as a file in a folder you choose, as the desktop app does.';
	}
}

// After a game is saved (gamedat.cc): into the browser's storage now, and
// into the folder.  On window, as the game calls it by name, which the
// build's optimizer can't see (it drops functions nothing calls).
window.qotc_saved_game = () => {
	qotc_save();
	qotc_folder_push();
	qotc_show_backup_state();
};

async function qotc_folder_setup() {
	if (qotc_folder_supported) {
		document.getElementById('folder-choose').addEventListener('click', async () => {
			let handle;
			try {
				handle = await showDirectoryPicker({id: 'qotc-saves', mode: 'readwrite', startIn: 'documents'});
			} catch (err) {
				return;    // Cancelled.
			}
			await qotc_folder_db(store => store.put(handle, 'saves'));
			await qotc_folder_use(handle);
		});
		document.getElementById('folder-reconnect').addEventListener('click', async () => {
			if (await qotc_folder_handle.requestPermission({mode: 'readwrite'}) === 'granted') {
				await qotc_folder_use(qotc_folder_handle);
			}
		});
		document.getElementById('folder-forget').addEventListener('click', async () => {
			await qotc_folder_db(store => store.delete('saves'));
			qotc_folder = qotc_folder_handle = null;
			qotc_folder_written.clear();
			qotc_saves_message('Saves are no longer written to the folder. The files already there stay.');
			qotc_show_folder_state();
		});
		try {
			const handle = await qotc_folder_db(store => store.get('saves'));
			if (handle) {
				qotc_folder_handle = handle;
				if (await handle.queryPermission({mode: 'readwrite'}) === 'granted') {
					await qotc_folder_use(handle);
				}
			}
		} catch (err) {
			console.error('The saves folder could not be read', err);
		}
	}
	qotc_show_folder_state();
}
