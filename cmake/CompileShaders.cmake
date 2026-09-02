# CompileShaders.cmake — fetch a pinned sokol-shdc prebuilt binary and wire
# GLSL -> MSL/HLSL5/GLSL410/GLES3 cross-compilation (PROJECT_SPEC.md §7).
#
# sokol-shdc is a prebuilt binary, not a library. We download it from the
# pinned sokol-tools-bin commit for the host OS/arch, or use SOKOL_SHDC_PATH
# if the user provides one.

set(SUMI_SOKOL_TOOLS_BIN_COMMIT "11d0cf678105d614d675e6d9bd2aaf3eeff12f8c")  # master 2026-08-29

set(SOKOL_SHDC_PATH "" CACHE FILEPATH "Path to an existing sokol-shdc binary (skips download)")

if(SOKOL_SHDC_PATH)
    set(SUMI_SHDC_EXECUTABLE "${SOKOL_SHDC_PATH}")
    if(NOT EXISTS "${SUMI_SHDC_EXECUTABLE}")
        message(FATAL_ERROR "SOKOL_SHDC_PATH points to a non-existent file: ${SUMI_SHDC_EXECUTABLE}")
    endif()
else()
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(_shdc_dir "osx_arm64")
        else()
            set(_shdc_dir "osx")
        endif()
        set(_shdc_name "sokol-shdc")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_shdc_dir "linux_arm64")
        else()
            set(_shdc_dir "linux")
        endif()
        set(_shdc_name "sokol-shdc")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_shdc_dir "win32")
        set(_shdc_name "sokol-shdc.exe")
    else()
        message(FATAL_ERROR "No prebuilt sokol-shdc for host '${CMAKE_HOST_SYSTEM_NAME}'; set SOKOL_SHDC_PATH.")
    endif()

    set(SUMI_SHDC_EXECUTABLE "${CMAKE_BINARY_DIR}/tools/${_shdc_name}")
    if(NOT EXISTS "${SUMI_SHDC_EXECUTABLE}")
        set(_shdc_url "https://raw.githubusercontent.com/floooh/sokol-tools-bin/${SUMI_SOKOL_TOOLS_BIN_COMMIT}/bin/${_shdc_dir}/${_shdc_name}")
        message(STATUS "Downloading sokol-shdc (${_shdc_dir} @ ${SUMI_SOKOL_TOOLS_BIN_COMMIT})")
        file(DOWNLOAD "${_shdc_url}" "${SUMI_SHDC_EXECUTABLE}" STATUS _shdc_status)
        list(GET _shdc_status 0 _shdc_err)
        if(NOT _shdc_err EQUAL 0)
            file(REMOVE "${SUMI_SHDC_EXECUTABLE}")
            message(FATAL_ERROR "Failed to download sokol-shdc from ${_shdc_url}: ${_shdc_status}")
        endif()
        file(CHMOD "${SUMI_SHDC_EXECUTABLE}" PERMISSIONS
             OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    endif()
endif()

# All output dialects configured up front (macOS/iOS Metal, D3D11, GL, GLES3).
# (Spec §5/§7 say "GLSL330"; current sokol-shdc dropped glsl330 — glsl410 matches
#  the phase-2 Linux GL 4.1 core target. Recorded in DECISIONS.md.)
set(SUMI_SHDC_SLANG "metal_macos:metal_ios:hlsl5:glsl410:glsl300es" CACHE STRING "sokol-shdc output dialects")

# sumi_compile_shader(<target> <shader.glsl>)
# Cross-compiles a sokol-shdc GLSL file to a C header next to the target's
# build dir and adds the generated header + include path to <target>.
# Safe to call for the same shader from multiple targets in one directory
# (Windows builds the core objects twice, see core/CMakeLists.txt): the
# custom command is created once, later calls only attach the output.
function(sumi_compile_shader target shader_src)
    get_filename_component(_name "${shader_src}" NAME_WE)
    get_filename_component(_abs "${shader_src}" ABSOLUTE)
    set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(_out "${_out_dir}/${_name}.glsl.h")
    file(MAKE_DIRECTORY "${_out_dir}")
    get_property(_have_rule SOURCE "${_out}" DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                 PROPERTY SUMI_SHDC_RULE_CREATED)
    if(NOT _have_rule)
        add_custom_command(
            OUTPUT "${_out}"
            COMMAND "${SUMI_SHDC_EXECUTABLE}"
                    --input "${_abs}"
                    --output "${_out}"
                    --slang "${SUMI_SHDC_SLANG}"
                    --format sokol
                    --errfmt gcc
            DEPENDS "${_abs}" "${SUMI_SHDC_EXECUTABLE}"
            COMMENT "sokol-shdc: ${_name}.glsl -> ${_name}.glsl.h [${SUMI_SHDC_SLANG}]"
            VERBATIM)
        set_source_files_properties("${_out}" PROPERTIES
            GENERATED TRUE HEADER_FILE_ONLY TRUE SUMI_SHDC_RULE_CREATED TRUE)
    endif()
    target_sources(${target} PRIVATE "${_out}")
    target_include_directories(${target} PRIVATE "${_out_dir}")
endfunction()
