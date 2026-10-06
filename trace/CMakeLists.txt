# trace — Phase 10 step 67's app fixes (DECISIONS_9 #11): THE ORBIT TRACE as a
# host library, shared by the desktop, the iPad and the Tab. The desktop's
# step-59 bridge (orbit_trace.{h,cpp}, a C++ class the desktop still uses
# directly) moved here under a pure-C surface (sumi_trace.h) the Swift and
# Kotlin shells consume. It links the core and Voxo; no GPU, no device.
add_library(sumi_trace STATIC src/orbit_trace.cpp src/sumi_trace.cpp)
target_include_directories(sumi_trace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/voxo/include)
target_compile_features(sumi_trace PUBLIC cxx_std_20)
if(TARGET sumi_static)
    target_link_libraries(sumi_trace PUBLIC sumi_static voxo)
else()
    target_link_libraries(sumi_trace PUBLIC sumi voxo)
endif()
if(MSVC)
    target_compile_options(sumi_trace PRIVATE /W4)
else()
    target_compile_options(sumi_trace PRIVATE -Wall -Wextra -fvisibility=hidden -fno-exceptions -fno-rtti)
endif()
