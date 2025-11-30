cmake_minimum_required(VERSION 3.16)
project(fmconv CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# libADLMIDI - for MIDI-style format conversion
# ============================================================================
set(libADLMIDI_STATIC ON CACHE BOOL "Build static library" FORCE)
set(WITH_MIDI_SEQUENCER ON CACHE BOOL "Enable MIDI sequencer" FORCE)
set(WITH_EMBEDDED_BANKS ON CACHE BOOL "Enable embedded banks" FORCE)
add_subdirectory(libADLMIDI)

# ============================================================================
# AdPlug + libbinio - for native OPL format conversion
# ============================================================================
# Build libbinio first (AdPlug depends on it)
set(libbinio_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libbinio" FORCE)
set(libbinio_INCLUDE_PACKAGING OFF CACHE BOOL "Skip packaging" FORCE)
add_subdirectory(libbinio)

# Build AdPlug
set(adplug_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static AdPlug" FORCE)
set(adplug_INCLUDE_TEST OFF CACHE BOOL "Skip tests" FORCE)
set(adplug_INCLUDE_PACKAGING OFF CACHE BOOL "Skip packaging" FORCE)
add_subdirectory(adplug)

# ============================================================================
# fmconv - Unified converter
# Converts both MIDI-style formats (with bank selection) and native OPL
# formats (with embedded instruments) to VGM
# ============================================================================
add_executable(fmconv
    src/unified_converter.cpp
    src/vgm_writer/vgm_chip.cpp
    src/vgm_writer/gd3_tag.cpp
    src/detection/bank_detector.cpp
    src/formats/hmp_to_midi.cpp
    src/adplug_vgm/vgm_opl.cpp
    src/miniz.c
    src/miniz_tdef.c
    src/miniz_tinfl.c
    src/miniz_zip.c
)

target_include_directories(fmconv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libADLMIDI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libADLMIDI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/adplug/src/generated/include
)

# miniz provides gzip compression (embedded, no external dependency)
target_compile_definitions(fmconv PRIVATE HAVE_MINIZ)

target_link_libraries(fmconv
    ADLMIDI_static
    adplug::adplug
)

if(WIN32)
    target_compile_definitions(fmconv PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS fmconv DESTINATION bin)
