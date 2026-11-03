#!/bin/bash
# Put Quest of the Companions builds online at downloads.xcast3d.com/u7-qotc/:
#
#   tools/downloads/deploy.sh Exult-U7-QOTC-macOS-arm64-abc1234.dmg Exult-U7-QOTC-Windows-x64-abc1234-setup.exe
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
#   u7-qotc/appcast-macos.xml  the update feed installed Mac builds check
#     (Sparkle): each new macOS .dmg whose app has the updater gets an
#     "-update.zip" of that app, signed and added to the feed.  Its build
#     number must be higher than every build already in the feed.
#   u7-qotc/appcast-windows.xml  the same for Windows (WinSparkle): each
#     new Windows "-setup.exe" is signed and becomes the feed's one update.
#     Its build number and version come from the commit in its name, which
#     must be in this repository.
# Both feeds carry their release notes, plain, from the newest release in
# CHANGELOG.md at the build's commit (release-notes.py).
#
# Needs wrangler logged in to the xcast3d.com account (npx wrangler login),
# Sparkle's tools (tools/friend-mode/get-sparkle.sh) and the private update
# key: ~/.u7-qotc/sparkle-private-key, or SPARKLE_KEY_FILE.  See
# docs/friend-mode/updates.md.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
builds="$here/public/u7-qotc"
max_bytes=$((25 * 1024 * 1024))
sparkle_bin="${SPARKLE_BIN:-$here/../../third_party/build/sparkle/bin}"
key_file="${SPARKLE_KEY_FILE:-$HOME/.u7-qotc/sparkle-private-key}"
feed="$builds/appcast-macos.xml"
win_feed="$builds/appcast-windows.xml"
repo="$here/../.."
# Our feeds and update key, as every build has them.
. "$repo/tools/friend-mode/qotc-release.env"

add_file() {
	if [ "$(stat -f %z "$1")" -gt "$max_bytes" ]; then
		echo "Too big for Cloudflare (25 MiB a file): $1" >&2
		exit 1
	fi
	cp "$1" "$builds/"
	echo "Added $(basename "$1")"
}

# A macOS .dmg's app, zipped for Sparkle and added to the feed.  ditto zips
# the way Finder does; other zippers can leave files in Sparkle.framework
# that Gatekeeper rejects.  An app without our feed and key (configure
# --with-qotc-services) is refused: it would never find another update.
add_mac_update() {
	local dmg="$1" mount app build newest zip work commit feed_url key
	mount="$(mktemp -d)"
	hdiutil attach -quiet -nobrowse -readonly -noautoopen -mountpoint "$mount" "$dmg"
	app="$(find "$mount" -maxdepth 1 -name '*.app' | head -1)"
	feed_url="$(/usr/libexec/PlistBuddy -c 'Print :SUFeedURL' "$app/Contents/Info.plist" 2>/dev/null || true)"
	key="$(/usr/libexec/PlistBuddy -c 'Print :SUPublicEDKey' "$app/Contents/Info.plist" 2>/dev/null || true)"
	if [ -z "$app" ] || [ "$feed_url" != "$QOTC_FEED_MACOS" ] || [ "$key" != "$QOTC_UPDATE_KEY" ]; then
		hdiutil detach -quiet "$mount"
		echo "$(basename "$dmg") doesn't have our updater (feed: ${feed_url:-none}); build it with" >&2
		echo "./configure --with-qotc-services=tools/friend-mode/qotc-release.env" >&2
		exit 1
	fi
	codesign --verify --deep --strict "$app"
	build="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$app/Contents/Info.plist")"
	newest="$(grep -oE '<sparkle:version>[0-9]+' "$feed" 2>/dev/null | grep -oE '[0-9]+' | sort -n | tail -1 || true)"
	if [ -n "$newest" ] && [ "$build" -le "$newest" ]; then
		hdiutil detach -quiet "$mount"
		echo "$(basename "$dmg") is build $build, but the feed already has build $newest." >&2
		exit 1
	fi
	zip="$builds/$(basename "${dmg%.dmg}")-update.zip"
	ditto -c -k --sequesterRsrc --keepParent "$app" "$zip"
	hdiutil detach -quiet "$mount"

	# Only the existing feed and the new zip, so older builds aren't rescanned.
	work="$(mktemp -d)"
	[ -f "$feed" ] && cp "$feed" "$work/"
	cp "$zip" "$work/"
	# Named like the zip, so generate_appcast embeds it in the new item.
	commit="$(build_commit "$(basename "$dmg")")"
	release_notes "$commit" >"$work/$(basename "${zip%.zip}").html"
	(cd "$work" && "$sparkle_bin/generate_appcast" . \
		--ed-key-file "$key_file" \
		--download-url-prefix "https://downloads.xcast3d.com/u7-qotc/" \
		--full-release-notes-url "https://xcast3d.com/u7-quest-of-the-companions/changelog.html" \
		--link "https://xcast3d.com/u7-quest-of-the-companions/" \
		--embed-release-notes \
		--maximum-deltas 0 \
		-o appcast-macos.xml)
	cp "$work/appcast-macos.xml" "$feed"
	rm -rf "$work"
	echo "Added build $build to appcast-macos.xml"
}

# The release notes for a commit, as an HTML fragment.
release_notes() {
	git -C "$repo" show "$1:CHANGELOG.md" | python3 -I "$here/release-notes.py"
}

# The commit in a build's name: Exult-U7-QOTC-<platform>-<commit>[-setup].<ext>
build_commit() {
	local commit
	commit="$(echo "$1" | sed -nE 's/^.*-([0-9a-f]{7,40})(-setup)?\.[a-z]+$/\1/p')"
	if [ -z "$commit" ] || ! git -C "$repo" cat-file -e "$commit^{commit}" 2>/dev/null; then
		echo "$1: no commit of this repository in its name." >&2
		exit 1
	fi
	echo "$commit"
}

# The highest build in a feed, or nothing.
feed_newest() {
	grep -oE 'sparkle:version(>|=")[0-9]+' "$1" 2>/dev/null | grep -oE '[0-9]+$' | sort -n | tail -1 || true
}

# A Windows installer, signed and made the Windows feed's update.  WinSparkle
# runs it with installerArguments: quietly, over the install, and it starts
# the game again (win32/qotc_installer.iss).
add_windows_update() {
	local exe="$1" name commit build version newest signature notes
	name="$(basename "$exe")"
	commit="$(build_commit "$name")"
	build="$(git -C "$repo" rev-list --count "58f31e831..$commit")"
	version="$(git -C "$repo" show "$commit:qotcversion.h" | sed -n 's/^#define QOTC_VERSION "\(.*\)"$/\1/p')"
	newest="$(feed_newest "$win_feed")"
	if [ -n "$newest" ] && [ "$build" -le "$newest" ]; then
		echo "$name is build $build, but the Windows feed already has build $newest." >&2
		exit 1
	fi
	signature="$("$sparkle_bin/sign_update" --ed-key-file "$key_file" -p "$exe")"
	notes="$(release_notes "$commit")"
	cat >"$win_feed" <<-EOF
	<?xml version="1.0" encoding="utf-8"?>
	<rss version="2.0" xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
	  <channel>
	    <title>Quest of the Companions</title>
	    <link>https://xcast3d.com/u7-quest-of-the-companions/</link>
	    <item>
	      <title>Version $version (build $build)</title>
	      <pubDate>$(LC_ALL=C date -u '+%a, %d %b %Y %H:%M:%S +0000')</pubDate>
	      <description><![CDATA[
	$notes
	      ]]></description>
	      <enclosure url="https://downloads.xcast3d.com/u7-qotc/$name"
	        sparkle:version="$build"
	        sparkle:shortVersionString="$version"
	        sparkle:os="windows-x64"
	        sparkle:installerArguments="/SILENT /SP- /NOCANCEL /NORESTART"
	        sparkle:edSignature="$signature"
	        length="$(stat -f %z "$exe")"
	        type="application/octet-stream"/>
	    </item>
	  </channel>
	</rss>
	EOF
	echo "Windows feed: build $build ($version)"
}

mkdir -p "$builds"
for file in "$@"; do
	if [ ! -f "$file" ]; then
		echo "No such file: $file" >&2
		exit 1
	fi
done
# The feed first: a build it refuses isn't copied either.
for file in "$@"; do
	case "$file" in
	*macOS*.dmg) add_mac_update "$file" ;;
	*Windows*-setup.exe) add_windows_update "$file" ;;
	esac
	add_file "$file"
done
if [ -f "$feed" ] && [ "$(stat -f %z "$feed")" -eq 0 ]; then
	echo "appcast-macos.xml is empty" >&2
	exit 1
fi

# Never upload an empty folder: that would take every build offline.
shopt -s nullglob
files=("$builds"/*.dmg "$builds"/*.zip "$builds"/*.exe)
if [ ${#files[@]} -eq 0 ]; then
	echo "No builds in $builds; give the files to deploy." >&2
	exit 1
fi

# Newest first, by the time each was added here.
newest() {
	local matches=("$builds"/$1)    # None with nullglob, so ls lists nothing else.
	[ ${#matches[@]} -gt 0 ] && basename "$(ls -t "${matches[@]}" | head -1)" || true
}
mac="$(newest '*macOS*.dmg')"
# The installer once there is one; before that, the zipped folder.
win="$(newest '*Windows*-setup.exe')"
[ -n "$win" ] || win="$(newest '*Windows*.zip')"

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
	ls -t "$builds" | grep -E '\.(dmg|zip|exe)$' | grep -v -- '-update\.zip$' | while read -r name; do
		size="$(stat -f %z "$builds/$name")"
		echo "<li><a href=\"$name\">$name</a> <small>$((size / 1024 / 1024)) MB</small></li>"
	done
	echo "</ul>"
} >"$builds/index.html"

echo "Deploying ${#files[@]} files (newest: ${mac:-no macOS}, ${win:-no Windows})"
cd "$here"
npx wrangler deploy
