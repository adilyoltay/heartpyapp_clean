cmake_minimum_required(VERSION 3.15)
project(heartpy_rn)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Resolve the repository root relative to this CMakeLists.
get_filename_component(HEARTPY_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../" REALPATH)
set(HEARTPY_CPP_DIR "${HEARTPY_REPO_ROOT}/cpp")
set(HEARTPY_MODULE_CPP_DIR "${HEARTPY_REPO_ROOT}/react-native-heartpy/cpp")
set(HEARTPY_ANDROID_CPP_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(HEARTPY_THIRD_PARTY_DIR "${HEARTPY_REPO_ROOT}/third_party")

set(HEARTPY_SOURCES
    "${HEARTPY_ANDROID_CPP_DIR}/native_analyze.cpp"
    "${HEARTPY_CPP_DIR}/heartpy_core.cpp"
    "${HEARTPY_CPP_DIR}/heartpy_stream.cpp"
    "${HEARTPY_MODULE_CPP_DIR}/rn_options_builder.cpp"
)

set(HEARTPY_GENERATED_CPP_DIR "${HEARTPY_MODULE_CPP_DIR}/generated")
if(EXISTS "${HEARTPY_GENERATED_CPP_DIR}")
    file(GLOB HEARTPY_GENERATED_SOURCES "${HEARTPY_GENERATED_CPP_DIR}/*.cpp" "${HEARTPY_GENERATED_CPP_DIR}/*.cxx")
    list(APPEND HEARTPY_SOURCES ${HEARTPY_GENERATED_SOURCES})
endif()

add_library(heartpy_rn SHARED ${HEARTPY_SOURCES})

target_include_directories(heartpy_rn PRIVATE
    "${HEARTPY_CPP_DIR}"
    "${HEARTPY_THIRD_PARTY_DIR}/kissfft"
    "${HEARTPY_MODULE_CPP_DIR}"
    "${HEARTPY_GENERATED_CPP_DIR}"
    "${REACT_NATIVE_DIR}/ReactCommon"
    "${REACT_NATIVE_DIR}/ReactCommon/jsi"
)

set(KISSFFT_DIR "${HEARTPY_THIRD_PARTY_DIR}/kissfft")
if(EXISTS "${KISSFFT_DIR}/kiss_fft.c" AND EXISTS "${KISSFFT_DIR}/kiss_fftr.c")
    add_library(kissfft STATIC
        "${KISSFFT_DIR}/kiss_fft.c"
        "${KISSFFT_DIR}/kiss_fftr.c"
    )
    target_include_directories(kissfft PUBLIC "${KISSFFT_DIR}")
    target_compile_definitions(heartpy_rn PRIVATE USE_KISSFFT=1)
    target_link_libraries(heartpy_rn PRIVATE kissfft)
endif()

target_link_libraries(heartpy_rn
    PRIVATE
        android
        log
)
