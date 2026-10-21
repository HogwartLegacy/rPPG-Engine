# NDK build of librppg_jni.so (referenced from the app module's build.gradle externalNativeBuild).
#
#   android { defaultConfig { externalNativeBuild { cmake {
#       arguments "-DRPPG_ONNXRUNTIME_DIR=/path/to/onnxruntime-android-1.20.1" } } }
#     externalNativeBuild { cmake { path "path/to/rPPG/bindings/android/CMakeLists.txt" } } }
cmake_minimum_required(VERSION 3.20)
project(rppg_android LANGUAGES C CXX)

set(RPPG_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(RPPG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. rppg_core)

add_library(rppg_jni SHARED rppg_jni.cpp)
target_link_libraries(rppg_jni PRIVATE rppg log)
target_link_options(rppg_jni PRIVATE "-Wl,--gc-sections")
