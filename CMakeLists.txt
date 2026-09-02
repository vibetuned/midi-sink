# Suminagashi MPE Visualizer Engine — root orchestration (PROJECT_SPEC.md §6, §7)
cmake_minimum_required(VERSION 3.24)
project(midi-sink VERSION 0.1.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)
include(CTest)

# ---------------------------------------------------------------------------
# Pinned dependencies (§7). All via FetchContent, pinned tags/commits.
# ---------------------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1)

# sokol has no build system of its own; we expose it as an INTERFACE target below.
FetchContent_Declare(sokol
    GIT_REPOSITORY https://github.com/floooh/sokol.git
    GIT_TAG        1847290135f95e57e6d220b0a41208306aafc0dd)  # master 2026-08-30

FetchContent_Declare(libremidi
    GIT_REPOSITORY https://github.com/celtera/libremidi.git
    GIT_TAG        v5.4.3)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4)

# stb_image_write (desktop harness only: PNG export of paper-dip prints).
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20)  # master 2026

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBREMIDI_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBREMIDI_TESTS OFF CACHE BOOL "" FORCE)

# Mobile builds produce the static core only (§5.4): the SwiftUI shell in
# ios/ and the Compose shell in android/ are the hosts — no GLFW window, no
# libremidi (CoreMIDI / AMidi feed sumi_push_midi from the shell), no desktop
# harness, no ctest binaries.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ANDROID)
    set(SUMI_MOBILE ON)
else()
    set(SUMI_MOBILE OFF)
endif()

if(SUMI_MOBILE)
    FetchContent_MakeAvailable(glm sokol)
else()
    FetchContent_MakeAvailable(glm sokol libremidi glfw stb)
endif()

add_library(sokol_headers INTERFACE)
target_include_directories(sokol_headers INTERFACE "${sokol_SOURCE_DIR}")

if(NOT SUMI_MOBILE)
    add_library(stb_headers INTERFACE)
    target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")
endif()

# sokol-shdc (prebuilt binary) download + GLSL cross-compilation helper (§7).
# Cross-compiles on the HOST (the NDK/iOS toolchains don't affect it).
include(CompileShaders)

add_subdirectory(core)
if(ANDROID)
    # The one JNI shared lib the Compose shell loads (§5.4).
    add_subdirectory(android/cpp)
elseif(NOT SUMI_MOBILE)
    add_subdirectory(desktop)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
