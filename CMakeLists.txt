cmake_minimum_required(VERSION 3.24)
project(Sonatrix VERSION 0.1.0 LANGUAGES CXX OBJCXX Swift)

# Require C++20 for lock-free concurrency and modern DSP features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set common flags for Real-time Audio
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -Wall -Wextra")

# Locate required Apple Frameworks
find_library(COREAUDIO_FRAMEWORK CoreAudio)
find_library(COREAUDIOKIT_FRAMEWORK CoreAudioKit)
find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
find_library(COREMIDI_FRAMEWORK CoreMIDI)
find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
find_library(AVFOUNDATION_FRAMEWORK AVFoundation)
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(APPKIT_FRAMEWORK AppKit)
find_library(SWIFTUI_FRAMEWORK SwiftUI)

# ------------------------------------------------------------------------------
# 1. The Core DSP and Event Engine (Static Library)
# ------------------------------------------------------------------------------
add_subdirectory(src/core)

# ------------------------------------------------------------------------------
# 2. Standalone macOS Application Target
# ------------------------------------------------------------------------------
add_executable(SonatrixStandalone MACOSX_BUNDLE
    src/standalone/SonatrixApp.swift
    src/ui/ArrangementView.swift
    src/ui/PatternBrowser.swift
    src/ui/DeltaGraphEditor.swift
)
target_link_libraries(SonatrixStandalone PRIVATE 
    SonatrixCore
    ${COREAUDIO_FRAMEWORK}
    ${COREMIDI_FRAMEWORK}
    ${AVFOUNDATION_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${SWIFTUI_FRAMEWORK}
)

set_target_properties(SonatrixStandalone PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/standalone/Info.plist
)

# ------------------------------------------------------------------------------
# 3. AudioUnit v3 (AUv3) Plugin Target
# ------------------------------------------------------------------------------
add_library(SonatrixAUv3 MODULE
    src/plugin/SonatrixAudioUnit.mm
    src/plugin/SonatrixAudioUnit.h
    src/ui/ArrangementView.swift
    src/ui/PatternBrowser.swift
    src/ui/DeltaGraphEditor.swift
)
target_link_libraries(SonatrixAUv3 PRIVATE
    SonatrixCore
    ${AUDIOUNIT_FRAMEWORK}
    ${AUDIOTOOLBOX_FRAMEWORK}
    ${COREAUDIO_FRAMEWORK}
    ${COREAUDIOKIT_FRAMEWORK}
    ${COREMIDI_FRAMEWORK}
    ${AVFOUNDATION_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${SWIFTUI_FRAMEWORK}
)

set_target_properties(SonatrixAUv3 PROPERTIES
    BUNDLE True
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION "appex"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/plugin/Info.plist
)
