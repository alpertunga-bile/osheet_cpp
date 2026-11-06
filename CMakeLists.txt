cmake_minimum_required(VERSION 3.26)   # 3.25+ required for CMAKE_CXX_STANDARD 23

# ---- vcpkg toolchain (cross-platform dependency resolution) ----------------
# Every third-party dep (skia, ffmpeg, indicators, argparse) is resolved by the
# vcpkg manifest toolchain. VCPKG_ROOT must point at your vcpkg checkout.
if(NOT DEFINED VCPKG_ROOT)
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    else()
        message(FATAL_ERROR
            "VCPKG_ROOT is required; export it before configuring "
            "(e.g. export VCPKG_ROOT=$HOME/.local/share/vcpkg).")
    endif()
endif()
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "")

project(osheet_cpp VERSION 0.1.0 LANGUAGES CXX)

# Static vcpkg triplets (e.g. x64-windows-static) build the deps with /MT, so
# our TUs must match or the link fails with LNK2038 (RuntimeLibrary mismatch).
# The toolchain does not set this because it is loaded from this file.
if(MSVC AND VCPKG_TARGET_TRIPLET MATCHES "static" AND NOT VCPKG_TARGET_TRIPLET MATCHES "static-md")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Single-config generators with no explicit type would otherwise get none of
# the Release flags below.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- C++ standard ----------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Sanity checks (fail fast at configure time) ---------------------------
# 1. Compiler version floor for C++23.
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
    message(FATAL_ERROR
        "osheet needs GCC >= 12 for C++23 (found ${CMAKE_CXX_COMPILER_VERSION}).")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
    message(FATAL_ERROR
        "osheet needs Clang >= 13 for C++23 (found ${CMAKE_CXX_COMPILER_VERSION}).")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.38)
    message(FATAL_ERROR
        "osheet needs MSVC >= 19.38 for C++23 (found ${CMAKE_CXX_COMPILER_VERSION}).")
endif()

# 2. Every source in the build list must exist (guards against a stale list).
set(OSHEET_SOURCES
    main.cpp
    src/metadata.cpp
    src/indicator_utils.cpp
    src/ffmpeg_utils.cpp
    src/tiles.cpp
    src/sheet.cpp
)

# ---- Build-type flags (per-compiler, cross-platform) -----------------------
# Portable baseline only (no -march=native): the binary must run on every
# desktop CPU, since CI builds are distributed.
# CMake's own Release defaults already give -O3 (GNU/Clang) and /O2 /Ob2 (MSVC),
# so only the arch baseline is added here. arm64: NEON is in the baseline.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|amd64)")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -march=x86-64-v2")
endif()

# Sanitizers (Debug only, opt-in). MSVC supports ASan only (no UBSan).
option(OSHEET_DEBUG_SANITIZE "Enable AddressSanitizer in Debug builds" OFF)
if(OSHEET_DEBUG_SANITIZE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        string(APPEND CMAKE_CXX_FLAGS_DEBUG " /fsanitize=address")
    else()
        string(APPEND CMAKE_CXX_FLAGS_DEBUG
               " -fno-omit-frame-pointer -fsanitize=address,undefined")
    endif()
endif()

# ---- Warnings (all configs; MSVC uses /W4) --------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /bigobj)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---- Performance / memory options ------------------------------------------
option(OSHEET_BUILD_LTO      "Link-time optimization" ON)

# ---- Dependencies (cross-platform via vcpkg) -------------------------------
find_package(FFMPEG REQUIRED)                  # vcpkg module: FindFFMPEG.cmake
find_package(unofficial-skia CONFIG REQUIRED)  # target: unofficial::skia::skia
find_package(indicators CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)

# ---- Target ----------------------------------------------------------------
add_executable(${PROJECT_NAME} ${OSHEET_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})

# Skia is built with -fno-rtti, so our TUs must match: otherwise they emit
# references to typeinfo for Sk* classes that Skia never exports (link error).
# MSVC uses /GR- for the same effect. Cross-platform, applied to all builds.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /GR-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti)
endif()

# ---- Dead-code / symbol stripping (Release) --------------------------------
# Compile side: per-function/data sections let the linker drop unused code.
# Link side must go through target_link_options: on MSVC, CMake calls link.exe
# directly, so linker flags inside CMAKE_CXX_FLAGS would only reach cl.exe.
if(MSVC)
    set(_osheet_compile_flags /Gy /Gw)
    set(_osheet_link_flags    /OPT:REF /OPT:ICF /INCREMENTAL:NO)
elseif(APPLE)
    set(_osheet_compile_flags -ffunction-sections -fdata-sections)
    set(_osheet_link_flags    -Wl,-dead_strip -Wl,-x -Wl,-S)
else()
    # --strip-all drops .symtab and .debug_* that the static Skia/FFmpeg libs
    # carry (~100 MB); Windows/macOS keep those out of the binary already.
    set(_osheet_compile_flags -ffunction-sections -fdata-sections)
    set(_osheet_link_flags    -Wl,--gc-sections -Wl,--as-needed -Wl,--strip-all)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Release>:${_osheet_compile_flags}>")
target_link_options(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Release>:${_osheet_link_flags}>")

target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial::skia::skia
    ${FFMPEG_LIBRARIES}
    indicators::indicators
    argparse::argparse
)

# ---- LTO (per target, must come after add_executable) ----------------------
if(OSHEET_BUILD_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT OSHEET_HAVE_IPO LANGUAGES CXX)
    if(OSHEET_HAVE_IPO)
        # Per-config: works for multi-config generators too.
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()
