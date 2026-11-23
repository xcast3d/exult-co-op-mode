#!/bin/bash
# The Mac release's libraries: for an older macOS than this one, and for
# both kinds of Mac (Apple Silicon and Intel).
#
# Homebrew builds its libraries for the macOS and the processor it runs
# on, so a release made with them only starts there (0.8.10 needed macOS 27
# on Apple Silicon; other Macs showed a stop sign on the app).  This builds
# the libraries the game bundles from their own sources, for macOS
# $QOTC_MACOS_MIN, once for each processor, and joins each pair into one
# universal library in third_party/build/macos<min>-universal/
# (git-ignored), along with the WebRTC libraries (build-rtc.sh):
#
#   tools/friend-mode/build-macos-deps.sh
#
# Then make-macos-release.sh builds the game with them
# (docs/friend-mode/macos-setup.md, "The Mac build").  Sources and
# checksums are Homebrew's (brew info --json=v2 <formula>), downloaded once
# into the folder's src/.  Needs CMake, and Rosetta to build the Intel half
# (CMake runs some of its test programs).
#
# 12.0 is the lowest the bundled Sparkle supports.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
min="${QOTC_MACOS_MIN:-12.0}"
archs="${QOTC_MACOS_ARCHS:-arm64 x86_64}"
prefix="${QOTC_MACOS_DEPS:-$root/third_party/build/macos${min%%.*}-universal}"
src="$prefix/src"
jobs="$(sysctl -n hw.ncpu)"

export MACOSX_DEPLOYMENT_TARGET="$min"
unset PKG_CONFIG_PATH CPATH LIBRARY_PATH

if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
	echo "Building the Intel half needs Rosetta: softwareupdate --install-rosetta" >&2
	exit 1
fi

mkdir -p "$src"

# name  url  sha256
sources=(
	"libogg https://ftp.osuosl.org/pub/xiph/releases/ogg/libogg-1.3.6.tar.gz 83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638"
	"libvorbis https://ftp.osuosl.org/pub/xiph/releases/vorbis/libvorbis-1.3.7.tar.xz b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b"
	"flac https://ftp.osuosl.org/pub/xiph/releases/flac/flac-1.5.0.tar.xz f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920"
	"opus https://ftp.osuosl.org/pub/xiph/releases/opus/opus-1.6.1.tar.gz 6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1"
	"libsndfile https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz 3799ca9924d3125038880367bf1468e53a1b7e3686a934f098b7e1d286cdb80e"
	"libpng https://downloads.sourceforge.net/project/libpng/libpng16/1.6.59/libpng-1.6.59.tar.xz d80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389"
	"SDL3 https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f"
	"fluidsynth https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v2.6.3.tar.gz c51ebe04c8a445f4a7dd5a86e09a27b5c5af547756574edc52fc61c88ebe8427"
	"munt https://github.com/munt/munt/archive/refs/tags/libmt32emu_2_8_6.tar.gz 9d351ccc4671027bdaf62ddc08aafdec8971ecb61835d9324bce949916cb0775"
)

# Downloads and checks one source (once).
fetch() {
	local name="$1" url="$2" sum="$3" file
	file="$src/$name-${url##*/}"
	if [ ! -f "$file" ] || ! echo "$sum  $file" | shasum -a 256 -c - >/dev/null 2>&1; then
		curl -fL --retry 3 -o "$file.part" "$url" >&2
		if ! echo "$sum  $file.part" | shasum -a 256 -c - >/dev/null; then
			echo "$name: checksum doesn't match" >&2
			rm -f "$file.part"
			exit 1
		fi
		mv "$file.part" "$file"
	fi
}

# Unpacks a fetched source into $work/<name>.
unpack() {
	local name="$1" url="$2" dir="$work/$1"
	rm -rf "$dir"
	mkdir -p "$dir"
	tar -xf "$src/$name-${url##*/}" -C "$dir" --strip-components 1
}

# CMake build and install of one source for $arch, shared libraries only.
# Every library names itself by its place in the joined folder, so the two
# halves match and the app's bundling sees one name.
build() {
	local name="$1" dir="$2"
	shift 2
	echo "== $name ($arch) -> $out (macOS $min)"
	cmake -S "$dir" -B "$dir/build" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$out" \
		-DCMAKE_INSTALL_NAME_DIR="$prefix/lib" \
		-DCMAKE_PREFIX_PATH="$out" \
		-DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local" \
		-DCMAKE_FIND_FRAMEWORK=LAST \
		-DCMAKE_OSX_DEPLOYMENT_TARGET="$min" \
		-DCMAKE_OSX_ARCHITECTURES="$arch" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DBUILD_SHARED_LIBS=ON \
		-DBUILD_TESTING=OFF \
		"$@"
	cmake --build "$dir/build" -j "$jobs"
	cmake --install "$dir/build"
}

for line in "${sources[@]}"; do
	read -r name url sum <<<"$line"
	fetch "$name" "$url" "$sum"
done

sdk="$(xcrun --show-sdk-path)"
for arch in $archs; do
	out="$prefix/arch/$arch"
	work="$out/work"
	# Nothing from Homebrew: everything the libraries use comes from here
	# or the macOS SDK.
	export PKG_CONFIG_LIBDIR="$out/lib/pkgconfig"
	rm -rf "$out"
	mkdir -p "$work" "$out/lib/pkgconfig"
	for line in "${sources[@]}"; do
		read -r name url sum <<<"$line"
		unpack "$name" "$url"
	done

	build libogg "$work/libogg"
	build libvorbis "$work/libvorbis"
	build flac "$work/flac" \
		-DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
		-DINSTALL_MANPAGES=OFF -DBUILD_CXXLIBS=OFF -DWITH_OGG=ON
	build opus "$work/opus" -DOPUS_BUILD_PROGRAMS=OFF
	# FluidSynth needs it for compressed (.sf3) SoundFonts.
	build libsndfile "$work/libsndfile" \
		-DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_CPACK=OFF \
		-DINSTALL_MANPAGES=OFF -DENABLE_EXTERNAL_LIBS=ON -DENABLE_MPEG=OFF
	build libpng "$work/libpng" \
		-DPNG_STATIC=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF
	build SDL3 "$work/SDL3" \
		-DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
	# Only what the game uses: SoundFonts through Core Audio.
	build fluidsynth "$work/fluidsynth" \
		-Denable-framework=OFF -Denable-libsndfile=ON \
		-Denable-sdl3=OFF -Denable-portaudio=OFF -Denable-readline=OFF \
		-Denable-jack=OFF -Denable-pulseaudio=OFF -Denable-pipewire=OFF \
		-Denable-dbus=OFF -Denable-libinstpatch=OFF -Denable-openmp=OFF \
		-Denable-ladspa=OFF -Denable-systemd=OFF -Denable-oss=OFF
	build munt "$work/munt" \
		-Dmunt_WITH_MT32EMU_QT=OFF -Dmunt_WITH_MT32EMU_SMF2WAV=OFF \
		-Dlibmt32emu_SHARED=ON

	echo "== WebRTC libraries ($arch) -> $out"
	CMAKE_OSX_ARCHITECTURES="$arch" RTC_PREFIX="$out" "$root/tools/friend-mode/build-rtc.sh"
	rm -rf "$work"
done

# Join: the first half's headers and descriptions, pointed at the joined
# folder, and each library made of both halves.
first="${archs%% *}"
rm -rf "$prefix/include" "$prefix/lib" "$prefix/share"
mkdir -p "$prefix/lib"
cp -R "$prefix/arch/$first/include" "$prefix/"
[ -d "$prefix/arch/$first/share" ] && cp -R "$prefix/arch/$first/share" "$prefix/"
(cd "$prefix/arch/$first/lib" && find . -type d) | while read -r d; do mkdir -p "$prefix/lib/$d"; done
(cd "$prefix/arch/$first/lib" && find . ! -type d) | while read -r f; do
	from="$prefix/arch/$first/lib/$f"
	to="$prefix/lib/$f"
	if [ -L "$from" ]; then
		ln -s "$(readlink "$from")" "$to"
	elif file -b "$from" | grep -q -e Mach-O -e 'ar archive'; then
		halves=()
		for arch in $archs; do
			halves+=("$prefix/arch/$arch/lib/$f")
		done
		lipo -create "${halves[@]}" -output "$to"
	else
		sed "s|$prefix/arch/$first|$prefix|g" "$from" >"$to"
	fi
done

# macOS's own zlib (zipped saves), which Homebrew's pkg-config described.
cat >"$prefix/lib/pkgconfig/zlib.pc" <<EOF
Name: zlib
Description: zlib compression library (macOS's own)
Version: $(sed -n 's/^#define ZLIB_VERSION "\(.*\)"/\1/p' "$sdk/usr/include/zlib.h")
Libs: -lz
Cflags:
EOF

# Every library has both halves, each for $min or lower.
bad=0
for f in "$prefix"/lib/*.dylib "$prefix"/lib/*.a; do
	[ -L "$f" ] && continue
	for arch in $archs; do
		if ! lipo "$f" -verify_arch "$arch"; then
			echo "$f has no $arch half" >&2
			bad=1
		fi
	done
	for v in $(otool -arch all -l "$f" | awk '/LC_BUILD_VERSION/{b=1} b&&/minos/{print $2; b=0}' | sort -u); do
		if [ "$(printf '%s\n%s\n' "$v" "$min" | sort -V | tail -1)" != "$min" ]; then
			echo "$f is for macOS $v" >&2
			bad=1
		fi
	done
done
[ "$bad" = 0 ] || exit 1
echo "Done: $prefix ($archs, all for macOS $min or lower)"
