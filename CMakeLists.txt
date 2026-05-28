cmake_minimum_required(VERSION 3.16)

project(deepstream_anpr_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DEEPSTREAM_ROOT /opt/nvidia/deepstream/deepstream-9.0 CACHE PATH "DeepStream installation root")
set(CUDA_ROOT /usr/local/cuda CACHE PATH "CUDA installation root")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0)

add_executable(deepstream-anpr
    src/main.cpp
    src/full_pipeline.cpp
    src/pipeline_builder.cpp
    src/plate_cropper.cpp
    src/plate_postprocess.cpp
    src/event_manager.cpp
    src/db_writer.cpp
)

add_executable(deepstream-anpr-ocr
    src/ocr_cli.cpp
    src/ocr_engine.cpp
)

target_include_directories(deepstream-anpr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GSTREAMER_INCLUDE_DIRS}
    ${DEEPSTREAM_ROOT}/sources/includes
    ${CUDA_ROOT}/targets/x86_64-linux/include
)

target_include_directories(deepstream-anpr-ocr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CUDA_ROOT}/targets/x86_64-linux/include
)

target_link_directories(deepstream-anpr PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
    ${CUDA_ROOT}/lib64
    ${DEEPSTREAM_ROOT}/lib
)

target_link_directories(deepstream-anpr-ocr PRIVATE
    ${CUDA_ROOT}/lib64
)

target_compile_options(deepstream-anpr PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_compile_options(deepstream-anpr-ocr PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(deepstream-anpr PRIVATE
    ${GSTREAMER_LIBRARIES}
    cuda
    cudart
    nvds_meta
    nvdsgst_meta
    nvds_batch_jpegenc
    nvbufsurface
)

target_link_libraries(deepstream-anpr-ocr PRIVATE
    nvinfer
    cuda
    cudart
)

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
