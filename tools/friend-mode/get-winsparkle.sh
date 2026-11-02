#!/bin/bash
# Fetch WinSparkle (the Windows updater) into third_party/build/winsparkle/
# (git-ignored): include/winsparkle.h and the 64-bit WinSparkle.dll, which
# Makefile.mingw finds there, links and installs next to Exult.exe.
#
#   tools/friend-mode/get-winsparkle.sh
#
# Pinned to one release and checked against its SHA-256 (from the release
# page on github.com/vslavik/winsparkle).  Runs in MSYS2 (the Windows
# workflow) or on the Mac.  See docs/friend-mode/updates.md.
set -euo pipefail

version="0.9.4"
sha256="6037df37fc263bd1650a1c4949681a9d40ffe991d01f35892a406cb5d103c976"

root="$(cd "$(dirname "$0")/../.." && pwd)"
dest="$root/third_party/build/winsparkle"

if [ -f "$dest/VERSION" ] && [ "$(cat "$dest/VERSION")" = "$version" ]; then
	echo "WinSparkle $version is already in $dest"
	exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
archive="$work/WinSparkle-$version.zip"
curl -fsSL -o "$archive" "https://github.com/vslavik/winsparkle/releases/download/v$version/WinSparkle-$version.zip"
if command -v sha256sum >/dev/null; then
	actual="$(sha256sum "$archive" | cut -d' ' -f1)"
else
	actual="$(shasum -a 256 "$archive" | cut -d' ' -f1)"
fi
if [ "$actual" != "$sha256" ]; then
	echo "WinSparkle-$version.zip doesn't match its SHA-256; not using it." >&2
	exit 1
fi

mkdir -p "$work/unpacked"
unzip -q "$archive" -d "$work/unpacked" -x '__MACOSX/*'
from="$work/unpacked/WinSparkle-$version"
rm -rf "$dest"
mkdir -p "$dest/include" "$dest/x64"
cp "$from/include/winsparkle.h" "$from/include/winsparkle-version.h" "$dest/include/"
cp "$from/x64/Release/WinSparkle.dll" "$dest/x64/"
cp "$from/COPYING" "$dest/COPYING.txt"
echo "$version" >"$dest/VERSION"
echo "WinSparkle $version -> $dest"
