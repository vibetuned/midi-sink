# Voxo — the internal MPE sampler (Phase 7, SOUND §1; DECISIONS_6 #2): a
# SIBLING core beside libsumi behind the pure C voxo.h. Static archive only
# for now (the shells link it beside sumi_static; a shared build follows the
# core's pattern when the SDK question is asked). Compiles the core's MIDI
# normalizer FROM SOURCE — one MPE decoder, two builds, zero runtime coupling;
# libsumi is never linked. Same strictness as the core.
add_library(voxo STATIC
    src/voxo.cpp
    src/backend_miniaudio.cpp
    ${CMAKE_SOURCE_DIR}/core/src/midi_normalizer.cpp)
target_include_directories(voxo
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/core/include     # sumi_core.h: the log-fn typedef and the input-mode enum the normalizer speaks
            ${CMAKE_SOURCE_DIR}/core/src         # midi_normalizer.h, log_levels.h
            ${miniaudio_SOURCE_DIR})
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
    target_link_libraries(voxo PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(voxo PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
