set(TONEAYAN_FORMATS VST3 Standalone)

if(TONEAYAN_ENABLE_ARA)
    set(TONEAYAN_ARA_ARGS
        IS_ARA_EFFECT TRUE
        ARA_TRANSFORMATION_FLAGS kARAPlaybackTransformationNoChanges)
endif()

juce_add_plugin(ToneAyan
    PRODUCT_NAME                "Tone-ayan"
    COMPANY_NAME                "Tone-ayan"
    COMPANY_WEBSITE             "https://github.com/sebasbata0509-glitch/claudio"
    BUNDLE_ID                   "com.toneayan.toneayan"
    PLUGIN_MANUFACTURER_CODE    Tnay
    PLUGIN_CODE                 Tay1
    FORMATS                     ${TONEAYAN_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx Pitch Shift
    COPY_PLUGIN_AFTER_BUILD     ${TONEAYAN_COPY_AFTER_BUILD}
    NEEDS_WEB_BROWSER           FALSE
    NEEDS_CURL                  FALSE
    ${TONEAYAN_ARA_ARGS})

# Plugin + UI sources live in an INTERFACE library so the plugin and the headless plugin test
# runner (tests/plugin) compile exactly the same code.
add_library(tone_plugin_sources INTERFACE)
target_sources(tone_plugin_sources INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CaptureRecorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateIO.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ara/ToneARA.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ToneLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PianoRoll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/Inspector.cpp)
target_include_directories(tone_plugin_sources INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(tone_plugin_sources INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(tone_plugin_sources INTERFACE tone::core juce::juce_audio_utils juce::juce_audio_formats)
if(MSVC)
    target_compile_options(tone_plugin_sources INTERFACE /utf-8)
endif()

target_link_libraries(ToneAyan
    PRIVATE
        tone_plugin_sources
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ToneAyan INTERFACE -flto=auto)
endif()
if(TONEAYAN_WERROR)
    if(MSVC)
        target_compile_options(ToneAyan PRIVATE /WX)
    else()
        target_compile_options(ToneAyan PRIVATE -Werror)
    endif()
endif()
