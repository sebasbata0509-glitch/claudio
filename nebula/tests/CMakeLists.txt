juce_add_console_app(NebulaTests PRODUCT_NAME "NebulaTests")

target_sources(NebulaTests PRIVATE TestMain.cpp)
file(GLOB NEBULA_TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*Tests.cpp)
target_sources(NebulaTests PRIVATE ${NEBULA_TEST_SRC})


target_link_libraries(NebulaTests
    PRIVATE
        nebula_sources
        ${NEBULA_LIBS}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME NebulaTests COMMAND NebulaTests)
