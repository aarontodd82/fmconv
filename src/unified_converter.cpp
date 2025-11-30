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

// Format categories
enum class FormatCategory
{
    MIDI_STYLE,     // Use libADLMIDI (needs bank)
    NATIVE_OPL,     // Use AdPlug (embedded instruments)
    UNKNOWN
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
    bool compress = true;       // Gzip to .vgz (default on)
    bool add_suffix = true;     // Add format suffix like _RAD (default on)

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

// Write data to file, optionally gzip compressed
// Returns actual bytes written (for reporting)
static size_t writeOutput(const std::string& filename, const std::vector<uint8_t>& data, bool compress, bool* was_compressed = nullptr)
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

// Determine format category from extension
static FormatCategory categorizeFormat(const std::string& filename)
{
    std::string ext = getExtension(filename);

    // MIDI-style formats (need bank selection)
    if (ext == "mid" || ext == "midi" || ext == "smf" || ext == "kar" ||
        ext == "rmi" || ext == "xmi" || ext == "mus" || ext == "hmp" ||
        ext == "hmi" || ext == "klm")
    {
        return FormatCategory::MIDI_STYLE;
    }

    // Native OPL formats (embedded instruments)
    // This covers all AdPlug-supported formats
    if (ext == "a2m" || ext == "a2t" || ext == "adl" || ext == "adlib" ||
        ext == "amd" || ext == "bam" || ext == "bmf" || ext == "cff" ||
        ext == "cmf" || ext == "d00" || ext == "dfm" || ext == "dmo" ||
        ext == "dro" || ext == "dtm" || ext == "got" || ext == "ha2" ||
        ext == "hsc" || ext == "hsp" || ext == "hsq" || ext == "imf" ||
        ext == "ims" || ext == "jbm" || ext == "ksm" || ext == "laa" ||
        ext == "lds" || ext == "m" || ext == "mad" || ext == "mdi" ||
        ext == "mdy" || ext == "mkf" || ext == "mkj" || ext == "msc" ||
        ext == "mtk" || ext == "mtr" || ext == "mus" || ext == "pis" ||
        ext == "plx" || ext == "rac" || ext == "rad" || ext == "raw" ||
        ext == "rix" || ext == "rol" || ext == "s3m" || ext == "sa2" ||
        ext == "sat" || ext == "sci" || ext == "sdb" || ext == "sng" ||
        ext == "sop" || ext == "sqx" || ext == "wlf" || ext == "xad" ||
        ext == "xms" || ext == "xsm" || ext == "agd")
    {
        return FormatCategory::NATIVE_OPL;
    }

    return FormatCategory::UNKNOWN;
}

void show_usage(const char* program_name)
{
    printf("Usage: %s [OPTIONS] <input> [output]\n\n", program_name);
    printf("Convert game music formats to VGM for OPL2/OPL3 hardware\n\n");

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
    printf("  --no-compress        Output .vgm instead of .vgz (gzip compressed)\n");
    printf("  --no-suffix          Don't add format suffix (_RAD, _A2M, etc) to filename\n");
    printf("  --verbose            Verbose output\n");
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
    printf("  %s descent.hmp                    # MIDI-style, auto-detect bank\n", program_name);
    printf("  %s doom.mus doom.vgm --bank 16    # MIDI-style, DMX bank\n", program_name);
    printf("  %s game.rad                       # Native RAD, embedded instruments\n", program_name);
    printf("  %s tune.a2m tune.vgm              # Native A2M tracker\n", program_name);
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
        else if (arg == "--no-compress")
        {
            opts.compress = false;
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
    printf("Writing: %s%s\n", opts.output_file.c_str(), opts.compress ? " (gzip compressed)" : "");
    bool was_compressed = false;
    size_t bytes_written = writeOutput(opts.output_file, vgm_data, opts.compress, &was_compressed);
    if (bytes_written == 0)
    {
        adl_close(player);
        delete gd3_tag;
        return 3;
    }

    printf("Success! %s size: %zu bytes (uncompressed VGM: %zu bytes)\n",
           was_compressed ? "VGZ" : "VGM",
           bytes_written,
           vgm_data.size());

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
    printf("Format: %s\n", player->gettype().c_str());

    std::string title = player->gettitle();
    std::string author = player->getauthor();
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

    // Create GD3 tag
    GD3Tag* gd3_tag = nullptr;
    if (!opts.title.empty() || !opts.author.empty() || !opts.album.empty() ||
        !title.empty() || !author.empty())
    {
        gd3_tag = new GD3Tag();
        gd3_tag->title_en = opts.title.empty() ? title : opts.title;
        gd3_tag->author_en = opts.author.empty() ? author : opts.author;
        gd3_tag->album_en = opts.album;
        gd3_tag->system_en = opts.system;
        gd3_tag->date = opts.date;
        gd3_tag->converted_by = "fmconv (AdPlug)";
        gd3_tag->notes = opts.notes;
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
    printf("Writing: %s%s\n", opts.output_file.c_str(), opts.compress ? " (gzip compressed)" : "");
    bool was_compressed = false;
    size_t bytes_written = writeOutput(opts.output_file, vgm_data, opts.compress, &was_compressed);
    if (bytes_written == 0)
    {
        delete player;
        delete gd3_tag;
        return 3;
    }

    printf("Success! %s size: %zu bytes (uncompressed VGM: %zu bytes)\n",
           was_compressed ? "VGZ" : "VGM",
           bytes_written,
           vgm_data.size());

    delete player;
    delete gd3_tag;

    return 0;
}

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
    if (opts.add_suffix && !user_specified_filename)
    {
        std::string suffix = getExtensionUpper(opts.input_file);
        if (!suffix.empty())
        {
            opts.output_file += "_" + suffix;
        }
    }

    // Add extension based on compression setting
    if (!user_specified_filename)
    {
        opts.output_file += opts.compress ? ".vgz" : ".vgm";
    }

    // Determine format category
    FormatCategory category = categorizeFormat(opts.input_file);

    switch (category)
    {
    case FormatCategory::MIDI_STYLE:
        return convert_midi_style(opts);

    case FormatCategory::NATIVE_OPL:
        return convert_native_opl(opts);

    case FormatCategory::UNKNOWN:
        // Try AdPlug first (it's more flexible)
        printf("Unknown format, trying AdPlug...\n");
        return convert_native_opl(opts);
    }

    return 0;
}
