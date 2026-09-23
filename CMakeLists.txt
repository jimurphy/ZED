cmake_minimum_required(VERSION 3.22)

# Set macOS defaults before compiler detection; leave Windows/MSVC untouched.
if(CMAKE_HOST_APPLE AND (NOT DEFINED CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "ZED macOS architecture")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "ZED minimum macOS version")
endif()

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

set(ZED_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND ZED_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(ZED
    VERSION "${PROJECT_VERSION}"
    COMPANY_NAME "South Coast Synthesis"
    COMPANY_COPYRIGHT "southcoastsynthesis"
    BUNDLE_ID com.SouthCoastSynthesis.ZED
    PLUGIN_MANUFACTURER_CODE Soco
    PLUGIN_CODE Zedd
    PRODUCT_NAME "ZED"
    DESCRIPTION "ZED"
    FORMATS ${ZED_PLUGIN_FORMATS}
    VST3_CATEGORIES Fx Filter
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_EXPORT_PREFIX ZEDAU
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
if(MSVC)
    # Expose M_PI before math headers, including in tests using ZED's DSP headers.
    target_compile_definitions(ZED PUBLIC _USE_MATH_DEFINES)
endif()
# JUCE 8.0.6 has no historical splash screen. JUCE_DISPLAY_SPLASH_SCREEN is
# obsolete in this pinned release and defining it emits an ignored-flag warning.
target_link_libraries(ZED PRIVATE
    ZEDBinaryData
    juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
)

if(APPLE)
    # Seal both completed bundles for local native testing. JUCE writes the
    # VST3 manifest after its signing step; AU otherwise has a linker-only seal.
    # Ad-hoc only: no signing identity, timestamp or notarisation.
    foreach(zed_format IN ITEMS VST3 AU)
        add_custom_command(TARGET ZED_${zed_format} POST_BUILD
            COMMAND /usr/bin/codesign --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:ZED_${zed_format}>"
            VERBATIM
        )
    endforeach()
endif()

option(ZED_BUILD_TESTS "Build the focused channel-layout regression test" OFF)
if(ZED_BUILD_TESTS)
    enable_testing()
    add_executable(ZEDChannelTests Tests/ChannelLayouts.cpp Tests/ConfigurationTests.cpp Tests/LifecycleTests.cpp Tests/TopologyTests.cpp)
    set_target_properties(ZEDChannelTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ZEDChannelTests_artefacts/$<CONFIG>"
    )
    target_include_directories(ZEDChannelTests PRIVATE
        Source Source/jdsplib
        "$<TARGET_PROPERTY:ZED,JUCE_GENERATED_SOURCES_DIRECTORY>"
    )
    target_link_libraries(ZEDChannelTests PRIVATE ZED ZEDBinaryData juce::juce_audio_utils)
    add_test(NAME ZEDChannelLayouts COMMAND ZEDChannelTests)

    # Measurement-only executable. Its test access uses existing private friendship;
    # neither production sources nor normal plug-in target options are changed.
    add_executable(ZEDStabilityCharacterisation Tests/StabilityCharacterisation.cpp)
    target_include_directories(ZEDStabilityCharacterisation PRIVATE
        Source Source/jdsplib
        "$<TARGET_PROPERTY:ZED,JUCE_GENERATED_SOURCES_DIRECTORY>"
    )
    target_link_libraries(ZEDStabilityCharacterisation PRIVATE ZED ZEDBinaryData juce::juce_audio_utils)
    add_test(NAME ZEDStabilityToolSmoke COMMAND ZEDStabilityCharacterisation
        --mode smoke --output "${CMAKE_CURRENT_BINARY_DIR}/stability-smoke")
endif()
