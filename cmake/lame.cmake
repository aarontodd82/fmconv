# CMake build for LAME MP3 encoder library
# This builds only libmp3lame as a static library (no frontend, no mpglib decoder)

set(LAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lame)

# Source files for libmp3lame (encoder only, no decoder/mpglib)
set(LAME_SOURCES
    ${LAME_DIR}/libmp3lame/bitstream.c
    ${LAME_DIR}/libmp3lame/encoder.c
    ${LAME_DIR}/libmp3lame/fft.c
    ${LAME_DIR}/libmp3lame/gain_analysis.c
    ${LAME_DIR}/libmp3lame/id3tag.c
    ${LAME_DIR}/libmp3lame/lame.c
    ${LAME_DIR}/libmp3lame/newmdct.c
    ${LAME_DIR}/libmp3lame/presets.c
    ${LAME_DIR}/libmp3lame/psymodel.c
    ${LAME_DIR}/libmp3lame/quantize.c
    ${LAME_DIR}/libmp3lame/quantize_pvt.c
    ${LAME_DIR}/libmp3lame/reservoir.c
    ${LAME_DIR}/libmp3lame/set_get.c
    ${LAME_DIR}/libmp3lame/tables.c
    ${LAME_DIR}/libmp3lame/takehiro.c
    ${LAME_DIR}/libmp3lame/util.c
    ${LAME_DIR}/libmp3lame/vbrquantize.c
    ${LAME_DIR}/libmp3lame/VbrTag.c
    ${LAME_DIR}/libmp3lame/version.c
)
# Note: mpglib_interface.c is excluded - it requires mpglib decoder which we don't need

# Add SSE optimizations for x64 builds
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND LAME_SOURCES ${LAME_DIR}/libmp3lame/vector/xmm_quantize_sub.c)
endif()

# Create static library
add_library(lame_static STATIC ${LAME_SOURCES})

# Include directories
target_include_directories(lame_static PUBLIC
    ${LAME_DIR}/include
    ${LAME_DIR}/libmp3lame
)

# Private include for config
target_include_directories(lame_static PRIVATE
    ${LAME_DIR}
)

# Compile definitions
# Note: We do NOT define HAVE_MPGLIB - this disables decode-on-the-fly feature
# which requires mpglib decoder that we don't include
target_compile_definitions(lame_static PRIVATE
    HAVE_CONFIG_H
    HAVE_STDINT_H
    STDC_HEADERS
)

# Use configMS.h as config.h on Windows
if(WIN32)
    target_compile_definitions(lame_static PRIVATE
        HAVE_CONFIG_H=0
    )
    # Copy configMS.h to config.h in build dir
    configure_file(
        ${LAME_DIR}/configMS.h
        ${CMAKE_CURRENT_BINARY_DIR}/lame_config/config.h
        COPYONLY
    )
    target_include_directories(lame_static PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/lame_config
    )
else()
    # Unix-like systems - generate config.h
    include(CheckIncludeFile)
    include(CheckTypeSize)

    check_include_file(stdint.h HAVE_STDINT_H)
    check_include_file(inttypes.h HAVE_INTTYPES_H)
    check_include_file(errno.h HAVE_ERRNO_H)
    check_include_file(fcntl.h HAVE_FCNTL_H)
    check_include_file(limits.h HAVE_LIMITS_H)
    check_include_file(string.h HAVE_STRING_H)

    check_type_size(short SIZEOF_SHORT)
    check_type_size(int SIZEOF_INT)
    check_type_size(long SIZEOF_LONG)
    check_type_size("unsigned short" SIZEOF_UNSIGNED_SHORT)
    check_type_size("unsigned int" SIZEOF_UNSIGNED_INT)
    check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
    check_type_size(float SIZEOF_FLOAT)
    check_type_size(double SIZEOF_DOUBLE)
    check_type_size("long double" SIZEOF_LONG_DOUBLE)

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lame_config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/lame_config/config.h
    )
    target_include_directories(lame_static PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/lame_config
    )
endif()

# Suppress warnings for third-party code
if(MSVC)
    target_compile_options(lame_static PRIVATE /W0)
else()
    target_compile_options(lame_static PRIVATE -w)
endif()

message(STATUS "LAME MP3 encoder library configured")
