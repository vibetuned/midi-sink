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

# Dear ImGui (desktop app only): the shared settings window, drawn into a
# second GLFW window with its own GL context (Phase 5, DECISIONS_4 #2). The
# harness may not include sokol headers, so sokol_imgui was never an option.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b)

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
    FetchContent_MakeAvailable(glm sokol libremidi glfw stb imgui)
endif()

# ---------------------------------------------------------------------------
# App version (Phase 5 working rule: no hand-edited version numbers). The
# release spine injects SUMI_APP_VERSION from the git tag; a local build
# falls back to `git describe`. The core's sumi_version() is the ABI version
# and a different number by design (DECISIONS_4 #3).
# ---------------------------------------------------------------------------
set(SUMI_APP_VERSION "" CACHE STRING
    "App version (vX.Y.Z from the release tag); empty = git describe")
set(SUMI_GIT_COMMIT "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE _sumi_commit OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET RESULT_VARIABLE _sumi_r)
    if(_sumi_r EQUAL 0 AND _sumi_commit)
        set(SUMI_GIT_COMMIT "${_sumi_commit}")
    endif()
    if(NOT SUMI_APP_VERSION)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                        OUTPUT_VARIABLE _sumi_describe OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET RESULT_VARIABLE _sumi_r)
        if(_sumi_r EQUAL 0 AND _sumi_describe)
            set(SUMI_APP_VERSION "${_sumi_describe}")
        endif()
    endif()
endif()
if(NOT SUMI_APP_VERSION)
    set(SUMI_APP_VERSION "0.0.0-dev")
endif()
string(REGEX REPLACE "^v" "" SUMI_APP_VERSION "${SUMI_APP_VERSION}")
# Info.plist needs numeric fields: X.Y.Z from a tag, 0.0.0 for dev builds.
if(SUMI_APP_VERSION MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(SUMI_APP_VERSION_SEMVER "${CMAKE_MATCH_1}")
else()
    set(SUMI_APP_VERSION_SEMVER "0.0.0")
endif()
message(STATUS "midi-sink version ${SUMI_APP_VERSION} (commit ${SUMI_GIT_COMMIT}, plist ${SUMI_APP_VERSION_SEMVER})")

add_library(sokol_headers INTERFACE)
target_include_directories(sokol_headers INTERFACE "${sokol_SOURCE_DIR}")

if(NOT SUMI_MOBILE)
    add_library(stb_headers INTERFACE)
    target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")

    # imgui core + the GLFW/OpenGL3 backends. The OpenGL3 backend carries its
    # own function loader (no GLEW/GLAD); it only needs the platform GL
    # library to resolve symbols at runtime.
    add_library(imgui_glfw_gl3 STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(imgui_glfw_gl3 PUBLIC ${imgui_SOURCE_DIR})
    target_compile_features(imgui_glfw_gl3 PUBLIC cxx_std_17)
    target_compile_definitions(imgui_glfw_gl3 PUBLIC GL_SILENCE_DEPRECATION)
    target_link_libraries(imgui_glfw_gl3 PUBLIC glfw)
    if(APPLE)
        target_link_libraries(imgui_glfw_gl3 PUBLIC "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(imgui_glfw_gl3 PUBLIC opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui_glfw_gl3 PUBLIC OpenGL::GL ${CMAKE_DL_LIBS})
    endif()
endif()

# sokol-shdc (prebuilt binary) download + GLSL cross-compilation helper (§7).
# Cross-compiles on the HOST (the NDK/iOS toolchains don't affect it).
include(CompileShaders)

add_subdirectory(core)
# hostmpe (Phase 4): host-side MPE generation, consumed by the tablet shells
# (Swift module map / JNI) and headlessly unit-tested on desktop. Built
# everywhere — it has no platform dependencies at all.
add_subdirectory(hostmpe)
if(ANDROID)
    # The one JNI shared lib the Compose shell loads (§5.4).
    add_subdirectory(android/cpp)
elseif(NOT SUMI_MOBILE)
    add_subdirectory(desktop)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
