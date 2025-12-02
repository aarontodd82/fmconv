# libopenmpt CMake build configuration
# Build libopenmpt as a static library from the OpenMPT submodule

option(BUILD_OPENMPT "Build libopenmpt for S3M/MOD/XM/IT support" ON)

if(BUILD_OPENMPT)

set(OPENMPT_DIR ${CMAKE_SOURCE_DIR}/openmpt)

# Collect source files
file(GLOB LIBOPENMPT_SOURCES
    ${OPENMPT_DIR}/libopenmpt/*.cpp
)

file(GLOB_RECURSE SOUNDLIB_SOURCES
    ${OPENMPT_DIR}/soundlib/*.cpp
)

file(GLOB_RECURSE SOUNDDSP_SOURCES
    ${OPENMPT_DIR}/sounddsp/*.cpp
)

file(GLOB_RECURSE COMMON_SOURCES
    ${OPENMPT_DIR}/common/*.cpp
)

# Bundled dependencies
file(GLOB MINIZ_SOURCES
    ${OPENMPT_DIR}/include/miniz/miniz.c
)

# Audio codec implementations
set(CODEC_SOURCES
    ${OPENMPT_DIR}/include/stb_vorbis/stb_vorbis.c
    ${OPENMPT_DIR}/include/minimp3/minimp3.c
)

# Our custom OPL export code (compiled in libopenmpt context)
set(OPENMPT_EXPORT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/openmpt/openmpt_export.cpp
)

# Create static library
add_library(libopenmpt STATIC
    ${LIBOPENMPT_SOURCES}
    ${SOUNDLIB_SOURCES}
    ${SOUNDDSP_SOURCES}
    ${COMMON_SOURCES}
    ${MINIZ_SOURCES}
    ${CODEC_SOURCES}
    ${OPENMPT_EXPORT_SOURCES}
)

# Include directories
target_include_directories(libopenmpt PUBLIC
    ${OPENMPT_DIR}
    ${OPENMPT_DIR}/src
    ${OPENMPT_DIR}/common
)

target_include_directories(libopenmpt PRIVATE
    ${OPENMPT_DIR}/include
    ${OPENMPT_DIR}/include/miniz
    ${OPENMPT_DIR}/include/minimp3
    ${OPENMPT_DIR}/include/stb_vorbis
)

# Compile definitions for minimal build
target_compile_definitions(libopenmpt PRIVATE
    LIBOPENMPT_BUILD
    # Use bundled dependencies
    MPT_WITH_MINIZ
    MPT_WITH_MINIMP3
    MPT_WITH_STBVORBIS
    # Disable optional features we don't need
    NO_PLUGINS
    NO_DMO
    NO_VST
    NO_LIBOPENMPT_C
    MODPLUG_NO_FILESAVE
)

# Platform-specific settings
if(MSVC)
    target_compile_definitions(libopenmpt PRIVATE
        WIN32
        _WIN32
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        _CRT_SECURE_NO_DEPRECATE
    )
    # Disable some warnings for third-party code
    target_compile_options(libopenmpt PRIVATE
        /wd4244  # conversion, possible loss of data
        /wd4267  # conversion from size_t
        /wd4305  # truncation
        /wd4996  # deprecated
        /wd4127  # conditional expression is constant
        /wd4389  # signed/unsigned mismatch
        /wd4100  # unreferenced formal parameter
        /wd4706  # assignment within conditional
        /bigobj  # increase object file sections
    )
else()
    target_compile_options(libopenmpt PRIVATE
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-unused-variable
    )
endif()

# C++17 required
target_compile_features(libopenmpt PUBLIC cxx_std_17)

endif() # BUILD_OPENMPT
