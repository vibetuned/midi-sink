# sumi-shell — the one JNI shared library the Compose app loads (§5.4).
# Built by Gradle's externalNativeBuild pointing CMake at the REPO ROOT with
# the NDK toolchain (which defines ANDROID); the root lists add_subdirectory
# for this directory on Android only.
add_library(sumi-shell SHARED sumi_jni.cpp)
target_link_libraries(sumi-shell PRIVATE
    sumi_static      # carries GLESv3 via its INTERFACE link
    EGL
    android
    amidi
    log)
# The field-dump debug hooks are internal, static-link-only (sumi_debug.h) —
# same arrangement as the desktop harness and unit tests.
target_include_directories(sumi-shell PRIVATE ${CMAKE_SOURCE_DIR}/core/src)
target_compile_options(sumi-shell PRIVATE -fvisibility=hidden)
