cmake_minimum_required(VERSION 3.25)   # 3.25+ required for CMAKE_CXX_STANDARD 23

# ---- Project ---------------------------------------------------------------
# CMake is cross-platform by default; the porting work here is removing the
# Linux-only assumptions (hardcoded toolchain path, no sanity checks).
project(osheet_cpp VERSION 0.1.0 LANGUAGES CXX)

# ---- C++ standard ----------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    metadata.hpp metadata.cpp
    indicator_utils.hpp indicator_utils.cpp
    ffmpeg_utils.hpp ffmpeg_utils.cpp
    tiles.hpp tiles.cpp
    sheet.hpp sheet.cpp
)
foreach(src IN LISTS OSHEET_SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(FATAL_ERROR "osheet: missing source file ${src}")
    endif()
endforeach()

# ---- Build-type defaults ---------------------------------------------------
# Default to Release when the user passes no -DCMAKE_BUILD_TYPE.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ---- Per-config flags ------------------------------------------------------
# Debug: keep frame pointers for meaningful backtraces and leave assertions
# active (we never add -DNDEBUG). ASan is optional but is the strongest
# memory sanity check for a debug build.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer")
# Release: aggressive optimization for the image-processing hot loop.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g")

# ---- Warnings (all configs; MSVC uses /W4) --------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /bigobj)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---- Optional debug sanitizer ----------------------------------------------
option(OSHEET_DEBUG_SANITIZE "Enable AddressSanitizer in Debug builds" OFF)
if(OSHEET_DEBUG_SANITIZE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
endif()

# ---- Performance / memory options ------------------------------------------
option(OSHEET_BUILD_LTO      "Link-time optimization" ON)
option(OSHEET_ENABLE_SIMD    "SIMD compiler flags" ON)
option(OSHEET_USE_MIMALLOC   "Use mimalloc allocator (needs mimalloc in vcpkg.json)" OFF)

# ---- SIMD flags (guarded; safe defaults per arch) --------------------------
# ponytail: -msse2 is guaranteed on all x86-64 CPUs. -mavx2 is a known risk on
# x86-64 CPUs older than Haswell (2013); check_cxx_compiler_flag only proves the
# compiler accepts the flag, not that the runtime CPU supports it — hence the
# documented caveat. MSVC auto-enables SIMD per target; it has no -m flags.
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

target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial::skia::skia
    ${FFMPEG_LIBRARIES}
    indicators::indicators
    argparse::argparse
)

# ---- mimalloc (optional) ---------------------------------------------------
# ponytail: enabling this requires adding "mimalloc" to vcpkg.json dependencies.
# Default is OFF so the base build needs no extra vcpkg package.
if(OSHEET_USE_MIMALLOC)
    find_package(mimalloc CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE mimalloc::mimalloc)
endif()

# ---- LTO (per target, must come after add_executable) ----------------------
# ponytail: IPO is safe on this multi-TU executable; gated on check_ipo_support
# so a compiler without IPO fails configure, not build.
if(OSHEET_BUILD_LTO)
    # ponytail: check_ipo_support ships as a CMake module; guard on COMMAND so a
    # partial CMake install (missing the module) still configures. Still gated on
    # IPO support so a compiler without IPO fails configure, not build.
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