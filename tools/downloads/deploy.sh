#!/bin/bash
# Put Quest of the Companions builds online at downloads.xcast3d.com/u7-qotc/:
#
#   tools/downloads/deploy.sh Exult-U7-QOTC-macOS-arm64-abc1234.dmg Exult-U7-QOTC-Windows-x64-abc1234.zip
#
# The builds are copied into public/u7-qotc/ (git-ignored), which keeps
# every build ever deployed; the whole folder is uploaded each time, so a
# file removed there disappears from the site.  With no files given, the
# folder is deployed as it is.
#
# Besides the builds it makes:
#   u7-qotc/              a page listing every build, newest first
#   u7-qotc/latest/macos  redirects to the newest macOS build
#   u7-qotc/latest/windows  ... and Windows build (for the website's buttons)
#
# Needs wrangler logged in to the xcast3d.com account (npx wrangler login).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
builds="$here/public/u7-qotc"
max_bytes=$((25 * 1024 * 1024))

mkdir -p "$builds"
for file in "$@"; do
	if [ ! -f "$file" ]; then
		echo "No such file: $file" >&2
		exit 1
	fi
	if [ "$(stat -f %z "$file")" -gt "$max_bytes" ]; then
		echo "Too big for Cloudflare (25 MiB a file): $file" >&2
		exit 1
	fi
	cp "$file" "$builds/"
	echo "Added $(basename "$file")"
done

# Never upload an empty folder: that would take every build offline.
shopt -s nullglob
files=("$builds"/*.dmg "$builds"/*.zip)
if [ ${#files[@]} -eq 0 ]; then
	echo "No builds in $builds; give the files to deploy." >&2
	exit 1
fi

# Newest first, by the time each was added here.
newest() {
	ls -t "$builds"/$1 2>/dev/null | head -1 | xargs basename 2>/dev/null || true
}
mac="$(newest '*macOS*')"
win="$(newest '*Windows*')"

{
	echo "/ https://xcast3d.com/u7-quest-of-the-companions/ 302"
	[ -n "$mac" ] && echo "/u7-qotc/latest/macos /u7-qotc/$mac 302"
	[ -n "$win" ] && echo "/u7-qotc/latest/windows /u7-qotc/$win 302"
} >"$here/public/_redirects"

{
	cat <<-'EOF'
	<!doctype html>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>Quest of the Companions builds</title>
	<style>body{font:16px/1.5 system-ui,sans-serif;max-width:40rem;margin:2rem auto;padding:0 1rem}li{margin:.3rem 0}small{color:#777}</style>
	<h1>Quest of the Companions builds</h1>
	<p><a href="https://xcast3d.com/u7-quest-of-the-companions/">About the project</a></p>
	<ul>
	EOF
	ls -t "$builds" | grep -E '\.(dmg|zip)$' | while read -r name; do
		size="$(stat -f %z "$builds/$name")"
		echo "<li><a href=\"$name\">$name</a> <small>$((size / 1024 / 1024)) MB</small></li>"
	done
	echo "</ul>"
} >"$builds/index.html"

echo "Deploying ${#files[@]} builds (newest: ${mac:-no macOS}, ${win:-no Windows})"
cd "$here"
npx wrangler deploy
