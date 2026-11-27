cmake_minimum_required(VERSION 3.22)

project(FormulaCompressor VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE 8.0.15, vendored locally in ThirdParty/ (extracted from the release zip).
add_subdirectory(ThirdParty/JUCE-8.0.15/JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(FormulaCompressor
    PRODUCT_NAME            "Formula Compressor"
    VERSION                 ${PROJECT_VERSION}
    COMPANY_NAME            "Nejiwwkr"
    BUNDLE_ID               "com.nejiwwkr.formulacompressor"
    PLUGIN_MANUFACTURER_CODE Njwk
    PLUGIN_CODE             Fmc1
    FORMATS                 VST3 Standalone
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx Dynamics
)

target_sources(FormulaCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/Expression.cpp
    Source/dsp/Expression.h
    Source/dsp/ExpressionParser.cpp
    Source/dsp/ExpressionParser.h
    Source/dsp/CompressorEngine.cpp
    Source/dsp/CompressorEngine.h
    Source/ui/Theme.h
    Source/ui/FormulaLookAndFeel.cpp
    Source/ui/FormulaLookAndFeel.h
    Source/ui/FormulaCurveEditor.cpp
    Source/ui/FormulaCurveEditor.h
    Source/ui/LevelMeter.cpp
    Source/ui/LevelMeter.h
    Source/ui/ScopePanel.cpp
    Source/ui/ScopePanel.h
    Source/state/PresetManager.cpp
    Source/state/PresetManager.h
)

target_compile_definitions(FormulaCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(FormulaCompressor
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Offline test app: drives the real parser, engine and AudioProcessor without a DAW.
juce_add_console_app(FormulaCompressorTests PRODUCT_NAME "FormulaCompressorTests")

target_sources(FormulaCompressorTests PRIVATE
    Tests/test_main.cpp
    Tests/TestFramework.h
    Tests/expression_tests.cpp
    Tests/engine_tests.cpp
    Tests/processor_tests.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/Expression.cpp
    Source/dsp/Expression.h
    Source/dsp/ExpressionParser.cpp
    Source/dsp/ExpressionParser.h
    Source/dsp/CompressorEngine.cpp
    Source/dsp/CompressorEngine.h
    Source/ui/FormulaLookAndFeel.cpp
    Source/ui/FormulaLookAndFeel.h
    Source/ui/FormulaCurveEditor.cpp
    Source/ui/FormulaCurveEditor.h
    Source/ui/LevelMeter.cpp
    Source/ui/LevelMeter.h
    Source/ui/ScopePanel.cpp
    Source/ui/ScopePanel.h
    Source/state/PresetManager.cpp
    Source/state/PresetManager.h
)

target_include_directories(FormulaCompressorTests PRIVATE Source)

target_compile_definitions(FormulaCompressorTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    "JucePlugin_Name=\"Formula Compressor\""
)

target_link_libraries(FormulaCompressorTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
