# The wasm module (Phase 5 §5): libsumi + the export glue, built with
# `emcmake cmake -B build-web -G Ninja && cmake --build build-web`.
# Output: build-web/web-dist/{sumi.js, sumi.wasm} beside the copied host page
# (web/site). The C-ABI is the export surface (EXPORTED_FUNCTIONS below);
# web/sumi_web.cpp adds the struct-building shim JS cannot do itself.
add_executable(sumi_web sumi_web.cpp)
target_link_libraries(sumi_web PRIVATE sumi_static)
# sumi_debug.h is internal (static-link only) — the wasm IS a static link.
target_include_directories(sumi_web PRIVATE ${CMAKE_SOURCE_DIR}/core/src)
target_compile_features(sumi_web PRIVATE cxx_std_20)
target_compile_options(sumi_web PRIVATE --use-port=emdawnwebgpu -fno-exceptions -fno-rtti)

set(SUMI_WEB_EXPORTS
    _sumi_version _sumi_dropped_midi_count
    _sumi_create _sumi_destroy _sumi_resize _sumi_update _sumi_render
    _sumi_push_midi
    _sumi_set_params _sumi_get_params _sumi_set_input_mode _sumi_map_cc _sumi_clear_cc_map
    _sumi_trigger_paper_dip _sumi_read_print
    _sumi_layout_probe
    _sumi_add_drop _sumi_add_tine _sumi_add_vortex _sumi_add_wake _sumi_add_pinch
    _malloc _free)
list(JOIN SUMI_WEB_EXPORTS "," _sumi_web_exports)

set_target_properties(sumi_web PROPERTIES
    OUTPUT_NAME sumi
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web-dist)
target_link_options(sumi_web PRIVATE
    --use-port=emdawnwebgpu
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createSumi
    -sENVIRONMENT=web
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864
    -sEXPORTED_FUNCTIONS=${_sumi_web_exports}
    -sEXPORTED_RUNTIME_METHODS=cwrap,ccall,HEAPU8,HEAPU16,HEAPU32,HEAPF32,UTF8ToString,stringToUTF8,lengthBytesUTF8
    -sSTACK_SIZE=1048576
    -sASSERTIONS=0)

# The host page and scenes ship beside the module — copied on EVERY build
# (not as a POST_BUILD of the module, which only runs when the wasm relinks).
add_custom_target(sumi_web_site ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/web/site ${CMAKE_BINARY_DIR}/web-dist
    COMMENT "web-dist: copy host page"
    VERBATIM)
add_dependencies(sumi_web_site sumi_web)
