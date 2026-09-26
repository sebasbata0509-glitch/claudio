add_library(nutsweller_dsp STATIC
    Analyzer.cpp
    Engine.cpp
    Modulation.cpp
    PitchDetector.cpp
    Synth.cpp)

target_include_directories(nutsweller_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(nutsweller_dsp PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(nutsweller_dsp PRIVATE /W4 /fp:fast)
else()
    target_compile_options(nutsweller_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter
        $<$<CONFIG:Release>:-O3>)
endif()
