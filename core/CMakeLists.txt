# libsumi — static + shared (PROJECT_SPEC.md §1, §6).
set(SUMI_CORE_SOURCES
    src/engine.cpp
    src/renderer.cpp
    src/displacement.cpp
    src/midi_normalizer.cpp
    src/voice_mapper.cpp)

if(APPLE)
    list(APPEND SUMI_CORE_SOURCES src/swapchain_metal.mm)
endif()

# One object library feeds both the static and shared artifacts.
# (SUMI_BUILD_SHARED only matters on Windows/D3D11 in phase 2 — on macOS the
# visibility attribute in sumi_core.h covers both; see DECISIONS.md.)
add_library(sumi_objs OBJECT ${SUMI_CORE_SOURCES})
target_include_directories(sumi_objs
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sumi_objs PRIVATE sokol_headers glm::glm)
target_compile_features(sumi_objs PUBLIC cxx_std_20)
target_compile_options(sumi_objs PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti)

if(APPLE)
    set_source_files_properties(src/swapchain_metal.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc")
endif()

sumi_compile_shader(sumi_objs ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/deform.glsl)
sumi_compile_shader(sumi_objs ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/composite.glsl)

add_library(sumi_static STATIC $<TARGET_OBJECTS:sumi_objs>)
add_library(sumi_shared SHARED $<TARGET_OBJECTS:sumi_objs>)

foreach(lib sumi_static sumi_shared)
    target_include_directories(${lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(${lib} PROPERTIES OUTPUT_NAME sumi)
    if(APPLE)
        target_link_libraries(${lib} INTERFACE
            "-framework Metal" "-framework QuartzCore" "-framework Foundation")
    endif()
endforeach()

if(APPLE)
    # The shared lib must carry its own framework deps.
    target_link_libraries(sumi_shared PRIVATE
        "-framework Metal" "-framework QuartzCore" "-framework Foundation")
endif()
