# presets — Phase 6 step 43 (QOL §3): the ONE preset serializer, host-side and
# pure C11, beside hostmpe. A preset is params + the custom palette + the CC
# map + the strip's assignments and control values + the layout-state
# defaults, as JSON; the four shells share this code path so they cannot
# drift. No dependencies beyond libc (no allocation); the core's header for
# the struct types only. sumi_preset_apply (its own translation unit) pushes a
# preset into an instance and needs the core at link time — a consumer that
# never calls it never pulls it.
add_library(sumi_presets STATIC src/sumi_preset.c src/sumi_preset_apply.c)
target_include_directories(sumi_presets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/core/include)
set_target_properties(sumi_presets PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(sumi_presets PRIVATE /W4)
else()
    target_compile_options(sumi_presets PRIVATE -Wall -Wextra -Wpedantic -fvisibility=hidden)
endif()
