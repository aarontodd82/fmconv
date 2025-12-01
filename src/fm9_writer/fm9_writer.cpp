/*
 * FM9 Writer implementation
 */

#include "fm9_writer.h"
#include <fstream>
#include <cstring>
#include <algorithm>

#ifdef HAVE_MINIZ
#include "../miniz.h"
#elif defined(HAVE_ZLIB)
#include <zlib.h>
#endif

// Helper to get lowercase extension
static std::string getExtensionLower(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// Gzip compression (same as unified_converter.cpp)
static std::vector<uint8_t> gzipCompress(const std::vector<uint8_t>& data) {
#if defined(HAVE_MINIZ) || defined(HAVE_ZLIB)
    if (data.empty()) return {};

    mz_ulong crc = mz_crc32(MZ_CRC32_INIT, data.data(), data.size());

    mz_ulong compressed_bound = mz_compressBound(static_cast<mz_ulong>(data.size()));
    std::vector<uint8_t> deflate_data(compressed_bound);

    mz_stream strm = {};
    strm.next_in = data.data();
    strm.avail_in = static_cast<unsigned int>(data.size());
    strm.next_out = deflate_data.data();
    strm.avail_out = static_cast<unsigned int>(deflate_data.size());

    int ret = mz_deflateInit2(&strm, MZ_DEFAULT_COMPRESSION, MZ_DEFLATED,
                               -MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY);
    if (ret != MZ_OK) return {};

    ret = mz_deflate(&strm, MZ_FINISH);
    mz_ulong deflate_size = strm.total_out;
    mz_deflateEnd(&strm);

    if (ret != MZ_STREAM_END) return {};

    std::vector<uint8_t> gzip_data;
    gzip_data.reserve(10 + deflate_size + 8);

    // Gzip header
    gzip_data.push_back(0x1f);
    gzip_data.push_back(0x8b);
    gzip_data.push_back(0x08);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0xff);

    // Deflate data
    gzip_data.insert(gzip_data.end(), deflate_data.begin(),
                     deflate_data.begin() + deflate_size);

    // Gzip trailer
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

FM9Writer::FM9Writer() = default;
FM9Writer::~FM9Writer() = default;

void FM9Writer::setVGMData(const std::vector<uint8_t>& vgm_data) {
    vgm_data_ = vgm_data;
}

bool FM9Writer::loadFile(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_ = "Failed to open file: " + path;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        error_ = "Failed to read file: " + path;
        return false;
    }

    return true;
}

uint8_t FM9Writer::detectAudioFormat(const std::string& path) {
    std::string ext = getExtensionLower(path);

    if (ext == "wav" || ext == "wave") {
        return FM9_AUDIO_WAV;
    }
    if (ext == "mp3") {
        return FM9_AUDIO_MP3;
    }

    // Try magic bytes
    std::ifstream file(path, std::ios::binary);
    if (!file) return FM9_AUDIO_NONE;

    uint8_t magic[4];
    file.read(reinterpret_cast<char*>(magic), 4);

    // WAV: "RIFF"
    if (magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F') {
        return FM9_AUDIO_WAV;
    }

    // MP3: ID3 tag or frame sync
    if ((magic[0] == 'I' && magic[1] == 'D' && magic[2] == '3') ||
        (magic[0] == 0xFF && (magic[1] & 0xE0) == 0xE0)) {
        return FM9_AUDIO_MP3;
    }

    return FM9_AUDIO_NONE;
}

bool FM9Writer::setAudioFile(const std::string& path) {
    audio_format_ = detectAudioFormat(path);
    if (audio_format_ == FM9_AUDIO_NONE) {
        error_ = "Unsupported audio format (use WAV or MP3): " + path;
        return false;
    }

    if (!loadFile(path, audio_data_)) {
        audio_format_ = FM9_AUDIO_NONE;
        return false;
    }

    return true;
}

bool FM9Writer::setFXFile(const std::string& path) {
    if (!loadFile(path, fx_data_)) {
        return false;
    }

    // Basic JSON validation - check for opening brace
    bool found_brace = false;
    for (uint8_t c : fx_data_) {
        if (c == '{') {
            found_brace = true;
            break;
        }
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
    }

    if (!found_brace) {
        error_ = "FX file does not appear to be valid JSON: " + path;
        fx_data_.clear();
        return false;
    }

    return true;
}

FM9Header FM9Writer::buildHeader() const {
    FM9Header header = {};

    header.magic[0] = 'F';
    header.magic[1] = 'M';
    header.magic[2] = '9';
    header.magic[3] = '0';

    header.version = 1;

    header.flags = 0;
    if (!audio_data_.empty()) header.flags |= FM9_FLAG_HAS_AUDIO;
    if (!fx_data_.empty()) header.flags |= FM9_FLAG_HAS_FX;

    header.audio_format = audio_format_;
    header.reserved = 0;

    // Offsets are from start of FM9 header (within decompressed data)
    // FX data is in the compressed section, right after the header
    // Audio is OUTSIDE the compressed section (appended raw after gzip)
    header.fx_offset = sizeof(FM9Header);
    header.fx_size = static_cast<uint32_t>(fx_data_.size());

    // Audio offset is special: it indicates audio exists but actual data
    // is after the gzip section. We store the size here so Teensy knows
    // how much to copy. The actual file offset = gzip_size (determined at runtime)
    header.audio_offset = 0;  // Not used for seeking (audio is after gzip)
    header.audio_size = static_cast<uint32_t>(audio_data_.size());

    return header;
}

size_t FM9Writer::write(const std::string& output_path) {
    if (vgm_data_.empty()) {
        error_ = "No VGM data set";
        return 0;
    }

    // Build the COMPRESSED portion:
    // [VGM data] + [FM9 Header] + [FX data]
    // Audio is appended UNCOMPRESSED after the gzip data
    std::vector<uint8_t> compressed_portion;
    compressed_portion.reserve(vgm_data_.size() + sizeof(FM9Header) + fx_data_.size());

    // VGM data first
    compressed_portion.insert(compressed_portion.end(), vgm_data_.begin(), vgm_data_.end());

    // FM9 header
    FM9Header header = buildHeader();
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
    compressed_portion.insert(compressed_portion.end(), header_bytes, header_bytes + sizeof(FM9Header));

    // FX data (if any) - goes in compressed section
    if (!fx_data_.empty()) {
        compressed_portion.insert(compressed_portion.end(), fx_data_.begin(), fx_data_.end());
    }

    // Gzip compress the VGM + header + FX
    std::vector<uint8_t> compressed = gzipCompress(compressed_portion);
    if (compressed.empty()) {
        error_ = "Gzip compression failed";
        return 0;
    }

    // Write to file
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        error_ = "Failed to open output file: " + output_path;
        return 0;
    }

    // Write compressed portion (VGM + FM9 header + FX)
    out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());

    // Append raw audio data AFTER the gzip section (uncompressed)
    if (!audio_data_.empty()) {
        out.write(reinterpret_cast<const char*>(audio_data_.data()), audio_data_.size());
    }

    out.close();

    return compressed.size() + audio_data_.size();
}
