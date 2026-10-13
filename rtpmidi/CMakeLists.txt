# rtpmidi — Phase 10 step 67's app fixes (DECISIONS_9 #14): RTP-MIDI with
# Apple's session protocol as a host library for the shells whose OS has no
# network session — the Tab. Pure C surface, POSIX UDP underneath, no
# threads; headlessly tested on the desktop over the loopback. Not built on
# Windows (winsock is not here; the desktop has no use for it yet).
add_library(sumi_rtpmidi STATIC src/sumi_rtpmidi.cpp)
target_include_directories(sumi_rtpmidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(sumi_rtpmidi PUBLIC cxx_std_20)
target_compile_options(sumi_rtpmidi PRIVATE -Wall -Wextra -fvisibility=hidden -fno-exceptions -fno-rtti)
