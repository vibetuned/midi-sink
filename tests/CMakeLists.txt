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
