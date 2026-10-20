# Phase 8 spike: libdatachannel through the Friend Mode lobby.
#   cmake -B build -DLIBDATACHANNEL_DIR=/path/to/libdatachannel
#   cmake --build build
# See docs/friend-mode/lobby.md.
cmake_minimum_required(VERSION 3.16)
project(rtc_spike CXX)
set(CMAKE_CXX_STANDARD 17)

set(LIBDATACHANNEL_DIR "" CACHE PATH "libdatachannel source checkout (with submodules)")
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(${LIBDATACHANNEL_DIR} libdatachannel EXCLUDE_FROM_ALL)

add_executable(rtc_spike rtc_spike.cc)
target_include_directories(rtc_spike PRIVATE ${LIBDATACHANNEL_DIR}/deps/json/single_include)
target_link_libraries(rtc_spike PRIVATE LibDataChannel::LibDataChannelStatic)
