cmake_minimum_required(VERSION 3.15)
project(heartpy_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(heartpy_core STATIC
    cpp/heartpy_core.cpp
    cpp/heartpy_stream.cpp
)

target_include_directories(heartpy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kissfft
)

if(APPLE)
    # Always link Accelerate because FFT path uses vDSP when available
    target_link_libraries(heartpy_core PRIVATE "-framework Accelerate")
    target_compile_definitions(heartpy_core PRIVATE USE_ACCELERATE_FFT=1)
    if(HEARTPY_ENABLE_ACCELERATE)
        target_compile_definitions(heartpy_core PRIVATE HEARTPY_ENABLE_ACCELERATE=1)
    endif()
endif()

if(HEARTPY_ENABLE_NEON)
    target_compile_definitions(heartpy_core PRIVATE HEARTPY_ENABLE_NEON=1)
endif()
option(USE_KISSFFT "Use KissFFT if available" ON)
if(USE_KISSFFT)
    # Prefer vendored kissfft if present
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kissfft/kiss_fft.c AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kissfft/kiss_fftr.c)
        message(STATUS "Using vendored KissFFT")
        target_sources(heartpy_core PRIVATE
            third_party/kissfft/kiss_fft.c
            third_party/kissfft/kiss_fftr.c)
        target_compile_definitions(heartpy_core PRIVATE USE_KISSFFT=1)
    else()
        find_path(KISSFFT_INCLUDE_DIR kiss_fftr.h)
        find_library(KISSFFT_LIB NAMES kissfft kissfft-float)
        if(KISSFFT_INCLUDE_DIR AND KISSFFT_LIB)
            message(STATUS "Using system KissFFT: ${KISSFFT_INCLUDE_DIR}")
            target_include_directories(heartpy_core PRIVATE ${KISSFFT_INCLUDE_DIR})
            target_link_libraries(heartpy_core PRIVATE ${KISSFFT_LIB})
            target_compile_definitions(heartpy_core PRIVATE USE_KISSFFT=1)
        else()
            message(WARNING "KissFFT not found; falling back to internal FFT")
        endif()
    endif()
endif()

if(MSVC)
    target_compile_options(heartpy_core PRIVATE /W4)
else()
    target_compile_options(heartpy_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_compile_definitions(heartpy_core PRIVATE HEARTPY_LOCK_TIMING=1)

# Optional example executable (can be expanded later)
add_executable(heartpy_example examples/example_main.cpp)
target_link_libraries(heartpy_example PRIVATE heartpy_core)

# MIT-BIH RR validation tool
add_executable(validate_rr_intervals examples/validate_rr_intervals.cpp)
target_link_libraries(validate_rr_intervals PRIVATE heartpy_core)

# Smoke test executable for CI/local validation
add_executable(heartpy_smoke examples/smoke_test.cpp)
target_link_libraries(heartpy_smoke PRIVATE heartpy_core)

add_executable(heartpy_compare_cpp examples/compare_cpp.cpp)
target_link_libraries(heartpy_compare_cpp PRIVATE heartpy_core)

add_executable(heartpy_compare_json examples/compare_cpp_json.cpp)
target_link_libraries(heartpy_compare_json PRIVATE heartpy_core)

add_executable(heartpy_compare_file_json examples/compare_file_json.cpp)
target_link_libraries(heartpy_compare_file_json PRIVATE heartpy_core)

add_executable(heartpy_compare_rr_json examples/compare_rr_json.cpp)
target_link_libraries(heartpy_compare_rr_json PRIVATE heartpy_core)
add_executable(realtime_demo examples/realtime_demo.cpp)
target_link_libraries(realtime_demo PRIVATE heartpy_core)

# Concurrency smoke test (push/poll on separate threads for a short duration)
add_executable(concurrency_smoke examples/concurrency_smoke.cpp)
target_link_libraries(concurrency_smoke PRIVATE heartpy_core)

# Simple PSD benchmark (optional)
add_executable(bench_filter_psd examples/bench_filter_psd.cpp)
target_link_libraries(bench_filter_psd PRIVATE heartpy_core)

# Poll latency benchmark (ring ON/OFF)
add_executable(bench_poll_latency examples/bench_poll_latency.cpp)
target_link_libraries(bench_poll_latency PRIVATE heartpy_core)
target_compile_definitions(bench_poll_latency PRIVATE HEARTPY_LOCK_TIMING=1)

add_executable(welch_psd_adaptive_test examples/welch_psd_adaptive_test.cpp)
target_link_libraries(welch_psd_adaptive_test PRIVATE heartpy_core)

# calcFreq off smoke test
add_executable(calcfreq_off_test examples/calcfreq_off_test.cpp)
target_link_libraries(calcfreq_off_test PRIVATE heartpy_core)

# threshold_rr mask test
add_executable(threshold_rr_mask_test examples/threshold_rr_mask_test.cpp)
target_link_libraries(threshold_rr_mask_test PRIVATE heartpy_core)

# Acceptance check helper target (requires python3)
add_custom_target(acceptance
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_acceptance.py --build-dir ${CMAKE_BINARY_DIR} --preset both --fs 50 --duration 180 --fast
  DEPENDS realtime_demo
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Running acceptance checks (torch + ambient)"
)

enable_testing()
option(HEARTPY_ENABLE_ACCELERATE "Use Apple Accelerate/vDSP where available" ON)
option(HEARTPY_ENABLE_NEON "Enable ARM NEON intrinsics" OFF)
add_test(NAME acceptance_torch
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_acceptance.py --build-dir ${CMAKE_BINARY_DIR} --preset torch --fs 50 --duration 60 --fast --hr-tol 100
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME acceptance_ambient
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_acceptance.py --build-dir ${CMAKE_BINARY_DIR} --preset ambient --fs 50 --duration 60 --fast --hr-tol 100
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME acceptance_torch_180
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_acceptance.py --build-dir ${CMAKE_BINARY_DIR} --preset torch --fs 50 --duration 180 --fast
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME acceptance_ambient_180
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_acceptance.py --build-dir ${CMAKE_BINARY_DIR} --preset ambient --fs 50 --duration 180 --fast
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME concurrency_smoke
  COMMAND ${CMAKE_BINARY_DIR}/concurrency_smoke
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME welch_psd_adaptive_test
  COMMAND ${CMAKE_BINARY_DIR}/welch_psd_adaptive_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME calcfreq_off_test
  COMMAND ${CMAKE_BINARY_DIR}/calcfreq_off_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME threshold_rr_mask_test
  COMMAND ${CMAKE_BINARY_DIR}/threshold_rr_mask_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
