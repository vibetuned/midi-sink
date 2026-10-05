# replay — Phase 9 step 65 (QOL §1, DECISIONS_8 #22): SESSION REPLAY, host-side
# and pure C11, beside hostmpe and the presets. The recorder (the shell's MIDI
# producer stages its bytes, the render thread hands them to the core at the
# frame boundary — so every byte's frame is exact), the text file, the player
# (a frame cursor feeding a sink) and the wall-time re-bucketing of the
# negative test. No dependencies beyond libc; the core's header for the
# types. sumi_replay_apply (its own translation unit) feeds a frame into an
# instance — the state through the preset serializer — and needs the core and
# sumi_presets at link time; a consumer that never calls it never pulls them.
add_library(sumi_replay STATIC src/sumi_replay.c src/sumi_replay_apply.c)
target_include_directories(sumi_replay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/presets/include)
target_link_libraries(sumi_replay PUBLIC sumi_presets)
set_target_properties(sumi_replay PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(sumi_replay PRIVATE /W4)
else()
    target_compile_options(sumi_replay PRIVATE -Wall -Wextra -Wpedantic -fvisibility=hidden)
endif()
