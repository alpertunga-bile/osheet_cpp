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
# OSHET_NATIVE=ON optimizes for the CPU that builds (the binary then needs a
# CPU with at least the same feature set); OFF targets every desktop CPU.
option(OSHEET_NATIVE "Optimize for the build machine's CPU" ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # /O2 is already in MSVC's default Release flags; /Ob2 inlines more.
    set(_osheet_opt_flag "/Ob2")
elseif(OSHEET_NATIVE)
    set(_osheet_opt_flag "-O3 -march=native")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|amd64)")
        set(_osheet_opt_flag "-O3 -march=x86-64-v2")
    else()
        set(_osheet_opt_flag "-O3")  # aarch64: NEON is in the baseline
    endif()
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${_osheet_opt_flag}")

# Sanitizers (Debug) + frame pointer: MSVC uses /fsanitize and always has frame pointers.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_osheet_debug_flags "/fsanitize=address,undefined")
else()
    set(_osheet_debug_flags "-fno-omit-frame-pointer -fsanitize=address,undefined")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${_osheet_debug_flags}")

# Dead-code stripping (Release): GNU, Apple, and MSVC all use different syntax.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_osheet_gc_flags "/OPT:REF /INCREMENTAL:NO")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(_osheet_gc_flags "-ffunction-sections -fdata-sections -Wl,-dead_strip")
else()
    set(_osheet_gc_flags "-ffunction-sections -Wl,--gc-sections")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${_osheet_gc_flags}")

# ---- Warnings (all configs; MSVC uses /W4) --------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /bigobj)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---- Optional debug sanitizer ----------------------------------------------
option(OSHEET_DEBUG_SANITIZE "Enable AddressSanitizer in Debug builds" OFF)
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

target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial::skia::skia
    ${FFMPEG_LIBRARIES}
    indicators::indicators
    argparse::argparse
)

# ---- LTO (per target, must come after add_executable) ----------------------
if(OSHEET_BUILD_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT OSHEET_HAVE_IPO LANGUAGES CXX)
    if(OSHEET_HAVE_IPO)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()
