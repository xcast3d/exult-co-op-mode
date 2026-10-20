# Phase 8 spike: libdatachannel through the Friend Mode lobby, linked the
# way Exult will be: against the static libraries tools/friend-mode/
# build-rtc.sh makes (libdatachannel with Mbed TLS).
#   tools/friend-mode/build-rtc.sh
#   cmake -S tools/friend-mode/rtc-spike -B <build>
#   cmake --build <build>
# See docs/friend-mode/lobby.md.
cmake_minimum_required(VERSION 3.16)
project(rtc_spike CXX)
set(CMAKE_CXX_STANDARD 17)

get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM)
set(RTC_PREFIX "${REPO}/third_party/build/${SYSTEM}-${CMAKE_SYSTEM_PROCESSOR}" CACHE PATH "build-rtc.sh's output")

find_package(Threads REQUIRED)
add_executable(rtc_spike rtc_spike.cc)
target_compile_definitions(rtc_spike PRIVATE RTC_STATIC)
target_include_directories(rtc_spike PRIVATE "${RTC_PREFIX}/include")
target_link_libraries(rtc_spike PRIVATE
	"${RTC_PREFIX}/lib/libdatachannel.a"
	"${RTC_PREFIX}/lib/libjuice.a"
	"${RTC_PREFIX}/lib/libusrsctp.a"
	"${RTC_PREFIX}/lib/libmbedtls.a"
	"${RTC_PREFIX}/lib/libmbedx509.a"
	"${RTC_PREFIX}/lib/libmbedcrypto.a"
	"${RTC_PREFIX}/lib/libeverest.a"
	"${RTC_PREFIX}/lib/libp256m.a"
	Threads::Threads)
