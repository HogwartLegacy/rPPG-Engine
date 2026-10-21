cmake_minimum_required(VERSION 3.20)
project(rppg VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RPPG_WITH_ONNXRUNTIME "Neural components (face detector, face mesh, deep rPPG) via ONNX Runtime" ON)
option(RPPG_BUILD_SHARED "Build librppg as a shared library (C API exported)" OFF)
option(RPPG_BUILD_APPS "Build desktop apps: rppg_cli, rppg_live, rppg_bench (needs OpenCV)" ON)
option(RPPG_BUILD_TESTS "Build unit tests" ON)
set(RPPG_ONNXRUNTIME_DIR "" CACHE PATH "ONNX Runtime package root containing include/ and lib/")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus /wd4244 /wd4267 /wd4305)
  add_compile_definitions(_USE_MATH_DEFINES NOMINMAX _CRT_SECURE_NO_WARNINGS)
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " /O2 /Ob2 /fp:precise")
else()
  add_compile_options(-Wall -Wextra -Wno-sign-compare -Wno-unused-parameter)
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3")
endif()

find_package(Threads REQUIRED)

if(RPPG_WITH_ONNXRUNTIME)
  # Desktop packages: include/ + lib/. Android AAR: headers/ + jni/<abi>/. iOS xcframework: Headers/.
  find_path(RPPG_ORT_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS ${RPPG_ONNXRUNTIME_DIR}/include ${RPPG_ONNXRUNTIME_DIR}/include/onnxruntime
          ${RPPG_ONNXRUNTIME_DIR}/include/onnxruntime/core/session ${RPPG_ONNXRUNTIME_DIR}/headers
          ${RPPG_ONNXRUNTIME_DIR}/Headers
    NO_CMAKE_FIND_ROOT_PATH)
  find_library(RPPG_ORT_LIBRARY NAMES onnxruntime
    HINTS ${RPPG_ONNXRUNTIME_DIR}/lib ${RPPG_ONNXRUNTIME_DIR}/jni/${ANDROID_ABI} ${RPPG_ONNXRUNTIME_DIR}
    NO_CMAKE_FIND_ROOT_PATH)
  if(NOT RPPG_ORT_INCLUDE_DIR OR NOT RPPG_ORT_LIBRARY)
    message(FATAL_ERROR "ONNX Runtime not found. Set RPPG_ONNXRUNTIME_DIR or -DRPPG_WITH_ONNXRUNTIME=OFF")
  endif()
  add_library(rppg_onnxruntime INTERFACE)
  target_include_directories(rppg_onnxruntime INTERFACE ${RPPG_ORT_INCLUDE_DIR})
  target_link_libraries(rppg_onnxruntime INTERFACE ${RPPG_ORT_LIBRARY})
  if(WIN32)
    set(RPPG_ORT_RUNTIME_DLLS "${RPPG_ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
                              "${RPPG_ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll")
  endif()
  message(STATUS "rppg: ONNX Runtime at ${RPPG_ORT_INCLUDE_DIR}")
endif()

add_subdirectory(src)

if(RPPG_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(RPPG_BUILD_APPS)
  find_package(OpenCV QUIET COMPONENTS core imgproc videoio highgui)
  if(OpenCV_FOUND)
    add_subdirectory(apps)
  else()
    message(WARNING "rppg: OpenCV not found, desktop apps are skipped (set OpenCV_DIR)")
  endif()
endif()
