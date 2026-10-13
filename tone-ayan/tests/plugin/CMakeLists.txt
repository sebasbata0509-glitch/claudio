# Headless tests of the plugin layer (processor, capture, state, editor) built from the same
# sources as the plugin, with ARA compiled out.
juce_add_console_app(ToneAyanPluginTests PRODUCT_NAME "ToneAyanPluginTests")
target_sources(ToneAyanPluginTests PRIVATE PluginTests.cpp)
target_include_directories(ToneAyanPluginTests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_compile_definitions(ToneAyanPluginTests PRIVATE
    JucePlugin_Name="Tone-ayan"
    JucePlugin_Enable_ARA=0
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(ToneAyanPluginTests
    PRIVATE
        tone_plugin_sources
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
add_test(NAME ToneAyanPluginTests COMMAND ToneAyanPluginTests)
