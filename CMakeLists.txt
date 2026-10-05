cmake_minimum_required(VERSION 3.25)   # 3.25+ required for CMAKE_CXX_STANDARD 23

set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake CACHE STRING "")

project(osheet_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg packages (resolved via the vcpkg toolchain file / manifest mode)
find_package(FFMPEG REQUIRED)                  # module: share/ffmpeg/FindFFMPEG.cmake
find_package(unofficial-skia CONFIG REQUIRED)  # target: unofficial::skia::skia
find_package(indicators CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)

add_executable(${PROJECT_NAME} main.cpp
        metadata.hpp
        metadata.cpp
        indicator_utils.hpp
        indicator_utils.cpp
        ffmpeg_utils.hpp
        ffmpeg_utils.cpp
        tiles.hpp
        tiles.cpp
        sheet.hpp
        sheet.cpp
        )
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
        unofficial::skia::skia
        ${FFMPEG_LIBRARIES}        # avutil, avcodec, avformat, swscale, ... (full paths)
        indicators::indicators
        argparse::argparse
)