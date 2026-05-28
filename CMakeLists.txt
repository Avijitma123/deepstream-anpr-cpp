cmake_minimum_required(VERSION 3.16)

project(deepstream_anpr_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(deepstream-anpr
    src/main.cpp
    src/pipeline_builder.cpp
    src/metadata_probe.cpp
    src/plate_cropper.cpp
    src/ocr_engine.cpp
    src/plate_postprocess.cpp
    src/event_manager.cpp
    src/db_writer.cpp
)

target_include_directories(deepstream-anpr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(deepstream-anpr PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set(DEEPSTREAM_ROOT /opt/nvidia/deepstream/deepstream-9.0 CACHE PATH "DeepStream installation root")
set(CUDA_ROOT /usr/local/cuda CACHE PATH "CUDA installation root")

add_library(nvdsinfer_custom_yolo_plate SHARED
    src/yolo_plate_parser.cpp
)

target_include_directories(nvdsinfer_custom_yolo_plate PRIVATE
    ${DEEPSTREAM_ROOT}/sources/includes
    ${CUDA_ROOT}/targets/x86_64-linux/include
)

target_compile_options(nvdsinfer_custom_yolo_plate PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS deepstream-anpr RUNTIME DESTINATION bin)
