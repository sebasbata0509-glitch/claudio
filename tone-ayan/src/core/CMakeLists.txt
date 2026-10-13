# Four small static libraries with a strict one-way dependency chain:
#   tone_common <- tone_model <- tone_analysis <- tone_engine
# None of them depends on JUCE.

function(tone_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(TONEAYAN_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion)
        if(TONEAYAN_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(tone_common STATIC common/Fft.cpp common/WavFile.cpp)
target_include_directories(tone_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(tone_common PUBLIC Threads::Threads)
tone_warnings(tone_common)

add_library(tone_model STATIC model/Scale.cpp model/Project.cpp model/Edits.cpp model/Serialize.cpp)
target_link_libraries(tone_model PUBLIC tone_common PRIVATE nlohmann_json::nlohmann_json)
tone_warnings(tone_model)

add_library(tone_analysis STATIC
    analysis/PYin.cpp analysis/Onsets.cpp analysis/Segmenter.cpp analysis/PitchMarks.cpp analysis/Analyzer.cpp)
target_link_libraries(tone_analysis PUBLIC tone_model)
tone_warnings(tone_analysis)

add_library(tone_engine STATIC engine/TimeMap.cpp engine/EditModel.cpp engine/Synthesis.cpp engine/Renderer.cpp)
target_link_libraries(tone_engine PUBLIC tone_analysis)
tone_warnings(tone_engine)

add_library(tone::core ALIAS tone_engine)
