cmake_minimum_required(VERSION 3.10)
project(ppg_simd LANGUAGES C CXX)

add_library(ppg_simd SHARED ppg_simd.cpp)

target_compile_features(ppg_simd PRIVATE cxx_std_17)

target_compile_options(ppg_simd PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wno-unused-parameter
)

find_library(log-lib log)

target_link_libraries(ppg_simd PRIVATE ${log-lib})
