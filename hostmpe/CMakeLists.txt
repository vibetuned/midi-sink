# hostmpe — shared host-side MPE generation (PHASE4_SPEC.md, outside core/).
# Static archive only; consumed by the tablet shells (Swift module map / JNI)
# and by the headless unit tests. Same strictness as the core.
add_library(hostmpe STATIC src/hostmpe.cpp)
target_include_directories(hostmpe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(hostmpe PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(hostmpe PRIVATE /EHs-c- /GR-)
else()
    target_compile_options(hostmpe PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti)
endif()
