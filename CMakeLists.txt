cmake_minimum_required(VERSION 3.22)

project(Tessitura VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TESSITURA_BUILD_APP   "Build the desktop application" ON)
option(TESSITURA_BUILD_TESTS "Build the headless engine tests" ON)
option(TESSITURA_ENABLE_ASIO "Build with ASIO support (requires ASIO SDK at ASIO_SDK_DIR)" OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/bigobj /MP)
endif()

set(TE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/tracktion_engine)
add_subdirectory(${TE_DIR}/modules/juce ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)
add_subdirectory(${TE_DIR}/modules ${CMAKE_BINARY_DIR}/tracktion EXCLUDE_FROM_ALL)

#==============================================================================
# tessitura_core: all JUCE + Tracktion module code, the engine layer and the UI,
# compiled once and shared by the app and the tests (JUCE "shared code" pattern).
add_library(tessitura_core STATIC)

file(GLOB_RECURSE TESSITURA_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.h)
target_sources(tessitura_core PRIVATE ${TESSITURA_SOURCES})

target_link_libraries(tessitura_core
    PRIVATE
        tracktion::tracktion_core
        tracktion::tracktion_engine
        tracktion::tracktion_graph
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

target_compile_definitions(tessitura_core
    PUBLIC
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
        JUCE_APPLICATION_NAME_STRING="Tessitura"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    INTERFACE
        $<TARGET_PROPERTY:tessitura_core,COMPILE_DEFINITIONS>)

if (TESSITURA_ENABLE_ASIO)
    if (NOT DEFINED ASIO_SDK_DIR)
        message(FATAL_ERROR "TESSITURA_ENABLE_ASIO requires -DASIO_SDK_DIR=<path to ASIO SDK>")
    endif()
    target_compile_definitions(tessitura_core PUBLIC JUCE_ASIO=1)
    target_include_directories(tessitura_core PRIVATE ${ASIO_SDK_DIR}/common)
endif()

target_include_directories(tessitura_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    INTERFACE
        $<TARGET_PROPERTY:tessitura_core,INCLUDE_DIRECTORIES>)

set_target_properties(tessitura_core PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    VISIBILITY_INLINES_HIDDEN TRUE
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(tessitura_core PRIVATE -Wno-unknown-pragmas)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tessitura_core PUBLIC atomic)
endif()

#==============================================================================
if (TESSITURA_BUILD_APP)
    juce_add_gui_app(Tessitura
        PRODUCT_NAME "Tessitura"
        COMPANY_NAME "Tessitura"
        BUNDLE_ID "app.tessitura.daw"
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "Tessitura records audio from your audio interface.")

    target_sources(Tessitura PRIVATE src/app/Main.cpp)
    target_link_libraries(Tessitura PRIVATE tessitura_core)
endif()

if (TESSITURA_BUILD_TESTS)
    enable_testing()
    add_executable(TessituraTests tests/TestMain.cpp tests/EngineTests.cpp)
    target_include_directories(TessituraTests PRIVATE ${TE_DIR}/modules/3rd_party/doctest)
    target_link_libraries(TessituraTests PRIVATE tessitura_core)
    add_test(NAME TessituraTests COMMAND TessituraTests)
endif()
