cmake_minimum_required(VERSION 3.22)

project(ZED VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    # JUCE 8.0.6: pin the release commit so the dependency cannot drift.
    GIT_TAG 51a8a6d7aeae7326956d747737ccf1575e61e209
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ZED
    COMPANY_NAME "South Coast Synthesis"
    COMPANY_COPYRIGHT "southcoastsynthesis"
    BUNDLE_ID com.SouthCoastSynthesis.ZED
    PLUGIN_MANUFACTURER_CODE Soco
    PLUGIN_CODE Zedd
    PRODUCT_NAME "ZED"
    DESCRIPTION "ZED"
    FORMATS VST3
    VST3_CATEGORIES Fx Filter
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_add_binary_data(ZEDBinaryData SOURCES Source/Galvji.ttc)
juce_generate_juce_header(ZED)

target_sources(ZED PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/FilterSpline.cpp
)
target_include_directories(ZED PRIVATE Source/jdsplib)
target_compile_definitions(ZED PUBLIC
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(ZED PRIVATE
    ZEDBinaryData
    juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
)

if(APPLE)
    # JUCE 8.0.6 writes moduleinfo.json after its signing step. Seal the final
    # bundle, including that manifest, for local native macOS host testing.
    add_custom_command(TARGET ZED_VST3 POST_BUILD
        COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:ZED_VST3>"
        VERBATIM
    )
endif()

option(ZED_BUILD_TESTS "Build the focused channel-layout regression test" OFF)
if(ZED_BUILD_TESTS)
    enable_testing()
    add_executable(ZEDChannelTests Tests/ChannelLayouts.cpp)
    set_target_properties(ZEDChannelTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ZEDChannelTests_artefacts/$<CONFIG>"
    )
    target_include_directories(ZEDChannelTests PRIVATE
        Source Source/jdsplib
        "$<TARGET_PROPERTY:ZED,JUCE_GENERATED_SOURCES_DIRECTORY>"
    )
    target_link_libraries(ZEDChannelTests PRIVATE ZED ZEDBinaryData juce::juce_audio_utils)
    add_test(NAME ZEDChannelLayouts COMMAND ZEDChannelTests)
endif()
