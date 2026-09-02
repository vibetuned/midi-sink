# Desktop harness — links the static core (the shared lib is exercised by
# tests/abi_c_compile, so both artifacts get consumed).
set(SUMI_DESKTOP_SOURCES src/main.cpp src/midi_harness.cpp)
if(APPLE)
    list(APPEND SUMI_DESKTOP_SOURCES src/metal_layer_glue.mm)
endif()

# The app is midi-sink; only the core library carries the sumi name.
add_executable(midi-sink ${SUMI_DESKTOP_SOURCES})
target_link_libraries(midi-sink PRIVATE sumi_static glfw libremidi stb_headers)
# The --field-dump test hooks (sumi_debug.h) are internal, static-link-only —
# the harness reaches them through the core's src dir, same as the unit tests.
target_include_directories(midi-sink PRIVATE ${CMAKE_SOURCE_DIR}/core/src)

if(APPLE)
    set_source_files_properties(src/metal_layer_glue.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(midi-sink PRIVATE "-framework Cocoa" "-framework QuartzCore")
endif()
