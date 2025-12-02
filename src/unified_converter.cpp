/*
 * unified_converter - Single tool to convert game music formats to VGM
 *
 * This is a unified converter that automatically routes files to the
 * appropriate backend based on format:
 *
 * 1. MIDI-style formats (libADLMIDI): MIDI, XMI, MUS, HMP/HMI
 *    - Use selectable FM instrument banks (79 banks)
 *    - Bank auto-detection based on game/filename
 *
 * 2. Native tracker/player formats (AdPlug): 40+ formats
 *    - A2M, RAD, S3M, D00, CMF, LAA, ROL, etc.
 *    - Have embedded instruments, no bank selection needed
 *    - Chip type auto-detected (OPL2, Dual OPL2, OPL3)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <set>
#include <map>
#include <vector>

#ifdef HAVE_MINIZ
#include "miniz.h"
#elif defined(HAVE_ZLIB)
#include <zlib.h>
#endif

// libADLMIDI for MIDI-style formats
#include "../libADLMIDI/include/adlmidi.h"
#include "../libADLMIDI/src/adlmidi_midiplay.hpp"
#include "../libADLMIDI/src/adlmidi_opl3.hpp"

// AdPlug for native formats
#include "adplug/adplug.h"
#include "adplug_vgm/vgm_opl.h"

// Shared components
#include "vgm_writer/vgm_chip.h"
#include "vgm_writer/gd3_tag.h"
#include "detection/bank_detector.h"
#include "formats/hmp_to_midi.h"
#include "fm9_writer/fm9_writer.h"

// OpenMPT for tracker formats (optional)
#ifdef HAVE_OPENMPT
#include "openmpt/openmpt_export.h"
#endif

// MP3 encoding (optional)
#ifdef HAVE_LAME
#include "audio/mp3_encoder.h"
#endif

// Format categories
enum class FormatCategory
{
    MIDI_STYLE,     // Use libADLMIDI (needs bank)
    NATIVE_OPL,     // Use AdPlug (embedded instruments)
    TRACKER_FORMAT, // Use OpenMPT (S3M/MOD/XM/IT with OPL and/or samples)
    VGM_INPUT,      // Already VGM/VGZ - pass through (for adding audio/fx to FM9)
    UNKNOWN
};

// Output format
enum class OutputFormat
{
    FM9,    // FM9 (gzipped, default)
    VGZ,    // VGZ (gzipped VGM)
    VGM     // VGM (uncompressed)
};

// Command-line options
struct Options
{
    std::string input_file;
    std::string output_file;
    int bank = -1;              // -1 = auto-detect (MIDI-style only)
    int vol_model = 0;          // 0 = auto (MIDI-style only)
    bool interactive = true;    // Prompt for uncertain banks
    bool show_banks = false;
    bool show_vol_models = false;
    bool show_formats = false;
    bool verbose = false;

    // Playback options
    int subsong = -1;           // -1 = default subsong (AdPlug)
    int max_length_sec = 600;   // Maximum length (AdPlug)
    bool loop_once = true;      // Stop after first loop (AdPlug)

    // Output options
    OutputFormat output_format = OutputFormat::FM9;  // Default to FM9
    bool add_suffix = true;     // Add format suffix like _RAD (default on)

    // FM9 options
    std::string audio_file;     // Optional audio file (WAV/MP3)
    std::string fx_file;        // Optional effects JSON file
    std::string image_file;     // Optional cover image (PNG/JPEG/GIF)
    bool dither_image = true;   // Apply dithering to cover image

    // Audio compression options
    bool no_compress_audio = false;  // Skip MP3 compression (embed WAV as-is)
    int audio_bitrate = 192;         // MP3 bitrate (96, 128, 160, 192, 256, 320)

    // GD3 metadata
    std::string title;
    std::string author;
    std::string album;
    std::string system;
    std::string date;
    std::string notes;
};

// Get lowercase file extension
static std::string getExtension(const std::string& filename)
{
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos)
        return "";

    std::string ext = filename.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// Get uppercase file extension (for suffix like _RAD)
static std::string getExtensionUpper(const std::string& filename)
{
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos)
        return "";

    std::string ext = filename.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    return ext;
}

// Get filename without path
static std::string getFilename(const std::string& path)
{
    size_t sep_pos = path.find_last_of("/\\");
    if (sep_pos == std::string::npos)
        return path;
    return path.substr(sep_pos + 1);
}

// Get directory from path
static std::string getDirectory(const std::string& path)
{
    size_t sep_pos = path.find_last_of("/\\");
    if (sep_pos == std::string::npos)
        return "";
    return path.substr(0, sep_pos + 1);
}

// Get filename without extension
static std::string getBasename(const std::string& filename)
{
    std::string name = getFilename(filename);
    size_t dot_pos = name.find_last_of('.');
    if (dot_pos == std::string::npos)
        return name;
    return name.substr(0, dot_pos);
}

// Check if path is a directory (ends with / or \, or exists as dir)
static bool isDirectory(const std::string& path)
{
    if (path.empty())
        return false;
    char last = path[path.length() - 1];
    return (last == '/' || last == '\\');
}

// Check if gzip compression is available
static bool hasGzipSupport()
{
#if defined(HAVE_MINIZ) || defined(HAVE_ZLIB)
    return true;
#else
    return false;
#endif
}

// Gzip decompression for VGZ input
// Returns empty vector on error or if not gzip
static std::vector<uint8_t> gzipDecompress(const std::vector<uint8_t>& data)
{
#if defined(HAVE_MINIZ) || defined(HAVE_ZLIB)
    if (data.size() < 18) return {};  // Too small for gzip

    // Check gzip magic
    if (data[0] != 0x1f || data[1] != 0x8b) {
        return {};  // Not gzip
    }

    // Get original size from last 4 bytes (little-endian)
    uint32_t orig_size =
        data[data.size() - 4] |
        (data[data.size() - 3] << 8) |
        (data[data.size() - 2] << 16) |
        (data[data.size() - 1] << 24);

    // Sanity check - don't allocate more than 64MB
    if (orig_size > 64 * 1024 * 1024) {
        fprintf(stderr, "Error: Decompressed size too large (%u bytes)\n", orig_size);
        return {};
    }

    std::vector<uint8_t> decompressed(orig_size);

    // Skip gzip header (minimum 10 bytes)
    size_t header_size = 10;
    uint8_t flags = data[3];

    // Handle optional header fields
    if (flags & 0x04) {  // FEXTRA
        if (header_size + 2 > data.size()) return {};
        uint16_t xlen = data[header_size] | (data[header_size + 1] << 8);
        header_size += 2 + xlen;
    }
    if (flags & 0x08) {  // FNAME
        while (header_size < data.size() && data[header_size] != 0) header_size++;
        header_size++;  // Skip null terminator
    }
    if (flags & 0x10) {  // FCOMMENT
        while (header_size < data.size() && data[header_size] != 0) header_size++;
        header_size++;
    }
    if (flags & 0x02) {  // FHCRC
        header_size += 2;
    }

    if (header_size >= data.size() - 8) return {};

    // Deflate data is between header and 8-byte trailer (CRC32 + size)
    size_t deflate_size = data.size() - header_size - 8;

    mz_stream strm = {};
    strm.next_in = data.data() + header_size;
    strm.avail_in = static_cast<unsigned int>(deflate_size);
    strm.next_out = decompressed.data();
    strm.avail_out = static_cast<unsigned int>(decompressed.size());

    // Use raw inflate (windowBits = -15)
    int ret = mz_inflateInit2(&strm, -MZ_DEFAULT_WINDOW_BITS);
    if (ret != MZ_OK) return {};

    ret = mz_inflate(&strm, MZ_FINISH);
    mz_inflateEnd(&strm);

    if (ret != MZ_STREAM_END) {
        fprintf(stderr, "Error: Gzip decompression failed\n");
        return {};
    }

    return decompressed;
#else
    (void)data;
    return {};
#endif
}

// Compress data using gzip format
// Returns empty vector on error or if compression not available
static std::vector<uint8_t> gzipCompress(const std::vector<uint8_t>& data)
{
#if defined(HAVE_MINIZ) || defined(HAVE_ZLIB)
    if (data.empty())
        return {};

    // Calculate CRC32 of uncompressed data (needed for gzip trailer)
    mz_ulong crc = mz_crc32(MZ_CRC32_INIT, data.data(), data.size());

    // Compress using raw deflate (windowBits = -15 for raw deflate, no zlib header)
    mz_ulong compressed_bound = mz_compressBound(static_cast<mz_ulong>(data.size()));
    std::vector<uint8_t> deflate_data(compressed_bound);

    mz_stream strm = {};
    strm.next_in = data.data();
    strm.avail_in = static_cast<unsigned int>(data.size());
    strm.next_out = deflate_data.data();
    strm.avail_out = static_cast<unsigned int>(deflate_data.size());

    // Use raw deflate (windowBits = -15)
    int ret = mz_deflateInit2(&strm, MZ_DEFAULT_COMPRESSION, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY);
    if (ret != MZ_OK)
    {
        fprintf(stderr, "Error: Failed to initialize deflate compression (error %d)\n", ret);
        return {};
    }

    ret = mz_deflate(&strm, MZ_FINISH);
    mz_ulong deflate_size = strm.total_out;
    mz_deflateEnd(&strm);

    if (ret != MZ_STREAM_END)
    {
        fprintf(stderr, "Error: Deflate compression failed (error %d)\n", ret);
        return {};
    }

    // Build gzip file: header (10 bytes) + deflate data + trailer (8 bytes)
    std::vector<uint8_t> gzip_data;
    gzip_data.reserve(10 + deflate_size + 8);

    // Gzip header (10 bytes)
    gzip_data.push_back(0x1f);  // Magic number
    gzip_data.push_back(0x8b);  // Magic number
    gzip_data.push_back(0x08);  // Compression method (deflate)
    gzip_data.push_back(0x00);  // Flags (none)
    gzip_data.push_back(0x00);  // Modification time (4 bytes, set to 0)
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);  // Extra flags
    gzip_data.push_back(0xff);  // OS (unknown)

    // Deflate compressed data
    gzip_data.insert(gzip_data.end(), deflate_data.begin(), deflate_data.begin() + deflate_size);

    // Gzip trailer: CRC32 (4 bytes, little-endian) + original size (4 bytes, little-endian)
    gzip_data.push_back(static_cast<uint8_t>(crc & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((crc >> 8) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((crc >> 16) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((crc >> 24) & 0xff));

    uint32_t orig_size = static_cast<uint32_t>(data.size());
    gzip_data.push_back(static_cast<uint8_t>(orig_size & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((orig_size >> 8) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((orig_size >> 16) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((orig_size >> 24) & 0xff));

    return gzip_data;
#else
    (void)data;
    return {};
#endif
}

// Write VGM/VGZ output (legacy helper)
static size_t writeVgmOutput(const std::string& filename, const std::vector<uint8_t>& data, bool compress, bool* was_compressed = nullptr)
{
    std::vector<uint8_t> output_data;
    bool did_compress = false;

    if (compress && hasGzipSupport())
    {
        output_data = gzipCompress(data);
        if (output_data.empty())
        {
            fprintf(stderr, "Warning: Compression failed, writing uncompressed\n");
            output_data = data;
        }
        else
        {
            did_compress = true;
        }
    }
    else
    {
        output_data = data;
    }

    if (was_compressed)
        *was_compressed = did_compress;

    std::ofstream out(filename, std::ios::binary);
    if (!out)
    {
        fprintf(stderr, "Error: Failed to open output file: %s\n", filename.c_str());
        return 0;
    }

    out.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());
    out.close();
    return output_data.size();
}

// Write output in the appropriate format (FM9, VGZ, or VGM)
// Returns bytes written, or 0 on error
static size_t writeOutputFile(const std::string& filename,
                               const std::vector<uint8_t>& vgm_data,
                               const Options& opts)
{
    size_t bytes_written = 0;
    const char* format_name = "VGM";

    if (opts.output_format == OutputFormat::FM9)
    {
        FM9Writer writer;
        writer.setVGMData(vgm_data);

        // Set source format from input file extension
        std::string ext = getExtension(opts.input_file);
        writer.setSourceFormat(ext);

        // Load optional audio
        if (!opts.audio_file.empty())
        {
            printf("Embedding audio: %s\n", opts.audio_file.c_str());

            // Check file type
            std::string audio_ext = getExtension(opts.audio_file);
            bool is_wav = (audio_ext == "wav" || audio_ext == "wave");
            bool is_mp3 = (audio_ext == "mp3");

            // Warn if bitrate specified for MP3 input (we don't re-encode)
            if (is_mp3 && opts.audio_bitrate != 192)
            {
                printf("Note: --audio-bitrate ignored for MP3 input (no re-encoding)\n");
            }

#ifdef HAVE_LAME
            if (is_wav && !opts.no_compress_audio)
            {
                // Compress WAV to MP3 (also normalizes to 44.1kHz stereo)
                printf("Converting to MP3 (%d kbps, 44.1kHz stereo)...\n", opts.audio_bitrate);

                std::string mp3_error;
                std::vector<uint8_t> mp3_data = encodeWAVtoMP3(opts.audio_file, opts.audio_bitrate, &mp3_error);

                if (mp3_data.empty())
                {
                    fprintf(stderr, "Warning: MP3 encoding failed: %s\n", mp3_error.c_str());
                    fprintf(stderr, "         Falling back to WAV\n");
                    // Fall through to load as normalized WAV
                    std::string wav_error;
                    std::vector<uint8_t> wav_data = normalizeWAVFile(opts.audio_file, &wav_error);
                    if (wav_data.empty())
                    {
                        fprintf(stderr, "Error: %s\n", wav_error.c_str());
                        return 0;
                    }
                    writer.setAudioData(wav_data, FM9_AUDIO_WAV);
                }
                else
                {
                    // Read original WAV size for comparison
                    std::ifstream wav_file(opts.audio_file, std::ios::binary | std::ios::ate);
                    size_t wav_size = wav_file ? static_cast<size_t>(wav_file.tellg()) : 0;

                    printf("MP3 encoded: %zu bytes", mp3_data.size());
                    if (wav_size > 0)
                    {
                        printf(" (%.1f%% of original)", 100.0 * mp3_data.size() / wav_size);
                    }
                    printf("\n");

                    writer.setAudioData(mp3_data, FM9_AUDIO_MP3);
                }
            }
            else if (is_wav && opts.no_compress_audio)
            {
                // Normalize WAV to 44.1kHz 16-bit stereo
                printf("Normalizing to 44.1kHz 16-bit stereo WAV...\n");
                std::string wav_error;
                std::vector<uint8_t> wav_data = normalizeWAVFile(opts.audio_file, &wav_error);
                if (wav_data.empty())
                {
                    fprintf(stderr, "Error: %s\n", wav_error.c_str());
                    return 0;
                }
                writer.setAudioData(wav_data, FM9_AUDIO_WAV);
            }
            else
#endif
            {
                // Load audio file as-is (MP3, or WAV when LAME not available)
#ifndef HAVE_LAME
                if (!opts.no_compress_audio && is_wav)
                {
                    printf("Note: MP3 encoding not available (LAME not linked), embedding as WAV\n");
                }
#endif
                if (!writer.setAudioFile(opts.audio_file))
                {
                    fprintf(stderr, "Error: %s\n", writer.getError().c_str());
                    return 0;
                }
            }
        }

        // Load optional FX
        if (!opts.fx_file.empty())
        {
            printf("Embedding effects: %s\n", opts.fx_file.c_str());
            if (!writer.setFXFile(opts.fx_file))
            {
                fprintf(stderr, "Error: %s\n", writer.getError().c_str());
                return 0;
            }
        }

        // Load optional image
        if (!opts.image_file.empty())
        {
            printf("Embedding cover image: %s%s\n", opts.image_file.c_str(),
                   opts.dither_image ? " (with dithering)" : " (no dither)");
            if (!writer.setImageFile(opts.image_file, opts.dither_image))
            {
                fprintf(stderr, "Error: %s\n", writer.getError().c_str());
                return 0;
            }
        }

        printf("Writing: %s (FM9 format, gzip compressed)\n", filename.c_str());
        bytes_written = writer.write(filename);
        if (bytes_written == 0)
        {
            fprintf(stderr, "Error: %s\n", writer.getError().c_str());
            return 0;
        }

        format_name = "FM9";
        printf("Success! %s size: %zu bytes", format_name, bytes_written);
        if (writer.hasAudio())
            printf(" (includes embedded audio)");
        if (writer.hasFX())
            printf(" (includes effects)");
        if (writer.hasImage())
            printf(" (includes cover image)");
        printf("\n");
    }
    else
    {
        // VGZ or VGM output
        bool compress = (opts.output_format == OutputFormat::VGZ);
        bool was_compressed = false;

        printf("Writing: %s%s\n", filename.c_str(), compress ? " (gzip compressed)" : "");
        bytes_written = writeVgmOutput(filename, vgm_data, compress, &was_compressed);
        if (bytes_written == 0)
            return 0;

        format_name = was_compressed ? "VGZ" : "VGM";
        printf("Success! %s size: %zu bytes (uncompressed VGM: %zu bytes)\n",
               format_name, bytes_written, vgm_data.size());
    }

    return bytes_written;
}

// Determine format category from extension
static FormatCategory categorizeFormat(const std::string& filename)
{
    std::string ext = getExtension(filename);

    // VGM/VGZ input (pass-through for adding audio/fx)
    if (ext == "vgm" || ext == "vgz" || ext == "fm9")
    {
        return FormatCategory::VGM_INPUT;
    }

    // MIDI-style formats (need bank selection)
    if (ext == "mid" || ext == "midi" || ext == "smf" || ext == "kar" ||
        ext == "rmi" || ext == "xmi" || ext == "mus" || ext == "hmp" ||
        ext == "hmi" || ext == "klm")
    {
        return FormatCategory::MIDI_STYLE;
    }

    // Tracker formats - use OpenMPT if available for accurate playback
    // OpenMPT supports 60+ tracker formats including those with OPL instruments
#ifdef HAVE_OPENMPT
    if (ext == "s3m" || ext == "mod" || ext == "xm" || ext == "it" ||
        ext == "mptm" || ext == "stm" || ext == "669" || ext == "667" ||
        ext == "mtm" || ext == "med" || ext == "okt" || ext == "far" ||
        ext == "mdl" || ext == "ams" || ext == "dbm" || ext == "digi" ||
        ext == "dmf" || ext == "dsm" || ext == "dsym" || ext == "dtm" ||
        ext == "amf" || ext == "psm" || ext == "mt2" || ext == "umx" ||
        ext == "j2b" || ext == "ptm" || ext == "sfx" || ext == "sfx2" ||
        ext == "nst" || ext == "wow" || ext == "ult" || ext == "gdm" ||
        ext == "mo3" || ext == "oxm" || ext == "plm" || ext == "ppm" ||
        ext == "stx" || ext == "stp" || ext == "rtm" || ext == "pt36" ||
        ext == "ice" || ext == "mmcmp" || ext == "xpk" || ext == "mms" ||
        ext == "c67" || ext == "m15" || ext == "stk" || ext == "st26" ||
        ext == "unic" || ext == "cba" || ext == "etx" || ext == "fc" ||
        ext == "fc13" || ext == "fc14" || ext == "fmt" || ext == "fst" ||
        ext == "ftm" || ext == "gmc" || ext == "gtk" || ext == "gt2" ||
        ext == "puma" || ext == "smod" || ext == "symmod" || ext == "tcb" ||
        ext == "xmf")
    {
        return FormatCategory::TRACKER_FORMAT;
    }
#endif

    // Native OPL formats (embedded instruments)
    // This covers AdPlug-supported formats (excluding those handled by OpenMPT above)
    if (ext == "a2m" || ext == "a2t" || ext == "adl" || ext == "adlib" ||
        ext == "amd" || ext == "bam" || ext == "bmf" || ext == "cff" ||
        ext == "cmf" || ext == "d00" || ext == "dfm" || ext == "dmo" ||
        ext == "dro" || ext == "got" || ext == "ha2" ||
        ext == "hsc" || ext == "hsp" || ext == "hsq" ||
        ext == "jbm" || ext == "ksm" || ext == "laa" ||
        ext == "lds" || ext == "m" || ext == "mad" || ext == "mdi" ||
        ext == "mdy" || ext == "mkf" || ext == "mkj" || ext == "msc" ||
        ext == "mtk" || ext == "mtr" || ext == "pis" ||
        ext == "plx" || ext == "rac" || ext == "rad" || ext == "raw" ||
        ext == "rix" || ext == "rol" || ext == "sa2" ||
        ext == "sat" || ext == "sci" || ext == "sdb" || ext == "sng" ||
        ext == "sop" || ext == "sqx" || ext == "wlf" || ext == "xad" ||
        ext == "xms" || ext == "xsm" || ext == "agd")
    {
        return FormatCategory::NATIVE_OPL;
    }

    // Formats supported by both AdPlug and OpenMPT - use AdPlug when OpenMPT not available
#ifndef HAVE_OPENMPT
    if (ext == "s3m" || ext == "imf" || ext == "ims" || ext == "dtm")
    {
        return FormatCategory::NATIVE_OPL;
    }
#endif

    return FormatCategory::UNKNOWN;
}

void show_usage(const char* program_name)
{
    printf("Usage: %s [OPTIONS] <input> [output]\n\n", program_name);
    printf("Convert game music formats to FM9/VGM for OPL2/OPL3 hardware\n\n");

    printf("MIDI-style formats (MIDI, RMI, XMI, MUS, HMP/HMI, KLM):\n");
    printf("  These formats use MIDI note/program messages and require\n");
    printf("  FM instrument banks. Bank can be auto-detected or specified.\n\n");

    printf("Native OPL formats (40+ types via AdPlug):\n");
    printf("  These formats have embedded instruments - no bank needed.\n");
    printf("  Chip type (OPL2, Dual OPL2, OPL3) is auto-detected.\n\n");

    printf("Options:\n");
    printf("  -b, --bank <N>       OPL3 bank for MIDI-style (0-78, default: auto)\n");
    printf("  -v, --vol-model <N>  Volume model for MIDI-style (0-11, default: 0)\n");
    printf("  -y, --yes            Non-interactive mode (no prompts)\n");
    printf("  -s, --subsong <N>    Subsong number for AdPlug formats\n");
    printf("  -l, --length <sec>   Maximum length in seconds (default: 600)\n");
    printf("  --no-loop            Don't stop at loop point (AdPlug formats)\n");
    printf("  --no-suffix          Don't add format suffix (_RAD, _A2M, etc) to filename\n");
    printf("  --verbose            Verbose output\n");
    printf("\n");
    printf("Output Format:\n");
    printf("  --format <fmt>       Output format: fm9 (default), vgz, vgm\n");
    printf("  --vgz                Shorthand for --format vgz\n");
    printf("  --vgm, --raw-vgm     Shorthand for --format vgm\n");
    printf("\n");
    printf("FM9 Options:\n");
    printf("  --audio <file>       Embed audio file (WAV or MP3) for playback\n");
    printf("  --fx <file>          Embed effects JSON file for automation\n");
    printf("  --image <file>       Embed cover image (PNG, JPEG, or GIF)\n");
    printf("  --no-dither          Disable dithering on cover image (clean output)\n");
    printf("\n");
    printf("Audio Compression (default: MP3 at 192kbps):\n");
    printf("  --uncompressed-audio Embed audio as WAV (no MP3 compression)\n");
    printf("  --audio-bitrate <N>  MP3 bitrate: 96, 128, 160, 192, 256, 320 (default: 192)\n");
    printf("\n");
    printf("Info:\n");
    printf("  --list-banks         Show all available FM banks\n");
    printf("  --list-vol-models    Show all volume models\n");
    printf("  --list-formats       Show all supported formats\n");
    printf("\n");
    printf("Metadata:\n");
    printf("  --title <text>       Track title\n");
    printf("  --author <text>      Composer name\n");
    printf("  --album <text>       Album/game name\n");
    printf("  --system <text>      Original system\n");
    printf("  --date <text>        Release date\n");
    printf("  --notes <text>       Additional notes\n");
    printf("\n");
    printf("  -o, --output <path>  Output file\n");
    printf("  -h, --help           Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s descent.hmp                    # Convert to FM9 (default)\n", program_name);
    printf("  %s doom.mus --vgz                 # Convert to VGZ format\n", program_name);
    printf("  %s game.rad --audio drums.mp3    # FM9 with embedded audio\n", program_name);
    printf("  %s tune.mid --fx effects.json    # FM9 with effects automation\n", program_name);
}

void show_banks()
{
    printf("Available FM banks (0-78) for MIDI-style formats:\n\n");
    printf("  0  - AIL (Audio Interface Library) - SimCity 2000, Miles Sound System\n");
    printf("  1  - Bisqwit (GENMIDI.OP2)\n");
    printf("  2  - HMI (Human Machine Interfaces) - Descent, Duke Nukem 3D\n");
    printf("  16 - DMX (GENMIDI.OP2) - DOOM, Heretic, Hexen\n");
    printf("  44 - Apogee IMF v1.0 - Wolfenstein 3D, Commander Keen\n");
    printf("  58 - WOPL Bank (Fat Man GM) - General MIDI\n");
    printf("  ... (79 banks total)\n\n");
    printf("Note: Bank selection only applies to MIDI, XMI, MUS, HMP/HMI formats.\n");
    printf("      Native formats (RAD, A2M, etc.) have embedded instruments.\n");
}

void show_vol_models()
{
    printf("Available volume models (for MIDI-style formats):\n\n");
    printf("  0  - AUTO: Automatically chosen by bank\n");
    printf("  1  - Generic: Linear scaling\n");
    printf("  2  - NativeOPL3: Logarithmic (OPL3 native)\n");
    printf("  3  - DMX: Logarithmic (DOOM)\n");
    printf("  4  - APOGEE: Logarithmic (Apogee Sound System)\n");
    printf("  10 - HMI: HMI Sound Operating System\n");
    printf("  11 - HMI_OLD: HMI (older variant)\n");
}

void show_formats()
{
    printf("Supported formats:\n\n");

    printf("MIDI-style formats (use FM instrument banks):\n");
    printf("  .mid, .midi, .smf, .kar  - Standard MIDI File\n");
    printf("  .xmi                      - Extended MIDI (Miles Sound)\n");
    printf("  .mus                      - DOOM/DMX Music\n");
    printf("  .hmp, .hmi                - Human Machine Interfaces MIDI\n");
    printf("\n");

    printf("Native OPL formats (embedded instruments via AdPlug):\n");
    printf("  .a2m    - Adlib Tracker 2\n");
    printf("  .adl    - Westwood ADL\n");
    printf("  .amd    - AMUSIC Adlib Tracker\n");
    printf("  .bam    - Bob's Adlib Music\n");
    printf("  .cff    - Boomtracker 4.0\n");
    printf("  .cmf    - Creative Music File\n");
    printf("  .d00    - EdLib\n");
    printf("  .dfm    - Digital-FM\n");
    printf("  .dmo    - Twin TrackPlayer\n");
    printf("  .dro    - DOSBox Raw OPL\n");
    printf("  .dtm    - DeFy Adlib Tracker\n");
    printf("  .got    - GOT (Game of Thrones?)\n");
    printf("  .hsc    - HSC-Tracker\n");
    printf("  .hsp    - HSC Packed\n");
    printf("  .imf, .wlf - id Software Music (Wolf3D, Duke3D)\n");
    printf("  .ksm    - Ken Silverman Music\n");
    printf("  .laa    - LucasArts AdLib Audio\n");
    printf("  .lds    - LOUDNESS Sound System\n");
    printf("  .mad    - Mlat Adlib Tracker\n");
    printf("  .mdi    - AdLib MIDI\n");
    printf("  .mkj    - MKJamz\n");
    printf("  .msc    - AdLib MSC\n");
    printf("  .mtk    - MPU-401 Trakker\n");
    printf("  .rad    - Reality AdLib Tracker\n");
    printf("  .raw    - Raw AdLib Capture\n");
    printf("  .rix    - Softstar RIX\n");
    printf("  .rol    - AdLib Visual Composer\n");
    printf("  .s3m    - Scream Tracker 3 (OPL instruments only)\n");
    printf("  .sa2    - Surprise! Adlib Tracker 2\n");
    printf("  .sat    - Surprise! Adlib Tracker\n");
    printf("  .sci    - Sierra SCI\n");
    printf("  .sng    - Various (SNGPlay, Faust, etc.)\n");
    printf("  .sop    - Note Sequencer by sopepos\n");
    printf("  .xad    - Various (FLASH, BMF, etc.)\n");
    printf("  .xms    - XMS-Tracker\n");
    printf("  ... and more!\n");
}

int prompt_user_for_bank(const BankDetection& detection)
{
    printf("\nBank auto-detection uncertain (%.0f%% confidence)\n", detection.confidence * 100);
    printf("Detected: Bank %d - %s\n", detection.bank_id, detection.reason.c_str());
    printf("Enter bank number (0-78) or press Enter to use detected bank: ");

    char input[32];
    if (fgets(input, sizeof(input), stdin))
    {
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0)
            return detection.bank_id;

        int bank = atoi(input);
        if (bank >= 0 && bank <= 78)
            return bank;
    }

    return detection.bank_id;
}

bool parse_args(int argc, char** argv, Options& opts)
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            show_usage(argv[0]);
            return false;
        }
        else if (arg == "--list-banks")
        {
            opts.show_banks = true;
            return true;
        }
        else if (arg == "--list-vol-models")
        {
            opts.show_vol_models = true;
            return true;
        }
        else if (arg == "--list-formats")
        {
            opts.show_formats = true;
            return true;
        }
        else if (arg == "-b" || arg == "--bank")
        {
            if (i + 1 < argc)
                opts.bank = atoi(argv[++i]);
        }
        else if (arg == "-v" || arg == "--vol-model")
        {
            if (i + 1 < argc)
                opts.vol_model = atoi(argv[++i]);
        }
        else if (arg == "-y" || arg == "--yes")
        {
            opts.interactive = false;
        }
        else if (arg == "-s" || arg == "--subsong")
        {
            if (i + 1 < argc)
                opts.subsong = atoi(argv[++i]);
        }
        else if (arg == "-l" || arg == "--length")
        {
            if (i + 1 < argc)
                opts.max_length_sec = atoi(argv[++i]);
        }
        else if (arg == "--no-loop")
        {
            opts.loop_once = false;
        }
        else if (arg == "--format" && i + 1 < argc)
        {
            std::string fmt = argv[++i];
            std::transform(fmt.begin(), fmt.end(), fmt.begin(), ::tolower);
            if (fmt == "fm9")
                opts.output_format = OutputFormat::FM9;
            else if (fmt == "vgz")
                opts.output_format = OutputFormat::VGZ;
            else if (fmt == "vgm")
                opts.output_format = OutputFormat::VGM;
            else
            {
                fprintf(stderr, "Error: Unknown format '%s' (use fm9, vgz, or vgm)\n", fmt.c_str());
                return false;
            }
        }
        else if (arg == "--vgz")
        {
            opts.output_format = OutputFormat::VGZ;
        }
        else if (arg == "--vgm" || arg == "--raw-vgm")
        {
            opts.output_format = OutputFormat::VGM;
        }
        else if (arg == "--audio" && i + 1 < argc)
        {
            opts.audio_file = argv[++i];
        }
        else if (arg == "--fx" && i + 1 < argc)
        {
            opts.fx_file = argv[++i];
        }
        else if (arg == "--image" && i + 1 < argc)
        {
            opts.image_file = argv[++i];
        }
        else if (arg == "--no-dither")
        {
            opts.dither_image = false;
        }
        else if (arg == "--uncompressed-audio")
        {
            opts.no_compress_audio = true;
        }
        else if (arg == "--audio-bitrate" && i + 1 < argc)
        {
            opts.audio_bitrate = atoi(argv[++i]);
            // Validate bitrate
            if (opts.audio_bitrate != 96 && opts.audio_bitrate != 128 &&
                opts.audio_bitrate != 160 && opts.audio_bitrate != 192 &&
                opts.audio_bitrate != 256 && opts.audio_bitrate != 320)
            {
                fprintf(stderr, "Warning: Non-standard bitrate %d, using 192\n", opts.audio_bitrate);
                opts.audio_bitrate = 192;
            }
        }
        else if (arg == "--no-suffix")
        {
            opts.add_suffix = false;
        }
        else if (arg == "--verbose")
        {
            opts.verbose = true;
        }
        else if (arg == "-o" || arg == "--output")
        {
            if (i + 1 < argc)
                opts.output_file = argv[++i];
        }
        else if (arg == "--title" && i + 1 < argc)
            opts.title = argv[++i];
        else if (arg == "--author" && i + 1 < argc)
            opts.author = argv[++i];
        else if (arg == "--album" && i + 1 < argc)
            opts.album = argv[++i];
        else if (arg == "--system" && i + 1 < argc)
            opts.system = argv[++i];
        else if (arg == "--date" && i + 1 < argc)
            opts.date = argv[++i];
        else if (arg == "--notes" && i + 1 < argc)
            opts.notes = argv[++i];
        else if (arg[0] != '-')
        {
            if (opts.input_file.empty())
                opts.input_file = arg;
            else if (opts.output_file.empty())
                opts.output_file = arg;
        }
    }

    return true;
}

// Convert using libADLMIDI (MIDI-style formats)
int convert_midi_style(const Options& opts)
{
    printf("Format category: MIDI-style (using libADLMIDI)\n");
    printf("Input:  %s\n", opts.input_file.c_str());
    printf("Output: %s\n", opts.output_file.c_str());

    // Warn about inapplicable options
    if (opts.subsong >= 0)
        printf("Note: --subsong option ignored for MIDI-style formats\n");
    if (!opts.loop_once)
        printf("Note: --no-loop option ignored for MIDI-style formats\n");
    if (opts.max_length_sec != 600)
        printf("Note: --length option ignored for MIDI-style formats\n");

    // Auto-detect bank if not specified
    int final_bank = opts.bank;
    if (final_bank < 0)
    {
        BankDetection detection = BankDetector::detect(opts.input_file.c_str());

        if (detection.confidence < 0.80f && opts.interactive)
        {
            final_bank = prompt_user_for_bank(detection);
        }
        else
        {
            final_bank = detection.bank_id;
            printf("Auto-detected bank: %d - %s (%.0f%% confidence)\n",
                   final_bank, detection.reason.c_str(), detection.confidence * 100);
        }
    }
    else
    {
        printf("Using bank: %d\n", final_bank);
    }

    // Initialize libADLMIDI
    printf("Initializing converter...\n");
    ADL_MIDIPlayer* player = adl_init(44100);
    if (!player)
    {
        fprintf(stderr, "Error: Failed to initialize libADLMIDI: %s\n", adl_errorString());
        return 2;
    }

    adl_setBank(player, final_bank);
    adl_setVolumeRangeModel(player, opts.vol_model);
    adl_setNumChips(player, 1);
    adl_setSoftPanEnabled(player, 1);

    // Load music file
    printf("Loading file...\n");

    std::string ext = getExtension(opts.input_file);
    bool is_hmp = (ext == "hmp" || ext == "hmi");

    std::vector<uint8_t> midi_data;
    int load_result = 0;

    if (is_hmp)
    {
        printf("Detected HMP format, converting to MIDI...\n");
        std::string hmp_error;

        if (!loadHMPasMIDI(opts.input_file.c_str(), midi_data, hmp_error))
        {
            fprintf(stderr, "Error: Failed to convert HMP file: %s\n", hmp_error.c_str());
            adl_close(player);
            return 2;
        }

        printf("HMP conversion successful (%zu bytes)\n", midi_data.size());
        load_result = adl_openData(player, midi_data.data(), midi_data.size());
    }
    else
    {
        load_result = adl_openFile(player, opts.input_file.c_str());
    }

    if (load_result < 0)
    {
        fprintf(stderr, "Error: Failed to load file: %s\n", adl_errorInfo(player));
        adl_close(player);
        return 2;
    }

    // Create GD3 tag
    GD3Tag* gd3_tag = nullptr;
    if (!opts.title.empty() || !opts.author.empty() || !opts.album.empty())
    {
        gd3_tag = new GD3Tag();
        gd3_tag->title_en = opts.title;
        gd3_tag->author_en = opts.author;
        gd3_tag->album_en = opts.album;
        gd3_tag->system_en = opts.system;
        gd3_tag->date = opts.date;
        gd3_tag->converted_by = "fmconv";
        gd3_tag->notes = opts.notes;
    }

    // Replace OPL chip with VGM writer
    printf("Converting to VGM format...\n");
    std::vector<uint8_t> vgm_data;

    auto* midiplay = static_cast<MIDIplay*>(player->adl_midiPlayer);
    auto* synth = midiplay->m_synth.get();
    auto* vgm_chip = new VGMOPL3(vgm_data, gd3_tag);
    synth->m_chips[0].reset(vgm_chip);

    synth->updateChannelCategories();
    synth->silenceAll();

    // Process the entire file
    int16_t discard[4];
    size_t sample_count = 0;
    while (adl_play(player, 2, discard) > 0)
    {
        vgm_chip->accumulateDelay(1);
        sample_count++;

        if (sample_count % 44100 == 0)
        {
            printf("  %.1f seconds...\n", sample_count / 44100.0);
        }
    }

    vgm_chip->finalize();

    printf("Conversion complete: %zu samples (%.2f seconds)\n",
           sample_count, sample_count / 44100.0);

    // Write output file
    size_t bytes_written = writeOutputFile(opts.output_file, vgm_data, opts);
    if (bytes_written == 0)
    {
        adl_close(player);
        delete gd3_tag;
        return 3;
    }

    adl_close(player);
    delete gd3_tag;

    return 0;
}

// Convert using AdPlug (native OPL formats)
int convert_native_opl(const Options& opts)
{
    printf("Format category: Native OPL (using AdPlug)\n");
    printf("Input:  %s\n", opts.input_file.c_str());
    printf("Output: %s\n", opts.output_file.c_str());

    // Warn about inapplicable options
    if (opts.bank >= 0)
        printf("Note: --bank option ignored for native OPL formats (embedded instruments)\n");
    if (opts.vol_model != 0)
        printf("Note: --vol-model option ignored for native OPL formats\n");

    // Create VGM-capturing OPL
    CVgmOpl vgm_opl;

    // Try to load with AdPlug
    printf("Loading with AdPlug...\n");
    CPlayer* player = CAdPlug::factory(opts.input_file, &vgm_opl);

    if (!player)
    {
        fprintf(stderr, "Error: AdPlug could not load file: %s\n", opts.input_file.c_str());
        fprintf(stderr, "       File format may not be supported or file may be corrupt.\n");
        return 2;
    }

    // Get file info
    std::string format_type = player->gettype();
    printf("Format: %s\n", format_type.c_str());

    std::string title = player->gettitle();
    std::string author = player->getauthor();
    std::string desc = player->getdesc();
    if (!title.empty())
        printf("Title:  %s\n", title.c_str());
    if (!author.empty())
        printf("Author: %s\n", author.c_str());

    unsigned int subsongs = player->getsubsongs();
    if (subsongs > 1)
        printf("Subsongs: %u\n", subsongs);

    // Select subsong
    if (opts.subsong >= 0)
    {
        printf("Playing subsong %d\n", opts.subsong);
        player->rewind(opts.subsong);
    }
    else
    {
        player->rewind(0);
    }

    // Calculate max samples
    uint32_t max_samples = (uint32_t)opts.max_length_sec * 44100;

    // Conversion loop
    printf("Converting...\n");

    uint32_t total_updates = 0;
    uint32_t samples_generated = 0;

    // Track fractional samples to avoid timing drift
    double fractional_samples = 0.0;

    // Loop detection - track the first occurrence of each order position
    // so we can find where the loop target first appeared
    // Map: order -> (sample_pos, write_index) for first occurrence
    std::map<unsigned int, std::pair<uint32_t, size_t>> first_occurrence;

    // Track previous position for loop detection
    unsigned int prev_order = player->getorder();

    while (samples_generated < max_samples)
    {
        float refresh = player->getrefresh();

        if (refresh <= 0 || refresh > 10000)
            refresh = 70.0f;

        unsigned int curr_order = player->getorder();

        // Record first occurrence of this order position
        if (opts.loop_once && first_occurrence.find(curr_order) == first_occurrence.end())
        {
            first_occurrence[curr_order] = std::make_pair(samples_generated, vgm_opl.getWriteCount());
        }

        prev_order = curr_order;

        bool still_playing = player->update();

        // Calculate samples using fractional accumulator to prevent timing drift
        double samples_per_tick = 44100.0 / (double)refresh;
        fractional_samples += samples_per_tick;
        uint32_t samples = (uint32_t)fractional_samples;
        fractional_samples -= samples;

        vgm_opl.advanceSamples(samples);
        samples_generated += samples;
        total_updates++;

        if (!still_playing)
        {
            // AdPlug signaled end - check if it's a loop or true end
            // When AdPlug detects a loop, the position is at the loop target
            unsigned int end_order = player->getorder();

            // If we ended at an earlier position than where we were, it's a loop
            if (end_order < prev_order || (end_order == 0 && prev_order > 0))
            {
                if (opts.verbose)
                    printf("Loop detected! Order %u -> order %u\n", prev_order, end_order);

                if (opts.loop_once)
                {
                    // Find where this order first appeared in our recording
                    auto it = first_occurrence.find(end_order);
                    if (it != first_occurrence.end())
                    {
                        uint32_t loop_sample_pos = it->second.first;
                        size_t loop_write_index = it->second.second;

                        printf("Loop point: order %u (first seen at sample %u, write %zu)\n",
                               end_order, loop_sample_pos, loop_write_index);

                        vgm_opl.setLoopPoint(loop_write_index, loop_sample_pos);
                    }
                    else
                    {
                        // Loop target wasn't seen during playback (unusual)
                        printf("Warning: Loop target order %u not found in recording\n", end_order);
                    }
                }
            }
            else
            {
                // True end - no loop (or loop to same/later position which is unusual)
                if (opts.verbose)
                    printf("Song ended at update %u (no loop detected)\n", total_updates);
            }
            break;
        }

        if (samples_generated % (44100 * 10) < samples)
        {
            printf("  %.1f seconds...\n", samples_generated / 44100.0f);
        }
    }

    // Create GD3 tag with metadata from file and/or command line
    GD3Tag* gd3_tag = nullptr;
    if (!opts.title.empty() || !opts.author.empty() || !opts.album.empty() ||
        !title.empty() || !author.empty())
    {
        gd3_tag = new GD3Tag();
        gd3_tag->title_en = opts.title.empty() ? title : opts.title;
        gd3_tag->author_en = opts.author.empty() ? author : opts.author;
        gd3_tag->album_en = opts.album;
        gd3_tag->system_en = opts.system.empty() ? format_type : opts.system;
        gd3_tag->date = opts.date;
        gd3_tag->converted_by = "fmconv (AdPlug)";
        // Use description for notes if no custom notes provided
        if (opts.notes.empty() && !desc.empty()) {
            std::string notes = desc;
            if (notes.length() > 256) {
                notes = notes.substr(0, 253) + "...";
            }
            gd3_tag->notes = notes;
        } else {
            gd3_tag->notes = opts.notes;
        }
    }

    // Generate VGM
    std::vector<uint8_t> vgm_data = vgm_opl.generateVgm(gd3_tag);

    float duration = samples_generated / 44100.0f;
    printf("Detected chip: %s\n", vgm_opl.getChipTypeString());
    printf("Conversion complete: %u updates, %.2f seconds\n", total_updates, duration);

    // Report loop status
    if (vgm_opl.hasLoopPoint())
    {
        printf("Loop: Yes (VGM will loop seamlessly)\n");
    }
    else if (!opts.loop_once)
    {
        printf("Loop: Disabled (--no-loop specified)\n");
    }
    else
    {
        printf("Loop: No loop detected\n");
    }

    printf("VGM size: %zu bytes\n", vgm_data.size());

    // Write output file
    size_t bytes_written = writeOutputFile(opts.output_file, vgm_data, opts);
    if (bytes_written == 0)
    {
        delete player;
        delete gd3_tag;
        return 3;
    }

    delete player;
    delete gd3_tag;

    return 0;
}

// Pass through VGM/VGZ input to FM9 (for adding audio/fx to existing VGM files)
int convert_vgm_passthrough(const Options& opts)
{
    printf("Format category: VGM/VGZ pass-through\n");
    printf("Input:  %s\n", opts.input_file.c_str());
    printf("Output: %s\n", opts.output_file.c_str());

    // Read input file
    std::ifstream infile(opts.input_file, std::ios::binary | std::ios::ate);
    if (!infile)
    {
        fprintf(stderr, "Error: Failed to open input file: %s\n", opts.input_file.c_str());
        return 2;
    }

    std::streamsize file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<uint8_t> file_data(static_cast<size_t>(file_size));
    if (!infile.read(reinterpret_cast<char*>(file_data.data()), file_size))
    {
        fprintf(stderr, "Error: Failed to read input file\n");
        return 2;
    }
    infile.close();

    printf("Read %zu bytes\n", file_data.size());

    // Check if it's gzipped (VGZ)
    std::vector<uint8_t> vgm_data;
    bool was_compressed = false;

    if (file_data.size() >= 2 && file_data[0] == 0x1f && file_data[1] == 0x8b)
    {
        printf("Detected gzip compression, decompressing...\n");
        vgm_data = gzipDecompress(file_data);
        if (vgm_data.empty())
        {
            fprintf(stderr, "Error: Failed to decompress VGZ file\n");
            return 2;
        }
        was_compressed = true;
        printf("Decompressed to %zu bytes\n", vgm_data.size());
    }
    else
    {
        vgm_data = std::move(file_data);
    }

    // Validate VGM header
    if (vgm_data.size() < 64 ||
        vgm_data[0] != 'V' || vgm_data[1] != 'g' ||
        vgm_data[2] != 'm' || vgm_data[3] != ' ')
    {
        fprintf(stderr, "Error: Not a valid VGM file (missing 'Vgm ' header)\n");
        return 2;
    }

    // Get VGM version
    uint32_t version = vgm_data[8] | (vgm_data[9] << 8) |
                       (vgm_data[10] << 16) | (vgm_data[11] << 24);
    printf("VGM version: %X.%02X\n", version >> 8, version & 0xFF);

    // Get total samples for duration calculation
    uint32_t total_samples = vgm_data[0x18] | (vgm_data[0x19] << 8) |
                             (vgm_data[0x1A] << 16) | (vgm_data[0x1B] << 24);
    float duration = total_samples / 44100.0f;
    printf("Duration: %.2f seconds (%u samples)\n", duration, total_samples);

    // Check for existing FM9 extension and strip it if present
    // (in case someone is re-processing an FM9 file)
    size_t fm9_pos = 0;
    for (size_t i = 64; i < vgm_data.size() - 4; i++)
    {
        if (vgm_data[i] == 'F' && vgm_data[i+1] == 'M' &&
            vgm_data[i+2] == '9' && vgm_data[i+3] == '0')
        {
            fm9_pos = i;
            break;
        }
    }

    if (fm9_pos > 0)
    {
        printf("Note: Stripping existing FM9 extension at offset %zu\n", fm9_pos);
        vgm_data.resize(fm9_pos);
    }

    // Check for existing GD3 tag and parse it
    // GD3 offset is stored at 0x14 (relative to 0x14)
    GD3Tag existing_gd3;
    bool has_existing_gd3 = false;
    uint32_t gd3_offset = 0;

    if (vgm_data.size() >= 0x18)
    {
        uint32_t gd3_rel = vgm_data[0x14] | (vgm_data[0x15] << 8) |
                          (vgm_data[0x16] << 16) | (vgm_data[0x17] << 24);

        if (gd3_rel > 0)
        {
            gd3_offset = 0x14 + gd3_rel;
            if (gd3_offset < vgm_data.size())
            {
                if (existing_gd3.parse(vgm_data.data() + gd3_offset, vgm_data.size() - gd3_offset))
                {
                    has_existing_gd3 = true;
                    printf("Found existing GD3 tag at offset 0x%X\n", gd3_offset);
                    if (!existing_gd3.title_en.empty())
                        printf("  Title: %s\n", existing_gd3.title_en.c_str());
                    if (!existing_gd3.author_en.empty())
                        printf("  Author: %s\n", existing_gd3.author_en.c_str());
                }
            }
        }
    }

    // Check if any CLI metadata options were provided
    bool has_cli_metadata = !opts.title.empty() || !opts.author.empty() ||
                            !opts.album.empty() || !opts.system.empty() ||
                            !opts.date.empty() || !opts.notes.empty();

    // If CLI options provided, merge them with existing GD3 (CLI overrides existing)
    if (has_cli_metadata)
    {
        GD3Tag new_gd3;

        if (has_existing_gd3)
        {
            // Start with existing values
            new_gd3 = existing_gd3;
        }

        // Override with CLI values where provided
        if (!opts.title.empty())
            new_gd3.title_en = opts.title;
        if (!opts.author.empty())
            new_gd3.author_en = opts.author;
        if (!opts.album.empty())
            new_gd3.album_en = opts.album;
        if (!opts.system.empty())
            new_gd3.system_en = opts.system;
        if (!opts.date.empty())
            new_gd3.date = opts.date;
        if (!opts.notes.empty())
            new_gd3.notes = opts.notes;

        // Mark as converted by fmconv
        if (new_gd3.converted_by.empty())
            new_gd3.converted_by = "fmconv";

        // Serialize new GD3 tag
        std::vector<uint8_t> gd3_data = new_gd3.serialize();

        // Strip old GD3 tag from VGM data (if present)
        if (has_existing_gd3 && gd3_offset > 0 && gd3_offset < vgm_data.size())
        {
            vgm_data.resize(gd3_offset);
        }

        // Update GD3 offset in header
        uint32_t new_gd3_offset = static_cast<uint32_t>(vgm_data.size()) - 0x14;
        vgm_data[0x14] = (new_gd3_offset >> 0) & 0xFF;
        vgm_data[0x15] = (new_gd3_offset >> 8) & 0xFF;
        vgm_data[0x16] = (new_gd3_offset >> 16) & 0xFF;
        vgm_data[0x17] = (new_gd3_offset >> 24) & 0xFF;

        // Append new GD3 data
        vgm_data.insert(vgm_data.end(), gd3_data.begin(), gd3_data.end());

        // Update EOF offset in header (relative to 0x04)
        uint32_t eof_offset = static_cast<uint32_t>(vgm_data.size()) - 4;
        vgm_data[0x04] = (eof_offset >> 0) & 0xFF;
        vgm_data[0x05] = (eof_offset >> 8) & 0xFF;
        vgm_data[0x06] = (eof_offset >> 16) & 0xFF;
        vgm_data[0x07] = (eof_offset >> 24) & 0xFF;

        printf("Updated GD3 metadata\n");
    }

    printf("VGM data size: %zu bytes\n", vgm_data.size());

    // Write output
    size_t bytes_written = writeOutputFile(opts.output_file, vgm_data, opts);
    if (bytes_written == 0)
    {
        return 3;
    }

    return 0;
}

#ifdef HAVE_OPENMPT
// Convert using OpenMPT (tracker formats with OPL and/or samples)
int convert_openmpt(const Options& opts)
{
    printf("Format category: Tracker (using OpenMPT)\n");
    printf("Input:  %s\n", opts.input_file.c_str());
    printf("Output: %s\n", opts.output_file.c_str());

    // Create context
    openmpt_export_context* ctx = openmpt_export_create();
    if (!ctx)
    {
        fprintf(stderr, "Error: Failed to create OpenMPT context\n");
        return 2;
    }

    // Load file
    printf("Loading with OpenMPT...\n");
    if (!openmpt_export_load(ctx, opts.input_file.c_str()))
    {
        fprintf(stderr, "Error: %s\n", openmpt_export_get_error(ctx));
        openmpt_export_destroy(ctx);
        return 2;
    }

    // Get file info
    const char* title = openmpt_export_get_title(ctx);
    const char* artist = openmpt_export_get_artist(ctx);
    const char* message = openmpt_export_get_message(ctx);
    const char* tracker = openmpt_export_get_tracker(ctx);
    const char* format = openmpt_export_get_format(ctx);
    const char* format_name = openmpt_export_get_format_name(ctx);
    bool has_opl = openmpt_export_has_opl(ctx) != 0;
    bool has_samples = openmpt_export_has_samples(ctx) != 0;

    printf("Format: %s\n", format);
    if (title && title[0])
        printf("Title:  %s\n", title);
    if (artist && artist[0])
        printf("Artist: %s\n", artist);
    if (tracker && tracker[0])
        printf("Tracker: %s\n", tracker);
    printf("Contains: %s%s%s\n",
           has_opl ? "OPL instruments" : "",
           (has_opl && has_samples) ? " + " : "",
           has_samples ? "Sample instruments" : "");

    if (!has_opl && !has_samples)
    {
        fprintf(stderr, "Error: File contains no playable instruments\n");
        openmpt_export_destroy(ctx);
        return 2;
    }

    std::vector<uint8_t> vgm_data;
    std::vector<int16_t> pcm_data;
    uint32_t sample_rate = 44100;

    // Render OPL if present
    if (has_opl)
    {
        printf("Rendering OPL instruments to VGM...\n");
        if (!openmpt_export_render_opl(ctx, sample_rate, opts.max_length_sec))
        {
            fprintf(stderr, "Error: %s\n", openmpt_export_get_error(ctx));
            openmpt_export_destroy(ctx);
            return 2;
        }

        size_t vgm_size = 0;
        const uint8_t* vgm_ptr = openmpt_export_get_vgm_data(ctx, &vgm_size);
        if (vgm_ptr && vgm_size > 0)
        {
            vgm_data.assign(vgm_ptr, vgm_ptr + vgm_size);
            printf("OPL rendered: %zu bytes VGM\n", vgm_size);
        }
    }

    // Render samples if present (reload context for clean render)
    if (has_samples)
    {
        // Need to reload for sample-only render
        openmpt_export_context* sample_ctx = openmpt_export_create();
        if (sample_ctx && openmpt_export_load(sample_ctx, opts.input_file.c_str()))
        {
            printf("Rendering sample instruments to PCM...\n");
            if (openmpt_export_render_samples(sample_ctx, sample_rate, opts.max_length_sec))
            {
                size_t pcm_size = 0;
                const int16_t* pcm_ptr = openmpt_export_get_pcm_data(sample_ctx, &pcm_size);
                if (pcm_ptr && pcm_size > 0)
                {
                    pcm_data.assign(pcm_ptr, pcm_ptr + pcm_size);
                    printf("Samples rendered: %zu samples (%.2f seconds)\n",
                           pcm_size / 2, (pcm_size / 2) / (float)sample_rate);
                }
            }
        }
        if (sample_ctx)
            openmpt_export_destroy(sample_ctx);
    }

    openmpt_export_destroy(ctx);

    // Handle output based on what we have
    if (vgm_data.empty() && pcm_data.empty())
    {
        fprintf(stderr, "Error: No audio data generated\n");
        return 2;
    }

    // If we only have samples (no OPL), handle based on output format
    if (vgm_data.empty() && !pcm_data.empty())
    {
        // For VGM/VGZ output, there's nothing useful to output
        if (opts.output_format != OutputFormat::FM9)
        {
            fprintf(stderr, "Error: This file contains only sample-based instruments (no OPL).\n");
            fprintf(stderr, "       VGM/VGZ output requires OPL content. Use FM9 format instead:\n");
            fprintf(stderr, "         fmconv \"%s\" --format fm9\n", opts.input_file.c_str());
            return 2;
        }

        // For FM9, create a minimal timing-only VGM for sample sync
        printf("No OPL instruments - creating timing-only VGM for sample sync\n");

        // Calculate total samples from PCM data (stereo, so divide by 2)
        uint32_t total_samples = static_cast<uint32_t>(pcm_data.size() / 2);

        // Create minimal VGM: header + wait commands + end
        vgm_data.resize(256, 0);  // 256-byte header

        // VGM magic "Vgm "
        vgm_data[0] = 'V'; vgm_data[1] = 'g';
        vgm_data[2] = 'm'; vgm_data[3] = ' ';

        // Version 1.51
        vgm_data[0x08] = 0x51; vgm_data[0x09] = 0x01;

        // Total samples
        vgm_data[0x18] = (total_samples >> 0) & 0xFF;
        vgm_data[0x19] = (total_samples >> 8) & 0xFF;
        vgm_data[0x1A] = (total_samples >> 16) & 0xFF;
        vgm_data[0x1B] = (total_samples >> 24) & 0xFF;

        // VGM data offset (relative to 0x34) = 256 - 0x34 = 204
        uint32_t data_offset = 256 - 0x34;
        vgm_data[0x34] = (data_offset >> 0) & 0xFF;
        vgm_data[0x35] = (data_offset >> 8) & 0xFF;
        vgm_data[0x36] = (data_offset >> 16) & 0xFF;
        vgm_data[0x37] = (data_offset >> 24) & 0xFF;

        // Add wait commands for the duration
        uint32_t remaining = total_samples;
        while (remaining > 0)
        {
            if (remaining <= 16)
            {
                // Short wait: 0x70-0x7F = wait 1-16 samples
                vgm_data.push_back(0x6F + static_cast<uint8_t>(remaining));
                remaining = 0;
            }
            else if (remaining <= 65535)
            {
                // Wait N samples: 0x61 nn nn
                vgm_data.push_back(0x61);
                vgm_data.push_back(remaining & 0xFF);
                vgm_data.push_back((remaining >> 8) & 0xFF);
                remaining = 0;
            }
            else
            {
                // Wait max and continue
                vgm_data.push_back(0x61);
                vgm_data.push_back(0xFF);
                vgm_data.push_back(0xFF);
                remaining -= 65535;
            }
        }

        // End of sound data
        vgm_data.push_back(0x66);

        // Update EOF offset (relative to 0x04)
        uint32_t eof_offset = static_cast<uint32_t>(vgm_data.size()) - 4;
        vgm_data[0x04] = (eof_offset >> 0) & 0xFF;
        vgm_data[0x05] = (eof_offset >> 8) & 0xFF;
        vgm_data[0x06] = (eof_offset >> 16) & 0xFF;
        vgm_data[0x07] = (eof_offset >> 24) & 0xFF;

        printf("Created timing VGM: %zu bytes, %u samples (%.2f seconds)\n",
               vgm_data.size(), total_samples, total_samples / 44100.0f);
    }

    // Create GD3 tag with metadata from module and/or command line
    GD3Tag* gd3_tag = nullptr;
    if (!opts.title.empty() || !opts.author.empty() || !opts.album.empty() ||
        (title && title[0]) || (artist && artist[0]))
    {
        gd3_tag = new GD3Tag();
        // Command line overrides module metadata
        gd3_tag->title_en = opts.title.empty() ? (title ? title : "") : opts.title;
        gd3_tag->author_en = opts.author.empty() ? (artist ? artist : "") : opts.author;
        gd3_tag->album_en = opts.album;
        gd3_tag->system_en = opts.system.empty() ? (format_name ? format_name : "") : opts.system;
        gd3_tag->date = opts.date;
        gd3_tag->converted_by = tracker && tracker[0] ?
            std::string("fmconv (") + tracker + ")" : "fmconv (OpenMPT)";
        // Include message in notes if present and no custom notes provided
        if (opts.notes.empty() && message && message[0]) {
            // Truncate long messages
            std::string msg(message);
            if (msg.length() > 256) {
                msg = msg.substr(0, 253) + "...";
            }
            gd3_tag->notes = msg;
        } else {
            gd3_tag->notes = opts.notes;
        }
    }

    // Add GD3 tag to VGM if we have metadata
    if (gd3_tag)
    {
        // TODO: Inject GD3 tag into VGM data
        // For now, the VGM from openmpt_export doesn't include GD3
    }

    // Write output
    size_t bytes_written = 0;

    if (opts.output_format == OutputFormat::FM9 && !pcm_data.empty())
    {
        // FM9 with embedded sample audio
        FM9Writer writer;
        writer.setVGMData(vgm_data);

        // Set source format from input file extension
        std::string ext = getExtension(opts.input_file);
        writer.setSourceFormat(ext);

        // Encode audio - MP3 by default, WAV if --uncompressed-audio specified
        std::vector<uint8_t> audio_data;
        uint8_t audio_format_code = FM9_AUDIO_WAV;
        size_t pcm_sample_count = pcm_data.size() / 2;  // Stereo pairs

#ifdef HAVE_LAME
        if (!opts.no_compress_audio)
        {
            // Encode to MP3
            printf("Encoding audio to MP3 (%d kbps)...\n", opts.audio_bitrate);

            MP3EncoderConfig mp3_config;
            mp3_config.sample_rate = sample_rate;
            mp3_config.channels = 2;
            mp3_config.bitrate_kbps = opts.audio_bitrate;

            std::string mp3_error;
            audio_data = encodePCMtoMP3(pcm_data.data(), pcm_sample_count, mp3_config, &mp3_error);

            if (audio_data.empty())
            {
                fprintf(stderr, "Warning: MP3 encoding failed: %s\n", mp3_error.c_str());
                fprintf(stderr, "         Falling back to WAV\n");
            }
            else
            {
                audio_format_code = FM9_AUDIO_MP3;
                printf("MP3 encoded: %zu bytes (%.1f%% of WAV size)\n",
                       audio_data.size(),
                       100.0 * audio_data.size() / (pcm_data.size() * sizeof(int16_t)));
            }
        }
#else
        if (!opts.no_compress_audio)
        {
            printf("Note: MP3 encoding not available (LAME not linked), using WAV\n");
        }
#endif

        // Fall back to WAV if MP3 encoding failed or was disabled
        if (audio_data.empty())
        {
            // Convert PCM to WAV format for embedding
            size_t num_samples = pcm_data.size();
            size_t data_size = num_samples * sizeof(int16_t);

            // WAV header (44 bytes)
            audio_data.resize(44 + data_size);
            uint8_t* p = audio_data.data();

            // RIFF header
            memcpy(p, "RIFF", 4); p += 4;
            uint32_t file_size = static_cast<uint32_t>(36 + data_size);
            memcpy(p, &file_size, 4); p += 4;
            memcpy(p, "WAVE", 4); p += 4;

            // fmt chunk
            memcpy(p, "fmt ", 4); p += 4;
            uint32_t fmt_size = 16;
            memcpy(p, &fmt_size, 4); p += 4;
            uint16_t wav_audio_format = 1;  // PCM
            memcpy(p, &wav_audio_format, 2); p += 2;
            uint16_t num_channels = 2;  // Stereo
            memcpy(p, &num_channels, 2); p += 2;
            memcpy(p, &sample_rate, 4); p += 4;
            uint32_t byte_rate = sample_rate * num_channels * sizeof(int16_t);
            memcpy(p, &byte_rate, 4); p += 4;
            uint16_t block_align = num_channels * sizeof(int16_t);
            memcpy(p, &block_align, 2); p += 2;
            uint16_t bits_per_sample = 16;
            memcpy(p, &bits_per_sample, 2); p += 2;

            // data chunk
            memcpy(p, "data", 4); p += 4;
            uint32_t data_chunk_size = static_cast<uint32_t>(data_size);
            memcpy(p, &data_chunk_size, 4); p += 4;

            // PCM samples
            memcpy(p, pcm_data.data(), data_size);

            audio_format_code = FM9_AUDIO_WAV;
        }

        // Set audio data
        writer.setAudioData(audio_data, audio_format_code);

        // Set cover image if provided
        if (!opts.image_file.empty())
        {
            if (!writer.setImageFile(opts.image_file, opts.dither_image))
            {
                fprintf(stderr, "Warning: %s\n", writer.getError().c_str());
            }
        }

        const char* audio_type = (audio_format_code == FM9_AUDIO_MP3) ? "MP3" : "WAV";
        printf("Writing: %s (FM9 with embedded %s audio)\n", opts.output_file.c_str(), audio_type);
        bytes_written = writer.write(opts.output_file);
        if (bytes_written == 0)
        {
            fprintf(stderr, "Error: %s\n", writer.getError().c_str());
            delete gd3_tag;
            return 3;
        }

        printf("Success! FM9 size: %zu bytes (VGM: %zu, Audio: %zu bytes %s)\n",
               bytes_written, vgm_data.size(), audio_data.size(), audio_type);
    }
    else
    {
        // Standard VGM/VGZ/FM9 output (no embedded audio)
        if (!pcm_data.empty())
        {
            printf("Warning: This file contains sample-based instruments that will not be\n");
            printf("         included in VGM/VGZ output. Use FM9 format to include them:\n");
            printf("           fmconv \"%s\" --format fm9\n", opts.input_file.c_str());
        }

        bytes_written = writeOutputFile(opts.output_file, vgm_data, opts);
        if (bytes_written == 0)
        {
            delete gd3_tag;
            return 3;
        }
    }

    delete gd3_tag;
    return 0;
}
#endif // HAVE_OPENMPT

int main(int argc, char** argv)
{
    Options opts;

    if (!parse_args(argc, argv, opts))
        return 0;

    if (opts.show_banks)
    {
        show_banks();
        return 0;
    }

    if (opts.show_vol_models)
    {
        show_vol_models();
        return 0;
    }

    if (opts.show_formats)
    {
        show_formats();
        return 0;
    }

    if (opts.input_file.empty())
    {
        show_usage(argv[0]);
        return 1;
    }

    // Generate output filename
    // Cases:
    // 1. No output specified -> same dir as input, add suffix, use .vgz/.vgm
    // 2. Directory only (ends with / or \) -> that dir, add suffix
    // 3. Full filename specified -> use as-is (no suffix added)

    bool user_specified_filename = false;

    if (opts.output_file.empty())
    {
        // Case 1: No output specified - use input directory
        opts.output_file = getDirectory(opts.input_file) + getBasename(opts.input_file);
    }
    else if (isDirectory(opts.output_file))
    {
        // Case 2: Directory only - use that directory with input basename
        opts.output_file = opts.output_file + getBasename(opts.input_file);
    }
    else
    {
        // Case 3: Full filename specified - use as-is
        user_specified_filename = true;
    }

    // Add format suffix (e.g., _RAD) if enabled and not user-specified filename
    // Skip suffix for VGM/VGZ/FM9 input (pass-through doesn't need it)
    FormatCategory input_category = categorizeFormat(opts.input_file);
    if (opts.add_suffix && !user_specified_filename && input_category != FormatCategory::VGM_INPUT)
    {
        std::string suffix = getExtensionUpper(opts.input_file);
        if (!suffix.empty())
        {
            opts.output_file += "_" + suffix;
        }
    }

    // Add extension based on output format
    if (!user_specified_filename)
    {
        switch (opts.output_format)
        {
        case OutputFormat::FM9:
            opts.output_file += ".fm9";
            break;
        case OutputFormat::VGZ:
            opts.output_file += ".vgz";
            break;
        case OutputFormat::VGM:
            opts.output_file += ".vgm";
            break;
        }
    }

    // Warn if audio/fx specified with non-FM9 format
    if (opts.output_format != OutputFormat::FM9)
    {
        if (!opts.audio_file.empty())
        {
            fprintf(stderr, "Warning: --audio ignored (only supported with FM9 format)\n");
            opts.audio_file.clear();
        }
        if (!opts.fx_file.empty())
        {
            fprintf(stderr, "Warning: --fx ignored (only supported with FM9 format)\n");
            opts.fx_file.clear();
        }
        if (!opts.image_file.empty())
        {
            fprintf(stderr, "Warning: --image ignored (only supported with FM9 format)\n");
            opts.image_file.clear();
        }
    }

    // Determine format category
    FormatCategory category = categorizeFormat(opts.input_file);

    switch (category)
    {
    case FormatCategory::VGM_INPUT:
        return convert_vgm_passthrough(opts);

    case FormatCategory::MIDI_STYLE:
        return convert_midi_style(opts);

    case FormatCategory::NATIVE_OPL:
        return convert_native_opl(opts);

    case FormatCategory::TRACKER_FORMAT:
#ifdef HAVE_OPENMPT
        return convert_openmpt(opts);
#else
        // Fallback when OpenMPT not available
        printf("Note: OpenMPT not available, trying AdPlug...\n");
        return convert_native_opl(opts);
#endif

    case FormatCategory::UNKNOWN:
        // Try AdPlug first (it's more flexible)
        printf("Unknown format, trying AdPlug...\n");
        return convert_native_opl(opts);
    }

    return 0;
}
