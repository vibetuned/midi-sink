# Desktop harness — links the static core (the shared lib is exercised by
# tests/abi_c_compile, so both artifacts get consumed).
set(SUMI_DESKTOP_SOURCES src/main.cpp)
if(APPLE)
    list(APPEND SUMI_DESKTOP_SOURCES src/metal_layer_glue.mm)
endif()

add_executable(sumi_desktop ${SUMI_DESKTOP_SOURCES})
target_link_libraries(sumi_desktop PRIVATE sumi_static glfw)

if(APPLE)
    set_source_files_properties(src/metal_layer_glue.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(sumi_desktop PRIVATE "-framework Cocoa" "-framework QuartzCore")
endif()
