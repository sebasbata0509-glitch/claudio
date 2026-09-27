cmake_minimum_required(VERSION 3.22)

# Windows: static MSVC runtime, so users never need a redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# macOS (only relevant if you ever build AU): universal binary.
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Nebula VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(NEBULA_BUILD_TESTS "Build the headless test runner" ON)
option(NEBULA_WERROR "Treat warnings in Nebula's own code as errors" OFF)
option(NEBULA_COPY_AFTER_BUILD "Copy the VST3 into the system plugin folder after building (needs admin on Windows)" OFF)

include(FetchContent)
# Use a local checkout with -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# All processor/DSP/UI sources live in an INTERFACE library so that both the
# plugin and the headless test runner compile exactly the same code.
add_library(nebula_sources INTERFACE)
file(GLOB_RECURSE NEBULA_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
target_sources(nebula_sources INTERFACE ${NEBULA_SRC})
target_include_directories(nebula_sources INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(nebula_sources INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    NEBULA_VERSION_STRING="${PROJECT_VERSION}")

set(NEBULA_LIBS
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

set(NEBULA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NEBULA_FORMATS AU)
endif()

juce_add_plugin(Nebula
    COMPANY_NAME                "Nebula Audio"
    COMPANY_WEBSITE             "https://github.com/sebasbata0509-glitch/claudio"
    BUNDLE_ID                   "com.nebulaaudio.nebula"
    PLUGIN_MANUFACTURER_CODE    NbAu
    PLUGIN_CODE                 Nbl1
    FORMATS                     ${NEBULA_FORMATS}
    PRODUCT_NAME                "Nebula"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Synth
    COPY_PLUGIN_AFTER_BUILD     ${NEBULA_COPY_AFTER_BUILD}
    NEEDS_WEB_BROWSER           FALSE
    NEEDS_CURL                  FALSE)

target_link_libraries(Nebula
    PRIVATE
        nebula_sources
        ${NEBULA_LIBS}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# GCC: run LTO code generation in parallel (otherwise lto-wrapper warns and links serially).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(Nebula INTERFACE -flto=auto)
endif()

if(NEBULA_WERROR)
    if(MSVC)
        target_compile_options(Nebula PRIVATE /WX)
    else()
        target_compile_options(Nebula PRIVATE -Werror)
    endif()
endif()

if(NEBULA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
