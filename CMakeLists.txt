cmake_minimum_required(VERSION 3.24)

project(FormulaCompressor VERSION 1.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

# JUCE 8.0.15 is pinned to a release tarball so a fresh clone can configure
# without a manual download. A local checkout can be supplied with
# -DFORMULA_JUCE_PATH=... or by keeping ThirdParty/JUCE-8.0.15 in place.
set(FORMULA_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")

if (FORMULA_JUCE_PATH)
    add_subdirectory("${FORMULA_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE-8.0.15/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE-8.0.15/JUCE"
                     "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    # Tag 8.0.15 resolves to commit 91ad83ae34a81e0833b1a2b0866f54846370ae53.
    FetchContent_Declare(JUCE
        URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/8.0.15
        URL_HASH SHA256=0eafd736286c6e7bb6958d79fce18d42cb654b8e231890d327b7cece379bd156
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

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
