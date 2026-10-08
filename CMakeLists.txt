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
# Optimization level: MSVC sets /O2 in its default Release flags and has no -O3.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_osheet_opt_flag "")
else()
    set(_osheet_opt_flag "-O3")
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
    set(_osheet_gc_flags "-ffunction-sections -fdata-sections -Wl,-gc-sections")
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
option(OSHEET_ENABLE_SIMD    "SIMD compiler flags" ON)

# ---- SIMD flags (guarded; safe defaults per arch) --------------------------
if(OSHEET_ENABLE_SIMD)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|amd64)")
            add_compile_options(-msse2)
            include(CheckCXXCompilerFlag)
            check_cxx_compiler_flag("-mavx2" OSHEET_HAVE_AVX2)
            if(OSHEET_HAVE_AVX2)
                add_compile_options(-mavx2)
            endif()
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64|arm64)")
            add_compile_options(-DARM_NEON)
        endif()
    endif()
endif()

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
    if(COMMAND check_ipo_support)
        include(CheckIPOSupport)
        check_ipo_support(RESULT OSHEET_HAVE_IPO)
    else()
        set(OSHEET_HAVE_IPO ON)
    endif()
    if(OSHEET_HAVE_IPO)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()
