cmake_minimum_required(VERSION 3.15)
project(heartpy_rn)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Use absolute paths to avoid confusion
add_library(heartpy_rn SHARED
    native_analyze.cpp
    /Users/adilyoltay/Desktop/heartpy-clean/cpp/heartpy_core.cpp
    /Users/adilyoltay/Desktop/heartpy-clean/cpp/heartpy_stream.cpp
    /Users/adilyoltay/Desktop/heartpy-clean/react-native-heartpy/cpp/rn_options_builder.cpp
)

target_include_directories(heartpy_rn PRIVATE
  /Users/adilyoltay/Desktop/heartpy-clean/cpp
  /Users/adilyoltay/Desktop/heartpy-clean/third_party/kissfft
  /Users/adilyoltay/Desktop/heartpy-clean/react-native-heartpy/cpp
  ${REACT_NATIVE_DIR}/ReactCommon
  ${REACT_NATIVE_DIR}/ReactCommon/jsi
)

# Use vendored KissFFT
if(EXISTS /Users/adilyoltay/Desktop/heartpy-clean/third_party/kissfft/kiss_fft.c AND EXISTS /Users/adilyoltay/Desktop/heartpy-clean/third_party/kissfft/kiss_fftr.c)
  add_library(kissfft STATIC
    /Users/adilyoltay/Desktop/heartpy-clean/third_party/kissfft/kiss_fft.c
    /Users/adilyoltay/Desktop/heartpy-clean/third_party/kissfft/kiss_fftr.c)
  target_include_directories(kissfft PUBLIC /Users/adilyoltay/Desktop/heartpy-clean/third_party/kissfft)
  target_compile_definitions(heartpy_rn PRIVATE USE_KISSFFT=1)
  target_link_libraries(heartpy_rn kissfft)
endif()

target_link_libraries(heartpy_rn android log)
