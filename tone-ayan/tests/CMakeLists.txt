add_executable(tone_tests
    test_fft.cpp
    test_pyin.cpp
    test_segmenter.cpp
    test_pitchmarks.cpp
    test_model.cpp
    test_timemap.cpp
    test_render.cpp)
target_include_directories(tone_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
target_link_libraries(tone_tests PRIVATE tone::core nlohmann_json::nlohmann_json Catch2::Catch2WithMain)
if(MSVC)
    target_compile_options(tone_tests PRIVATE /utf-8 /bigobj)
endif()

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(tone_tests DISCOVERY_MODE PRE_TEST)

if(TONEAYAN_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
