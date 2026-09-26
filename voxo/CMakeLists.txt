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
add_library(voxo STATIC
    src/voxo.cpp
    src/backend_miniaudio.cpp
    src/ds_preset.cpp
    src/decoders.cpp
    ${CMAKE_SOURCE_DIR}/core/src/midi_normalizer.cpp
    ${pugixml_SOURCE_DIR}/src/pugixml.cpp
    ${miniz_SOURCE_DIR}/miniz.c
    ${miniz_SOURCE_DIR}/miniz_zip.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c
    ${miniz_SOURCE_DIR}/miniz_tdef.c)
target_include_directories(voxo
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/core/include     # sumi_core.h: the log-fn typedef and the input-mode enum the normalizer speaks
            ${CMAKE_SOURCE_DIR}/core/src         # midi_normalizer.h, log_levels.h
            ${miniaudio_SOURCE_DIR}
            ${pugixml_SOURCE_DIR}/src
            ${miniz_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}/miniz_export
            ${dr_libs_SOURCE_DIR})
target_compile_definitions(voxo PRIVATE
    PUGIXML_NO_EXCEPTIONS PUGIXML_NO_XPATH
    MINIZ_NO_TIME MINIZ_NO_ARCHIVE_WRITING_APIS)
target_compile_features(voxo PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(voxo PRIVATE /EHs-c- /GR-)
else()
    target_compile_options(voxo PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti)
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
