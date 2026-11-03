#!/bin/sh
# Quest of the Companions (single player for now) built with Emscripten, to
# run in a browser.  Needs: brew install emscripten, and this checkout built
# natively first (the data files come from it).
#
#   web/build.sh     then   web/serve.sh
#
# The native build lives in the source folder, so the web build works in a
# copy of the sources, web/build (git-ignored); only changed files are copied
# again.  Steps: autoreconf, emconfigure, emmake (the data/ tools fail: they
# are run as WebAssembly, and the data files come from this checkout
# instead), then a final link to web/out/exult.html with Asyncify, the page
# (shell.html, pre.js) and Exult's own data.  Players pick their own game
# files on the page.
set -e
MAIN=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$MAIN/web/build"
mkdir -p "$BUILD"
cd "$MAIN"
git ls-files -co --exclude-standard | grep -v '^web/' > "$BUILD/.files"
rsync -a --checksum --files-from="$BUILD/.files" "$MAIN/" "$BUILD/"
# Headers the data build would generate.
find data -name '*_flx.h' | rsync -a --checksum --files-from=- "$MAIN/" "$BUILD/"
cd "$BUILD"

if [ ! -f configure ]; then
	autoreconf -i
fi
if [ ! -f Makefile ]; then
	mkdir -p empty-pc
	PKG_CONFIG_LIBDIR="$BUILD/empty-pc" \
	SDL_CFLAGS="-sUSE_SDL=3" SDL_LIBS="-sUSE_SDL=3" \
	OGG_CFLAGS="-sUSE_OGG=1 -sUSE_VORBIS=1" OGG_LIBS="-sUSE_OGG=1 -sUSE_VORBIS=1" \
	ZLIB_CFLAGS="-sUSE_ZLIB=1" ZLIB_LIBS="-sUSE_ZLIB=1" \
	emconfigure ./configure --host=wasm32-unknown-emscripten --disable-tools --disable-data \
		--with-friend-rtc=no --disable-alsa --disable-fluidsynth --disable-mt32emu \
		--disable-shp-thumbnailer --disable-aseprite-plugin --disable-gimp-plugin \
		--with-optimization=normal
fi

rm -f exult    # So make links it again, and the log has the link line.
emmake make -k -j8 CXXFLAGS="-O2 -fexceptions" CFLAGS="-O2 -fexceptions" LDFLAGS="-fexceptions" \
	> "$MAIN/web/make.log" 2>&1 || true
grep -q 'libtool: link: .*-o exult ' "$MAIN/web/make.log" || {
	echo "Exult didn't link; see web/make.log" >&2
	exit 1
}

# The package: Exult's own data files.  No game files.
PKG="$MAIN/web/pkg"
OUT="$MAIN/web/out"
rm -rf "$PKG"
mkdir -p "$PKG/data/friend" "$OUT"
cp "$MAIN"/data/*.flx "$MAIN"/data/*.txt "$PKG/data/"
cp -R "$MAIN/data/friend/skills" "$MAIN/data/friend/title.shp" "$MAIN/data/friend/CREDITS.txt" "$PKG/data/friend/"
cp "$MAIN/web/exult-web.cfg" "$PKG/exult.cfg"

# The final link, as libtool did it, to a web page.
LINK=$(grep 'libtool: link: .*-o exult ' "$MAIN/web/make.log" | sed "s/^libtool: link: //; s| -o exult | -o $OUT/exult.html |")
sh -c "$LINK -fexceptions -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sALLOW_MEMORY_GROWTH=1 \
	-sINITIAL_MEMORY=268435456 -sSTACK_SIZE=8388608 -sFORCE_FILESYSTEM=1 -lidbfs.js \
	--shell-file $MAIN/web/shell.html --pre-js $MAIN/web/pre.js --pre-js $MAIN/web/backup.js --pre-js $MAIN/web/savefolder.js --preload-file $PKG@/pkg"
cp "$MAIN/web/quest-of-the-companions-title.avif" "$OUT/"
# Exult's own icon, the ankh, as the page's (desktop/, win32/).
cp "$MAIN/desktop/info.exult.exult.svg" "$OUT/favicon.svg"
cp "$MAIN/win32/exult.ico" "$OUT/favicon.ico"
ls -la "$OUT"
