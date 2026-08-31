# Suminagashi MPE Visualizer Engine — root orchestration (PROJECT_SPEC.md §6, §7)
cmake_minimum_required(VERSION 3.24)
project(suminagashi-engine VERSION 0.1.0 LANGUAGES C CXX)

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

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBREMIDI_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBREMIDI_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glm sokol libremidi glfw)

add_library(sokol_headers INTERFACE)
target_include_directories(sokol_headers INTERFACE "${sokol_SOURCE_DIR}")

# sokol-shdc (prebuilt binary) download + GLSL cross-compilation helper (§7).
include(CompileShaders)

add_subdirectory(core)
add_subdirectory(desktop)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
