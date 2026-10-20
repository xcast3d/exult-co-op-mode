#!/bin/bash
# Phase 8: build Friend Mode's WebRTC libraries, static, into
# third_party/build/<system>-<cpu>/ (git-ignored): Mbed TLS (the encryption
# WebRTC requires) and libdatachannel (data channels only, no media).
# Both are pinned submodules:
#
#   git submodule update --init --recursive third_party/libdatachannel third_party/mbedtls
#   tools/friend-mode/build-rtc.sh
#
# Needs CMake.  See docs/friend-mode/lobby.md.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
system="$(uname -s | tr '[:upper:]' '[:lower:]')"
cpu="$(uname -m)"
prefix="${RTC_PREFIX:-$root/third_party/build/$system-$cpu}"
work="$prefix/work"
jobs="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

for dir in libdatachannel mbedtls; do
	if [ ! -f "$root/third_party/$dir/CMakeLists.txt" ]; then
		echo "third_party/$dir is missing: run git submodule update --init --recursive" >&2
		exit 1
	fi
done

echo "== Mbed TLS -> $prefix"
cmake -S "$root/third_party/mbedtls" -B "$work/mbedtls" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$prefix" \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF \
	-DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON \
	-DMBEDTLS_USER_CONFIG_FILE="$root/tools/friend-mode/mbedtls_friend_config.h"
cmake --build "$work/mbedtls" -j "$jobs"
cmake --install "$work/mbedtls"
# Code built against the installed headers sees the same configuration.
config="$prefix/include/mbedtls/mbedtls_config.h"
if ! grep -q "Friend Mode" "$config"; then
	printf '\n/* Friend Mode (build-rtc.sh) */\n' >> "$config"
	cat "$root/tools/friend-mode/mbedtls_friend_config.h" >> "$config"
fi

echo "== libdatachannel -> $prefix"
cmake -S "$root/third_party/libdatachannel" -B "$work/libdatachannel" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$prefix" \
	-DCMAKE_PREFIX_PATH="$prefix" \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DUSE_MBEDTLS=ON -DNO_MEDIA=ON -DNO_EXAMPLES=ON -DNO_TESTS=ON
cmake --build "$work/libdatachannel" --target datachannel-static -j "$jobs"

# The static library and what it needs, by hand (its install rule wants the
# shared one too).
mkdir -p "$prefix/lib" "$prefix/include"
cp "$work/libdatachannel/libdatachannel-static.a" "$prefix/lib/libdatachannel.a" 2>/dev/null \
	|| cp "$work/libdatachannel/libdatachannel.a" "$prefix/lib/libdatachannel.a"
find "$work/libdatachannel" -name 'libjuice*.a' -exec cp {} "$prefix/lib/libjuice.a" \;
find "$work/libdatachannel" -name 'libusrsctp*.a' -exec cp {} "$prefix/lib/libusrsctp.a" \;
cp -R "$root/third_party/libdatachannel/include/rtc" "$prefix/include/"
cp -R "$root/third_party/libdatachannel/deps/json/single_include/nlohmann" "$prefix/include/"

echo "== done: $prefix/lib"
ls -la "$prefix/lib"/*.a
