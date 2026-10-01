cmake_minimum_required(VERSION 3.22)
project(NFGlue VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS Universal Binary")
option(NFGlue_FETCH_JUCE "Fetch JUCE when not installed" ON)
option(NFGlue_ENABLE_AAX "Build AAX (licensed SDK required)" OFF)
# The AAX SDK is expected in ~/Documents/AAX_SDK (owner's Mac). Override with -DNFGlue_AAX_SDK_PATH=...
# (the Windows script does this from the AAX_SDK_PATH environment variable).
if(WIN32)
 set(_nfglue_home "$ENV{USERPROFILE}")
else()
 set(_nfglue_home "$ENV{HOME}")
endif()
set(NFGlue_AAX_SDK_PATH "${_nfglue_home}/Documents/AAX_SDK" CACHE PATH "Licensed AAX SDK path")

find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
 if(NOT NFGlue_FETCH_JUCE)
  message(FATAL_ERROR "JUCE not found. Set JUCE_DIR or enable NFGlue_FETCH_JUCE.")
 endif()
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
 FetchContent_MakeAvailable(JUCE)
endif()

set(NFGlue_FORMATS VST3)
if(APPLE)
 list(APPEND NFGlue_FORMATS AU)
endif()
if(NFGlue_ENABLE_AAX)
 if(NOT EXISTS "${NFGlue_AAX_SDK_PATH}")
  message(FATAL_ERROR "NFGlue_ENABLE_AAX requires the AAX SDK at NFGlue_AAX_SDK_PATH (${NFGlue_AAX_SDK_PATH}).")
 endif()
 juce_set_aax_sdk_path("${NFGlue_AAX_SDK_PATH}")
 list(APPEND NFGlue_FORMATS AAX)
endif()

juce_add_plugin(NFGlue
 COMPANY_NAME "NF Audio Tools"
 COMPANY_COPYRIGHT "NF Audio Tools by Nenno Fernando. (C) 2026 NF Audio Tools. All rights reserved."
 BUNDLE_ID "com.nfaudiotools.nfglue"
 AAX_IDENTIFIER "com.nfaudiotools.nfglue"
 PLUGIN_MANUFACTURER_CODE Nfat
 PLUGIN_CODE Nfgl
 FORMATS ${NFGlue_FORMATS}
 PRODUCT_NAME "NF Glue"
 PLUGIN_NAME "NF Glue"
 DESCRIPTION "Simple musical compressor"
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
 VST3_CATEGORIES Fx Dynamics Stereo
 AU_MAIN_TYPE kAudioUnitType_Effect
 AAX_CATEGORY AAX_ePlugInCategory_Dynamics)
juce_generate_juce_header(NFGlue)

# Artwork embedded as binary data (header NFGlueBinaryData.h).
file(GLOB NFGlue_READY_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/PNG_READY_1200x400/*.png")
juce_add_binary_data(NFGlueAssets HEADER_NAME NFGlueBinaryData.h NAMESPACE NFGlueBinaryData SOURCES ${NFGlue_READY_ASSETS})
target_sources(NFGlue PRIVATE
 Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h
 Source/DSP/Compressor.h
 Source/UI/ValueCapsule.cpp Source/UI/ValueCapsule.h
 Source/PresetManager.cpp Source/PresetManager.h
 Source/UI/NFGlueLookAndFeel.cpp Source/UI/NFGlueLookAndFeel.h)
target_compile_definitions(NFGlue PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(NFGlue PRIVATE NFGlueAssets juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

add_executable(NFGlueDspTests Tests/CompressorTests.cpp)
if(MSVC)
 target_compile_options(NFGlueDspTests PRIVATE /W4)
else()
 target_compile_options(NFGlueDspTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME NFGlueDspTests COMMAND NFGlueDspTests)
