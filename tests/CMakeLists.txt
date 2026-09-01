# abi_c_compile: strict C11 translation unit including sumi_core.h, linked
# against the shared library (proves the header is pure C and the full ABI is
# exported from day one).
add_executable(abi_c_compile abi_c_compile.c)
set_target_properties(abi_c_compile PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
target_compile_options(abi_c_compile PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(abi_c_compile PRIVATE sumi_shared)

add_test(NAME abi_c_compile COMMAND abi_c_compile)

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
