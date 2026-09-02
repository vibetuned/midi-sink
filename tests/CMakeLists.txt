# abi_c_compile: strict C11 translation unit including sumi_core.h, linked
# against the shared library (proves the header is pure C and the full ABI is
# exported from day one). On Windows the same source is built a second time
# against the STATIC library — DECISIONS #7: the C11 ABI test must link
# against both artifacts (the DLL consumer sees dllimport via SUMI_USE_SHARED,
# the static consumer sees the bare no-op SUMI_API).
if(MSVC)
    set(SUMI_ABI_WARNING_FLAGS /W4)
else()
    set(SUMI_ABI_WARNING_FLAGS -Wall -Wextra -Wpedantic)
endif()

add_executable(abi_c_compile abi_c_compile.c)
set_target_properties(abi_c_compile PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
target_compile_options(abi_c_compile PRIVATE ${SUMI_ABI_WARNING_FLAGS})
target_link_libraries(abi_c_compile PRIVATE sumi_shared)

add_test(NAME abi_c_compile COMMAND abi_c_compile)

if(WIN32)
    # The test executable must find sumi.dll next to itself at runtime.
    add_custom_command(TARGET abi_c_compile POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:sumi_shared> $<TARGET_FILE_DIR:abi_c_compile>
        VERBATIM)

    add_executable(abi_c_compile_static abi_c_compile.c)
    set_target_properties(abi_c_compile_static PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF)
    target_compile_options(abi_c_compile_static PRIVATE ${SUMI_ABI_WARNING_FLAGS})
    target_link_libraries(abi_c_compile_static PRIVATE sumi_static)
    add_test(NAME abi_c_compile_static COMMAND abi_c_compile_static)
endif()

# normalizer_tests: headless decoder/mapper tests — compiles the GPU-free core
# sources directly (no sokol, no libsumi link), CI-runnable without a GPU.
add_executable(normalizer_tests
    normalizer_tests.cpp
    ${CMAKE_SOURCE_DIR}/core/src/midi_normalizer.cpp
    ${CMAKE_SOURCE_DIR}/core/src/voice_mapper.cpp
    ${CMAKE_SOURCE_DIR}/core/src/layouts.cpp
    ${CMAKE_SOURCE_DIR}/core/src/displacement.cpp)
target_include_directories(normalizer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/core/src)
target_compile_features(normalizer_tests PRIVATE cxx_std_20)

add_test(NAME normalizer_tests COMMAND normalizer_tests)

# hostmpe (Phase 4): headless unit tests for the host-side MPE library, plus
# the strict-C11 consumer proving its header is pure C (working rule — the
# Swift module-map pattern can only import C headers).
add_executable(hostmpe_tests hostmpe_tests.cpp)
target_link_libraries(hostmpe_tests PRIVATE hostmpe)
add_test(NAME hostmpe_tests COMMAND hostmpe_tests)

add_executable(hostmpe_c_compile hostmpe_c_compile.c)
set_target_properties(hostmpe_c_compile PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
target_compile_options(hostmpe_c_compile PRIVATE ${SUMI_ABI_WARNING_FLAGS})
target_link_libraries(hostmpe_c_compile PRIVATE hostmpe)
add_test(NAME hostmpe_c_compile COMMAND hostmpe_c_compile)

# field_dump_compare: GPU-free comparator for the §4.6 cross-backend field
# regression — compares two --field-dump files against tolerances. Run
# manually against the committed Metal fixture (tests/fixtures/); see the
# step-11 evidence.
add_executable(field_dump_compare field_dump_compare.c)
set_target_properties(field_dump_compare PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)

# mpe_stress_win: Windows-native port of tests/osmose_stress.swift (CoreMIDI
# is macOS-only). Feeds the same Osmose-density MPE stream into a WinMM
# output port (use a loopMIDI virtual port so midi-sink sees it as an input).
if(WIN32)
    add_executable(mpe_stress_win mpe_stress_win.cpp)
    target_compile_features(mpe_stress_win PRIVATE cxx_std_20)
    target_link_libraries(mpe_stress_win PRIVATE winmm)
endif()

# Linux/ALSA evidence feeders: same schedules through snd_seq virtual source
# ports (midi-sink's rescan + track_virtual open them automatically).
if(UNIX AND NOT APPLE)
    find_package(ALSA REQUIRED)
    foreach(feeder mpe_stress_alsa wind_breath_alsa)
        add_executable(${feeder} ${feeder}.cpp)
        target_compile_features(${feeder} PRIVATE cxx_std_20)
        target_link_libraries(${feeder} PRIVATE ALSA::ALSA)
    endforeach()
endif()
