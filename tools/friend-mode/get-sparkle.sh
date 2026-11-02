#!/bin/bash
# Fetch Sparkle (the Mac updater) into third_party/build/sparkle/ (git-ignored):
# Sparkle.framework, which ./configure finds there and the bundle copies into
# Exult.app, and bin/ with the release tools (generate_keys, sign_update,
# generate_appcast) that tools/downloads/deploy.sh uses.
#
#   tools/friend-mode/get-sparkle.sh
#
# Pinned to one release and checked against its SHA-256 (from the release
# page on github.com/sparkle-project/Sparkle).  See docs/friend-mode/updates.md.
set -euo pipefail

version="2.10.1"
sha256="1a856d2a9edb8e628de88121d7be04cf0e2167f8641650b78fca00b14244cf27"

root="$(cd "$(dirname "$0")/../.." && pwd)"
dest="$root/third_party/build/sparkle"

if [ -f "$dest/VERSION" ] && [ "$(cat "$dest/VERSION")" = "$version" ]; then
	echo "Sparkle $version is already in $dest"
	exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
archive="$work/Sparkle-$version.tar.xz"
curl -fsSL -o "$archive" "https://github.com/sparkle-project/Sparkle/releases/download/$version/Sparkle-$version.tar.xz"
if [ "$(shasum -a 256 "$archive" | cut -d' ' -f1)" != "$sha256" ]; then
	echo "Sparkle-$version.tar.xz doesn't match its SHA-256; not using it." >&2
	exit 1
fi

mkdir -p "$work/unpacked"
tar -xJf "$archive" -C "$work/unpacked"
rm -rf "$dest"
mkdir -p "$dest"
# ditto keeps the framework's symlinks and signature intact.
ditto "$work/unpacked/Sparkle.framework" "$dest/Sparkle.framework"
ditto "$work/unpacked/bin" "$dest/bin"
echo "$version" >"$dest/VERSION"
echo "Sparkle $version -> $dest"
