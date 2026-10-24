#!/bin/bash
# Build the Phase 8 session check (rtc_session_test.cc) against the static
# libraries tools/friend-mode/build-rtc.sh makes.  Output: /tmp/rtc_session_test
# (or the path given).
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
prefix="${RTC_PREFIX:-$root/third_party/build/$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)}"
out="${1:-/tmp/rtc_session_test}"
lib="$prefix/lib"
c++ -std=gnu++17 -O1 -pthread -DFRIEND_RTC -DRTC_STATIC -I"$root" -I"$root/headers" -I"$root/files" -isystem "$prefix/include" \
	"$root/tools/friend-mode/rtc-session-test/rtc_session_test.cc" \
	"$root/friendnet.cc" "$root/friendtcp.cc" "$root/friendrtc.cc" "$root/files/sha1/sha1.cpp" \
	"$lib/libdatachannel.a" "$lib/libjuice.a" "$lib/libusrsctp.a" "$lib/libmbedtls.a" \
	"$lib/libmbedx509.a" "$lib/libmbedcrypto.a" "$lib/libeverest.a" "$lib/libp256m.a" \
	-o "$out"
echo "built $out"
