#!/bin/bash
# The Mac release: configure, build, bundle, sign and the disk image, from
# the libraries build-macos-deps.sh made for macOS $QOTC_MACOS_MIN:
#
#   tools/friend-mode/get-sparkle.sh          # once
#   tools/friend-mode/build-macos-deps.sh     # once (and when a library changes)
#   tools/friend-mode/make-macos-release.sh
#
# Gives Exult-U7-QOTC-macOS-<cpu>-<commit>.dmg here, holding
# "Quest of the Companions.app" (docs/friend-mode/releasing.md).  It leaves
# the tree configured for the release (-O2, our updater); plain ./configure
# goes back to a development build.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
min="${QOTC_MACOS_MIN:-12.0}"
cpu="$(uname -m)"
prefix="${QOTC_MACOS_DEPS:-$root/third_party/build/macos${min%%.*}-$cpu}"
app_name="Quest of the Companions.app"
jobs="$(sysctl -n hw.ncpu)"
cd "$root"

if [ ! -f "$prefix/lib/pkgconfig/sdl3.pc" ] || [ ! -f "$prefix/lib/libdatachannel.a" ]; then
	echo "No libraries for macOS $min in $prefix: run tools/friend-mode/build-macos-deps.sh" >&2
	exit 1
fi
if [ ! -d third_party/build/sparkle/Sparkle.framework ]; then
	echo "No Sparkle: run tools/friend-mode/get-sparkle.sh" >&2
	exit 1
fi
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
	echo "Warning: uncommitted changes; a release is built from a commit." >&2
fi

export MACOSX_DEPLOYMENT_TARGET="$min"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
unset PKG_CONFIG_PATH

./configure --with-optimization=normal \
	--with-qotc-services=tools/friend-mode/qotc-release.env \
	--with-friend-rtc="$prefix"
if grep -qE '^[A-Z_]*(CFLAGS|LIBS) = .*(/opt/homebrew|/usr/local)' Makefile; then
	echo "configure picked up Homebrew libraries:" >&2
	grep -E '^[A-Z_]*(CFLAGS|LIBS) = .*(/opt/homebrew|/usr/local)' Makefile >&2
	exit 1
fi
if ! grep -q 'define HAVE_ZIP_SUPPORT 1' config.h; then
	echo "configure found no zlib (zipped saves): run build-macos-deps.sh again" >&2
	exit 1
fi
make clean
make -j "$jobs"
rm -rf Exult.app "$app_name"
make bundle_shared
mv Exult.app "$app_name"
/usr/libexec/PlistBuddy -c "Add :LSMinimumSystemVersion string $min" "$app_name/Contents/Info.plist"

# Each library by itself (dylibbundler's changes break their signatures,
# and --deep doesn't reach Resources/lib), then the app.  Sparkle keeps its
# own signature.
for f in "$app_name"/Contents/Resources/lib/*.dylib; do
	codesign --force --sign - "$f"
done
codesign --force --sign - "$app_name"
codesign --verify --deep --strict "$app_name"

# Nothing from Homebrew, and nothing newer than $min.
bad=0
while IFS= read -r -d '' f; do
	file -b "$f" | grep -q Mach-O || continue
	if otool -L "$f" | grep -qE '/opt/homebrew|/usr/local|third_party/build'; then
		echo "$f still links outside the app:" >&2
		otool -L "$f" | grep -E '/opt/homebrew|/usr/local|third_party/build' >&2
		bad=1
	fi
	for v in $(otool -l "$f" | awk '/LC_BUILD_VERSION/{b=1} b&&/minos/{print $2; b=0}' | sort -u); do
		if [ "$(printf '%s\n%s\n' "$v" "$min" | sort -V | tail -1)" != "$min" ]; then
			echo "$f is for macOS $v" >&2
			bad=1
		fi
	done
done < <(find "$app_name" -type f -print0)
[ "$bad" = 0 ] || exit 1
plist="$app_name/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Print :SUFeedURL" "$plist" >/dev/null

commit="$(git rev-parse --short HEAD)"
# Never the name of a real release's image.
[ -z "$(git status --porcelain --untracked-files=no)" ] || commit="$commit-dirty"
name="Exult-U7-QOTC-macOS-$cpu-$commit"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
ditto "$app_name" "$stage/$app_name"
ln -s /Applications "$stage/Applications"
cp README.md README.macOS COPYING "$stage/"
git rev-parse HEAD >"$stage/COMMIT.txt"
rm -f "$name.dmg"
hdiutil create -quiet -volname "Quest of the Companions" -srcfolder "$stage" -format UDZO "$name.dmg"
echo "$name.dmg: build $(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$plist")," \
	"version $(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$plist")," \
	"for macOS $min and newer ($cpu)"
