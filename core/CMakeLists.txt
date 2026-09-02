# libsumi — static + shared (PROJECT_SPEC.md §1, §6).
set(SUMI_CORE_SOURCES
    src/engine.cpp
    src/renderer.cpp
    src/displacement.cpp
    src/midi_normalizer.cpp
    src/voice_mapper.cpp
    src/layouts.cpp)

if(APPLE)
    list(APPEND SUMI_CORE_SOURCES src/swapchain_metal.mm)
elseif(WIN32)
    list(APPEND SUMI_CORE_SOURCES src/swapchain_d3d11.cpp)
else()
    # Linux (and Android later): OpenGL / GLES3, host-owned context (§5.1).
    list(APPEND SUMI_CORE_SOURCES src/swapchain_gl.cpp)
endif()

# Core compile flags: no exceptions, no RTTI, hidden symbols (spec §7; the
# public ABI is exported explicitly via SUMI_API). MSVC spells these
# /EHs-c- (no unwind tables, extern "C" never throws) and /GR-; symbol
# hiding is the Windows default, so no /visibility analog is needed.
if(MSVC)
    set(SUMI_CORE_COMPILE_OPTIONS /EHs-c- /GR-)
else()
    set(SUMI_CORE_COMPILE_OPTIONS -fvisibility=hidden -fno-exceptions -fno-rtti)
endif()

# DECISIONS.md #7 / DECISIONS_2.md: on Windows one object set cannot feed both
# artifacts — the shared objects need __declspec(dllexport) baked in
# (SUMI_BUILD_SHARED) while the static objects must stay bare. Objects are
# therefore compiled TWICE on Windows only; every other platform keeps the
# single set (visibility("default") serves both artifacts there).
function(sumi_add_object_lib name)
    add_library(${name} OBJECT ${SUMI_CORE_SOURCES})
    target_include_directories(${name}
        PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE sokol_headers glm::glm)
    target_compile_features(${name} PUBLIC cxx_std_20)
    target_compile_options(${name} PRIVATE ${SUMI_CORE_COMPILE_OPTIONS})
    sumi_compile_shader(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/deform.glsl)
    sumi_compile_shader(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/composite.glsl)
endfunction()

sumi_add_object_lib(sumi_objs)

if(APPLE)
    set_source_files_properties(src/swapchain_metal.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc")
endif()

add_library(sumi_static STATIC $<TARGET_OBJECTS:sumi_objs>)

if(WIN32)
    sumi_add_object_lib(sumi_objs_shared)
    target_compile_definitions(sumi_objs_shared PRIVATE SUMI_BUILD_SHARED)
    add_library(sumi_shared SHARED $<TARGET_OBJECTS:sumi_objs_shared>)
    # Consumers of the DLL must see dllimport declarations (sumi_core.h's
    # three-state SUMI_API macro).
    target_compile_definitions(sumi_shared INTERFACE SUMI_USE_SHARED)
else()
    add_library(sumi_shared SHARED $<TARGET_OBJECTS:sumi_objs>)
endif()

foreach(lib sumi_static sumi_shared)
    target_include_directories(${lib} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(${lib} PROPERTIES OUTPUT_NAME sumi)
    if(APPLE)
        target_link_libraries(${lib} INTERFACE
            "-framework Metal" "-framework QuartzCore" "-framework Foundation")
    endif()
endforeach()

if(WIN32)
    # Both artifacts share OUTPUT_NAME "sumi" elsewhere, but on Windows the
    # DLL's import library (sumi.lib) would collide with the static sumi.lib —
    # the static archive keeps a distinct name.
    set_target_properties(sumi_static PROPERTIES OUTPUT_NAME sumi_static)
    # sokol's MSVC path auto-links d3d11/dxgi via #pragma comment(lib, ...);
    # spelled out here as well so non-MSVC Windows toolchains keep working.
    foreach(lib sumi_static sumi_shared)
        target_link_libraries(${lib} INTERFACE d3d11 dxgi)
    endforeach()
    target_link_libraries(sumi_shared PRIVATE d3d11 dxgi)
endif()

if(APPLE)
    # The shared lib must carry its own framework deps.
    target_link_libraries(sumi_shared PRIVATE
        "-framework Metal" "-framework QuartzCore" "-framework Foundation")
endif()

if(UNIX AND NOT APPLE)
    # sokol_gfx's desktop-GL path links libGL directly (it uses the system
    # <GL/gl.h> prototypes — no loader library, per the GL backend notes in
    # swapchain_gl.cpp).
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)
    foreach(lib sumi_static sumi_shared)
        target_link_libraries(${lib} INTERFACE OpenGL::GL)
    endforeach()
    target_link_libraries(sumi_shared PRIVATE OpenGL::GL)
endif()
