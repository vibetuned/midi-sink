# The wasm module (Phase 5 §5): libsumi + the export glue, built with
# `emcmake cmake -B build-web -G Ninja && cmake --build build-web`.
# Output: build-web/web-dist/{sumi.js, sumi.wasm} beside the copied host page
# (web/site). The C-ABI is the export surface (EXPORTED_FUNCTIONS below);
# web/sumi_web.cpp adds the struct-building shim JS cannot do itself.
add_executable(sumi_web sumi_web.cpp)
target_link_libraries(sumi_web PRIVATE sumi_static sumi_presets)   # step 44b: the one preset serializer
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
    _sumi_add_drop _sumi_add_tine _sumi_add_vortex _sumi_add_wake _sumi_add_pinch _sumi_add_chladni
    _sumi_add_burst _sumi_add_spark_shear _sumi_add_spark _sumi_add_chirikov
    _sumi_gesture_tap _sumi_gesture_pinch _sumi_gesture_twist _sumi_gesture_press _sumi_gesture_press_end
    _sumi_set_palette
    _sumi_get_palette
    _sumi_palette_preset_count
    _sumi_palette_preset
    _sumi_read_field
    _sumi_export_begin
    _sumi_export_poll
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

# ---------------------------------------------------------------------------
# Step 59b (SYNTH §6, DECISIONS_7 #28): THE SUZU LAB — Voxo with no device, Suzu only,
# a STANDALONE wasm (no emscripten JavaScript glue) that the lab's AudioWorklet
# instantiates itself and node loads for the web gate: the same module, the same
# engine script (web/suzu/site/suzu-engine.js), two hosts. Fixed memory (no growth:
# the worklet's views never detach); the exports are suzu_web.c's flat surface.
# Output: build-web/suzu-dist/{suzu.wasm, the lab's pages (step 59c: a page per voice family)}.
add_executable(suzu_web suzu/suzu_web.c)
target_link_libraries(suzu_web PRIVATE voxo)
set(SUZU_WEB_EXPORTS _sw_apply _sw_create _sw_destroy _sw_inspect _sw_inspect_ptr _sw_inspect_stride _sw_log_clear _sw_log_len _sw_log_ptr _sw_midi _sw_out_ptr _sw_param_count _sw_param_defaults _sw_param_get _sw_param_is_int _sw_param_name _sw_param_set _sw_recent_ptr _sw_render _sw_set_gain _sw_trace _sw_trace_decimation _sw_trace_mask _sw_trace_ptr _sw_trace_recent _sw_trace_stride _sw_version)
list(JOIN SUZU_WEB_EXPORTS "," _suzu_web_exports)
set_target_properties(suzu_web PROPERTIES
    OUTPUT_NAME suzu
    SUFFIX ".wasm"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/suzu-dist)
target_link_options(suzu_web PRIVATE
    -sSTANDALONE_WASM=1 --no-entry
    -sEXPORTED_FUNCTIONS=${_suzu_web_exports}
    -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=0
    -sSTACK_SIZE=262144
    -sFILESYSTEM=0
    -sASSERTIONS=0)
add_custom_target(suzu_web_site ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/web/suzu/site ${CMAKE_BINARY_DIR}/suzu-dist
    COMMENT "suzu-dist: copy the lab's pages"
    VERBATIM)
add_dependencies(suzu_web_site suzu_web)
