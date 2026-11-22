#!/bin/sh
# Copies the browser version (web/out, from web/build.sh) into the website's
# dist folder as play/, so it is served at
# xcast3d.com/u7-quest-of-the-companions/play/ once dist/ is uploaded.
#
#   web/copy-to-site.sh [site folder]    (default: ../ultima7-co-op-site)
#
# The site's build (npm run build) clears dist/, so run this after it,
# then upload dist/ as usual (.htaccess included).
set -e
MAIN=$(cd "$(dirname "$0")/.." && pwd)
SITE=${1:-"$MAIN/../ultima7-co-op-site"}
OUT="$MAIN/web/out"
if [ ! -f "$OUT/exult.html" ] || [ ! -f "$OUT/licenses.txt" ]; then
	echo "No complete build in web/out: run web/build.sh first" >&2
	exit 1
fi
if [ ! -f "$SITE/dist/index.html" ]; then
	echo "No built site in $SITE/dist: run npm run build there first" >&2
	exit 1
fi
PLAY="$SITE/dist/play"
rm -rf "$PLAY"
mkdir -p "$PLAY"
for f in exult.js exult.wasm exult.data licenses.txt favicon.svg favicon.ico quest-of-the-companions-title.avif .htaccess; do
	cp "$OUT/$f" "$PLAY/"
done
cp "$OUT/exult.html" "$PLAY/index.html"    # The folder's own address shows it.
ls -la "$PLAY"
