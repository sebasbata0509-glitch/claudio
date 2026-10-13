cmake_minimum_required(VERSION 3.22)

# Windows: static MSVC runtime so the VST3 needs no redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(ToneAyan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TONEAYAN_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" ON)
option(TONEAYAN_BUILD_TESTS  "Build unit tests and the command-line tool" ON)
option(TONEAYAN_ENABLE_ARA   "Build with ARA2 support" ON)
option(TONEAYAN_WERROR       "Treat warnings in Tone-ayan's own code as errors" OFF)
option(TONEAYAN_COPY_AFTER_BUILD "Copy the VST3 into the system VST3 folder after building (needs admin on Windows)" OFF)

include(cmake/Dependencies.cmake)

# JUCE-free core: common, model, analysis and DSP engine. Tests and the CLI only need this.
add_subdirectory(src/core)

if(TONEAYAN_BUILD_PLUGIN)
    add_subdirectory(src/plugin)
endif()

if(TONEAYAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(tools)
endif()
