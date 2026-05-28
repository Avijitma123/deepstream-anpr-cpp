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

install(TARGETS deepstream-anpr RUNTIME DESTINATION bin)
