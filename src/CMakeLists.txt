set(RPPG_SOURCES
  dsp/fft.cpp
  dsp/filter.cpp
  dsp/detrend.cpp
  dsp/spectral.cpp
  dsp/resample.cpp
  dsp/peaks.cpp
  dsp/stats.cpp
  methods/methods.cpp
  methods/prism.cpp
  vision/image.cpp
  vision/roi.cpp
  vision/face.cpp
  nn/ort_model.cpp
  nn/stream_rppg.cpp
  engine/analysis.cpp
  engine/physio.cpp
  engine/engine.cpp
  engine/c_api.cpp
)

if(RPPG_BUILD_SHARED)
  add_library(rppg SHARED ${RPPG_SOURCES})
  target_compile_definitions(rppg PRIVATE RPPG_BUILDING_DLL PUBLIC RPPG_SHARED)
else()
  add_library(rppg STATIC ${RPPG_SOURCES})
endif()

target_include_directories(rppg
  PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rppg PUBLIC Threads::Threads)
if(RPPG_WITH_ONNXRUNTIME)
  target_link_libraries(rppg PRIVATE rppg_onnxruntime)
  target_compile_definitions(rppg PRIVATE RPPG_HAVE_ORT)
endif()

install(TARGETS rppg ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/rppg DESTINATION include)
