set(NSW_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NSW_FORMATS AU)
endif()

juce_add_plugin(NutSweller
    COMPANY_NAME                "NutSweller"
    COMPANY_WEBSITE             "https://github.com/sebasbata0509-glitch/claudio"
    BUNDLE_ID                   "com.nutsweller.nutsweller"
    PLUGIN_MANUFACTURER_CODE    NtSw
    PLUGIN_CODE                 Nsw1
    FORMATS                     ${NSW_FORMATS}
    PRODUCT_NAME                "NutSweller"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
    VST3_CATEGORIES             Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD     FALSE
    NEEDS_WEB_BROWSER           FALSE
    NEEDS_CURL                  FALSE)

target_sources(NutSweller PRIVATE
    LookAndFeel.cpp
    Parameters.cpp
    PluginEditor.cpp
    PluginProcessor.cpp
    Widgets.cpp)

target_include_directories(NutSweller PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(NutSweller PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(NutSweller
    PRIVATE
        nutsweller_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
