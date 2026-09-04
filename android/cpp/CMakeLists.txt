# sumi-shell — the one JNI shared library the Compose app loads (§5.4).
# Built by Gradle's externalNativeBuild pointing CMake at the REPO ROOT with
# the NDK toolchain (which defines ANDROID); the root lists add_subdirectory
# for this directory on Android only.
#
# Phase 4 (Step 22): the shell links hostmpe (the shared host-side MPE
# library — Kotlin consumes it through the JNI layer exactly as Swift does
# through the module map) and carries the two headless suites with their
# main() renamed, so the FULL hostmpe suite + the normalizer's loopback
# conformance suite run ON-DEVICE from a debug Intent (DONE gate).
add_library(sumi-shell SHARED
    sumi_jni.cpp
    sumi_play.cpp
    ${CMAKE_SOURCE_DIR}/tests/hostmpe_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/normalizer_tests.cpp)
target_link_libraries(sumi-shell PRIVATE
    sumi_static      # carries GLESv3 via its INTERFACE link
    hostmpe
    EGL
    android
    amidi
    log)
# The field-dump debug hooks are internal, static-link-only (sumi_debug.h) —
# same arrangement as the desktop harness and unit tests. normalizer_tests
# includes the core's internal headers the same way.
target_include_directories(sumi-shell PRIVATE ${CMAKE_SOURCE_DIR}/core/src)
target_compile_options(sumi-shell PRIVATE -fvisibility=hidden)
set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/hostmpe_tests.cpp PROPERTIES
    COMPILE_DEFINITIONS "main=hostmpe_tests_main")
set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/normalizer_tests.cpp PROPERTIES
    COMPILE_DEFINITIONS "main=normalizer_tests_main")
