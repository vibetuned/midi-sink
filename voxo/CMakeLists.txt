# Voxo — the internal MPE sampler (Phase 7, SOUND §1; DECISIONS_6 #2): a
# SIBLING core beside libsumi behind the pure C voxo.h. Static archive only
# for now (the shells link it beside sumi_static; a shared build follows the
# core's pattern when the SDK question is asked). Compiles the core's MIDI
# normalizer FROM SOURCE — one MPE decoder, two builds, zero runtime coupling;
# libsumi is never linked. Same strictness as the core.
# The Decent Sampler front end (step 50): pugixml (no exceptions, no XPath),
# miniz's four split sources, dr_wav / dr_flac — all compiled into this
# archive, none of their own build systems. miniz's split header includes a
# generated miniz_export.h; we provide the one-line stub it expects.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/miniz_export/miniz_export.h "#define MINIZ_EXPORT\n")
# One source list, two backends (step 59b, DECISIONS_7 #28): miniaudio on every native
# platform; `none` where the host drives voxo_render itself — the web (the Suzu lab's
# AudioWorklet, the web gate in node) and the native reference build the gate compares
# against (voxo_nofma below).
set(VOXO_CORE_SOURCES
    src/voxo.cpp
    src/ds_preset.cpp
    src/decoders.cpp
    src/instrument.cpp
    src/bus.cpp
    src/suzu.cpp
    ${CMAKE_SOURCE_DIR}/core/src/midi_normalizer.cpp
    ${pugixml_SOURCE_DIR}/src/pugixml.cpp
    ${miniz_SOURCE_DIR}/miniz.c
    ${miniz_SOURCE_DIR}/miniz_zip.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c
    ${miniz_SOURCE_DIR}/miniz_tdef.c)
function(voxo_configure target)
    target_include_directories(${target}
        PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_SOURCE_DIR}/core/include     # sumi_core.h: the log-fn typedef and the input-mode enum the normalizer speaks
                ${CMAKE_SOURCE_DIR}/core/src         # midi_normalizer.h, log_levels.h
                ${pugixml_SOURCE_DIR}/src
                ${miniz_SOURCE_DIR}
                ${CMAKE_CURRENT_BINARY_DIR}/miniz_export
                ${dr_libs_SOURCE_DIR})
    target_compile_definitions(${target} PRIVATE
        PUGIXML_NO_EXCEPTIONS PUGIXML_NO_XPATH
        MINIZ_NO_TIME MINIZ_NO_ARCHIVE_WRITING_APIS)
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /EHs-c- /GR-)
    else()
        target_compile_options(${target} PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti)
    endif()
endfunction()

if(EMSCRIPTEN)
    # The web: no device, no audio library (the lab's worklet renders).
    add_library(voxo STATIC ${VOXO_CORE_SOURCES} src/backend_none.cpp)
    voxo_configure(voxo)
    return()
endif()

add_library(voxo STATIC ${VOXO_CORE_SOURCES} src/backend_miniaudio.cpp)
voxo_configure(voxo)
target_include_directories(voxo PRIVATE ${miniaudio_SOURCE_DIR})

# The web gate's native reference (step 59b): the same sources with no device and no
# fused multiply-add — wasm has none, so this build and the lab's wasm compute the same
# operations in the same order (DECISIONS_7 #29). Tests only; nothing ships it.
if(BUILD_TESTING AND NOT SUMI_MOBILE)
    add_library(voxo_nofma STATIC ${VOXO_CORE_SOURCES} src/backend_none.cpp)
    voxo_configure(voxo_nofma)
    if(NOT MSVC)
        # -O3 whatever the tree's build type, as the web build: at -O1 and up the compiler rewrites some
        # math-library calls (pow(2, x) as exp2(x), …), so an unoptimized reference calls other functions
        target_compile_options(voxo_nofma PRIVATE -ffp-contract=off -O3)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(voxo_nofma PUBLIC m)
    endif()
endif()
# miniaudio's platform backends (DECISIONS_6 #5). Apple links the frameworks
# (MA_NO_RUNTIME_LINKING in backend_miniaudio.cpp; iOS forbids dlopen anyway);
# Linux and Windows load ALSA / PulseAudio / WASAPI at runtime.
if(APPLE)
    # miniaudio's CoreAudio backend speaks to AVAudioSession on iOS through
    # Objective-C headers: the implementation TU compiles as Objective-C++ on
    # Apple (the same file stays plain C++ everywhere else).
    enable_language(OBJCXX)
    set_source_files_properties(src/backend_miniaudio.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(voxo PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_link_libraries(voxo PUBLIC "-framework AVFoundation")   # the audio session miniaudio activates
    endif()
elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(voxo PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
