cmake_minimum_required(VERSION 3.22)
project(SwellChain VERSION 0.1.0 LANGUAGES C CXX)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(CHAIN_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND CHAIN_FORMATS AU)
endif()
juce_add_plugin(SwellChain
  COMPANY_NAME "Swell-audio"
  BUNDLE_ID "fr.swell-audio.chain"
  PLUGIN_MANUFACTURER_CODE Swel
  PLUGIN_CODE Swch
  FORMATS ${CHAIN_FORMATS}
  PRODUCT_NAME "Swell Chain"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Swell Chain traite ta voix et l'analyse."
  COPY_PLUGIN_AFTER_BUILD FALSE)
juce_add_binary_data(ChainAssets SOURCES swell-mark.png)
target_sources(SwellChain PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(SwellChain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
if(MSVC)
  target_compile_options(SwellChain PRIVATE /utf-8 /bigobj)
endif()
target_link_libraries(SwellChain PRIVATE ChainAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

enable_testing()
juce_add_console_app(ChainTest PRODUCT_NAME "Swell Chain Test")
juce_generate_juce_header(ChainTest)
target_sources(ChainTest PRIVATE Tests/ChainTest.cpp)
target_compile_definitions(ChainTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
if(MSVC)
  target_compile_options(ChainTest PRIVATE /utf-8 /bigobj)
endif()
target_link_libraries(ChainTest PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChainDSP COMMAND ChainTest)
