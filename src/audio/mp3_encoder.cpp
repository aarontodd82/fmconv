/*
 * MP3 Encoder implementation using LAME
 */

#include "mp3_encoder.h"

#ifdef HAVE_LAME
#include "lame.h"
#endif

#include <cstring>
#include <fstream>
#include <algorithm>

MP3Encoder::MP3Encoder() = default;

MP3Encoder::~MP3Encoder() {
#ifdef HAVE_LAME
    if (lame_) {
        lame_close(lame_);
        lame_ = nullptr;
    }
#endif
}

bool MP3Encoder::initialize(const MP3EncoderConfig& config) {
#ifdef HAVE_LAME
    // Close any existing encoder
    if (lame_) {
        lame_close(lame_);
        lame_ = nullptr;
    }

    config_ = config;

    // Initialize LAME
    lame_ = lame_init();
    if (!lame_) {
        error_ = "Failed to initialize LAME encoder";
        return false;
    }

    // Configure encoder
    lame_set_num_channels(lame_, config.channels);
    lame_set_in_samplerate(lame_, config.sample_rate);
    lame_set_out_samplerate(lame_, config.sample_rate);  // No resampling
    lame_set_brate(lame_, config.bitrate_kbps);
    lame_set_mode(lame_, config.channels == 1 ? MONO : JOINT_STEREO);
    lame_set_quality(lame_, 2);  // High quality (0=best, 9=worst)

    // VBR off - use constant bitrate for predictable file sizes
    lame_set_VBR(lame_, vbr_off);

    // Initialize parameters
    if (lame_init_params(lame_) < 0) {
        error_ = "Failed to initialize LAME parameters";
        lame_close(lame_);
        lame_ = nullptr;
        return false;
    }

    return true;
#else
    error_ = "MP3 encoding not available (LAME library not linked)";
    return false;
#endif
}

std::vector<uint8_t> MP3Encoder::encode(const int16_t* pcm, size_t sample_count) {
    std::vector<uint8_t> result;

#ifdef HAVE_LAME
    if (!lame_) {
        error_ = "Encoder not initialized";
        return result;
    }

    if (sample_count == 0 || !pcm) {
        return result;
    }

    // Allocate output buffer
    // LAME recommends 1.25 * num_samples + 7200
    size_t buffer_size = static_cast<size_t>(1.25 * sample_count) + 7200;
    result.resize(buffer_size);

    int bytes_encoded;
    if (config_.channels == 2) {
        // Interleaved stereo input
        bytes_encoded = lame_encode_buffer_interleaved(
            lame_,
            const_cast<short*>(pcm),
            static_cast<int>(sample_count),
            result.data(),
            static_cast<int>(result.size())
        );
    } else {
        // Mono input
        bytes_encoded = lame_encode_buffer(
            lame_,
            pcm,
            nullptr,  // Right channel (unused for mono)
            static_cast<int>(sample_count),
            result.data(),
            static_cast<int>(result.size())
        );
    }

    if (bytes_encoded < 0) {
        error_ = "LAME encoding failed with error code: " + std::to_string(bytes_encoded);
        result.clear();
        return result;
    }

    result.resize(static_cast<size_t>(bytes_encoded));
#else
    (void)pcm;
    (void)sample_count;
    error_ = "MP3 encoding not available (LAME library not linked)";
#endif

    return result;
}

std::vector<uint8_t> MP3Encoder::finish() {
    std::vector<uint8_t> result;

#ifdef HAVE_LAME
    if (!lame_) {
        return result;
    }

    // Flush encoder - get any remaining data
    // LAME recommends 7200 bytes for flush buffer
    result.resize(7200);

    int bytes_encoded = lame_encode_flush(
        lame_,
        result.data(),
        static_cast<int>(result.size())
    );

    if (bytes_encoded < 0) {
        error_ = "LAME flush failed with error code: " + std::to_string(bytes_encoded);
        result.clear();
        return result;
    }

    result.resize(static_cast<size_t>(bytes_encoded));

    // Close encoder
    lame_close(lame_);
    lame_ = nullptr;
#endif

    return result;
}

size_t MP3Encoder::estimateOutputSize(size_t sample_count, int bitrate_kbps, int sample_rate) {
    // MP3 output size estimate: (bitrate_kbps * 1000 / 8) * (sample_count / sample_rate)
    // Add 10% margin for headers and padding
    double duration_sec = static_cast<double>(sample_count) / sample_rate;
    double bytes = (bitrate_kbps * 1000.0 / 8.0) * duration_sec;
    return static_cast<size_t>(bytes * 1.1) + 7200;  // Add LAME buffer requirement
}

// Convenience function implementations

std::vector<uint8_t> encodePCMtoMP3(const int16_t* pcm, size_t sample_count,
                                     const MP3EncoderConfig& config,
                                     std::string* error) {
    std::vector<uint8_t> result;

    MP3Encoder encoder;
    if (!encoder.initialize(config)) {
        if (error) *error = encoder.getError();
        return result;
    }

    // Encode in chunks to avoid huge temporary buffers
    const size_t chunk_size = 44100;  // 1 second at a time
    size_t offset = 0;

    result.reserve(MP3Encoder::estimateOutputSize(sample_count, config.bitrate_kbps, config.sample_rate));

    while (offset < sample_count) {
        size_t this_chunk = std::min(chunk_size, sample_count - offset);
        size_t pcm_offset = offset * config.channels;

        auto chunk_data = encoder.encode(pcm + pcm_offset, this_chunk);
        result.insert(result.end(), chunk_data.begin(), chunk_data.end());

        offset += this_chunk;
    }

    // Flush remaining data
    auto flush_data = encoder.finish();
    result.insert(result.end(), flush_data.begin(), flush_data.end());

    return result;
}

// Helper to parse WAV header
struct WAVHeader {
    int sample_rate = 0;
    int channels = 0;
    int bits_per_sample = 0;
    int audio_format = 0;  // 1 = PCM, 3 = IEEE float
    size_t data_offset = 0;
    size_t data_size = 0;
    bool valid = false;
};

static WAVHeader parseWAVHeader(const uint8_t* data, size_t size) {
    WAVHeader header;

    if (size < 44) return header;

    // Check RIFF header
    if (memcmp(data, "RIFF", 4) != 0) return header;
    if (memcmp(data + 8, "WAVE", 4) != 0) return header;

    // Parse chunks
    size_t pos = 12;
    while (pos + 8 <= size) {
        char chunk_id[5] = {0};
        memcpy(chunk_id, data + pos, 4);
        uint32_t chunk_size = data[pos + 4] | (data[pos + 5] << 8) |
                              (data[pos + 6] << 16) | (data[pos + 7] << 24);

        if (strcmp(chunk_id, "fmt ") == 0 && pos + 8 + 16 <= size) {
            header.audio_format = data[pos + 8] | (data[pos + 9] << 8);
            // Accept PCM (1) and IEEE float (3)
            if (header.audio_format != 1 && header.audio_format != 3) return header;

            header.channels = data[pos + 10] | (data[pos + 11] << 8);
            header.sample_rate = data[pos + 12] | (data[pos + 13] << 8) |
                                 (data[pos + 14] << 16) | (data[pos + 15] << 24);
            header.bits_per_sample = data[pos + 22] | (data[pos + 23] << 8);
        }
        else if (strcmp(chunk_id, "data") == 0) {
            header.data_offset = pos + 8;
            header.data_size = chunk_size;
            break;
        }

        pos += 8 + chunk_size;
        // Align to 2-byte boundary
        if (chunk_size % 2) pos++;
    }

    // Validate supported formats
    if (header.sample_rate > 0 && header.channels > 0 &&
        header.channels <= 2 && header.data_size > 0) {
        // PCM: 8, 16, 24, 32-bit
        if (header.audio_format == 1 &&
            (header.bits_per_sample == 8 || header.bits_per_sample == 16 ||
             header.bits_per_sample == 24 || header.bits_per_sample == 32)) {
            header.valid = true;
        }
        // IEEE float: 32-bit
        else if (header.audio_format == 3 && header.bits_per_sample == 32) {
            header.valid = true;
        }
    }

    return header;
}

// Linear interpolation resampler
static void resampleLinear(const int16_t* src, size_t src_samples, int src_channels,
                           std::vector<int16_t>& dst, int src_rate, int dst_rate) {
    // Calculate output sample count
    size_t dst_samples = static_cast<size_t>(
        static_cast<double>(src_samples) * dst_rate / src_rate + 0.5);

    dst.resize(dst_samples * 2);  // Always output stereo

    double ratio = static_cast<double>(src_rate) / dst_rate;

    for (size_t i = 0; i < dst_samples; i++) {
        double src_pos = i * ratio;
        size_t src_idx = static_cast<size_t>(src_pos);
        double frac = src_pos - src_idx;

        // Clamp to valid range
        if (src_idx >= src_samples - 1) {
            src_idx = src_samples - 1;
            frac = 0;
        }

        for (int ch = 0; ch < 2; ch++) {
            int src_ch = (src_channels == 1) ? 0 : ch;  // Mono to stereo

            int16_t s0 = src[src_idx * src_channels + src_ch];
            int16_t s1 = (src_idx + 1 < src_samples)
                ? src[(src_idx + 1) * src_channels + src_ch]
                : s0;

            // Linear interpolation
            int32_t sample = static_cast<int32_t>(s0 * (1.0 - frac) + s1 * frac);
            dst[i * 2 + ch] = static_cast<int16_t>(std::max(-32768, std::min(32767, sample)));
        }
    }
}

// Convert any WAV format to 16-bit 44.1kHz stereo PCM
static bool convertToStandardPCM(const uint8_t* wav_data, size_t wav_size,
                                  const WAVHeader& header,
                                  std::vector<int16_t>& pcm_out,
                                  std::string* error) {
    const uint8_t* src_data = wav_data + header.data_offset;
    size_t bytes_per_sample = header.bits_per_sample / 8;
    size_t src_samples = header.data_size / (bytes_per_sample * header.channels);

    // First convert to 16-bit (keep original sample rate and channels)
    std::vector<int16_t> pcm_native;
    pcm_native.reserve(src_samples * header.channels);

    for (size_t i = 0; i < src_samples * header.channels; i++) {
        const uint8_t* sample_ptr = src_data + i * bytes_per_sample;
        int32_t sample = 0;

        if (header.audio_format == 3) {
            // IEEE float 32-bit
            float f;
            memcpy(&f, sample_ptr, 4);
            sample = static_cast<int32_t>(f * 32767.0f);
        }
        else if (header.bits_per_sample == 8) {
            // 8-bit unsigned
            sample = (static_cast<int32_t>(sample_ptr[0]) - 128) * 256;
        }
        else if (header.bits_per_sample == 16) {
            // 16-bit signed little-endian
            sample = static_cast<int16_t>(sample_ptr[0] | (sample_ptr[1] << 8));
        }
        else if (header.bits_per_sample == 24) {
            // 24-bit signed little-endian
            sample = sample_ptr[0] | (sample_ptr[1] << 8) | (sample_ptr[2] << 16);
            if (sample & 0x800000) sample |= 0xFF000000;  // Sign extend
            sample >>= 8;  // Convert to 16-bit range
        }
        else if (header.bits_per_sample == 32) {
            // 32-bit signed little-endian
            sample = sample_ptr[0] | (sample_ptr[1] << 8) |
                     (sample_ptr[2] << 16) | (sample_ptr[3] << 24);
            sample >>= 16;  // Convert to 16-bit range
        }

        // Clamp to 16-bit range
        sample = std::max(-32768, std::min(32767, sample));
        pcm_native.push_back(static_cast<int16_t>(sample));
    }

    // Resample to 44100Hz and convert mono to stereo if needed
    if (header.sample_rate == 44100 && header.channels == 2) {
        // Already in target format
        pcm_out = std::move(pcm_native);
    }
    else {
        // Need to resample and/or convert channels
        resampleLinear(pcm_native.data(), src_samples, header.channels,
                       pcm_out, header.sample_rate, 44100);
    }

    return true;
}

std::vector<uint8_t> encodeWAVtoMP3(const std::string& wav_path,
                                     int bitrate_kbps,
                                     std::string* error) {
    std::vector<uint8_t> result;

    // Read WAV file
    std::ifstream file(wav_path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error) *error = "Failed to open WAV file: " + wav_path;
        return result;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> wav_data(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(wav_data.data()), file_size)) {
        if (error) *error = "Failed to read WAV file";
        return result;
    }

    return encodeWAVDataToMP3(wav_data, bitrate_kbps, error);
}

std::vector<uint8_t> encodeWAVDataToMP3(const std::vector<uint8_t>& wav_data,
                                         int bitrate_kbps,
                                         std::string* error) {
    std::vector<uint8_t> result;

    // Parse WAV header
    WAVHeader header = parseWAVHeader(wav_data.data(), wav_data.size());
    if (!header.valid) {
        if (error) *error = "Unsupported WAV format (need PCM 8/16/24/32-bit or 32-bit float, mono/stereo)";
        return result;
    }

    // Get PCM data pointer
    if (header.data_offset + header.data_size > wav_data.size()) {
        if (error) *error = "WAV file truncated";
        return result;
    }

    // Convert to standard 16-bit 44.1kHz stereo
    std::vector<int16_t> pcm_standard;
    if (!convertToStandardPCM(wav_data.data(), wav_data.size(), header, pcm_standard, error)) {
        return result;
    }

    // Configure encoder for standard format
    MP3EncoderConfig config;
    config.sample_rate = 44100;
    config.channels = 2;
    config.bitrate_kbps = bitrate_kbps;

    size_t sample_count = pcm_standard.size() / 2;  // Stereo pairs
    return encodePCMtoMP3(pcm_standard.data(), sample_count, config, error);
}

// Helper to build standard WAV from PCM data
static std::vector<uint8_t> buildStandardWAV(const std::vector<int16_t>& pcm) {
    size_t data_size = pcm.size() * sizeof(int16_t);
    std::vector<uint8_t> wav(44 + data_size);
    uint8_t* p = wav.data();

    // RIFF header
    memcpy(p, "RIFF", 4); p += 4;
    uint32_t file_size = static_cast<uint32_t>(36 + data_size);
    memcpy(p, &file_size, 4); p += 4;
    memcpy(p, "WAVE", 4); p += 4;

    // fmt chunk
    memcpy(p, "fmt ", 4); p += 4;
    uint32_t fmt_size = 16;
    memcpy(p, &fmt_size, 4); p += 4;
    uint16_t audio_format = 1;  // PCM
    memcpy(p, &audio_format, 2); p += 2;
    uint16_t num_channels = 2;  // Stereo
    memcpy(p, &num_channels, 2); p += 2;
    uint32_t sample_rate = 44100;
    memcpy(p, &sample_rate, 4); p += 4;
    uint32_t byte_rate = 44100 * 2 * sizeof(int16_t);
    memcpy(p, &byte_rate, 4); p += 4;
    uint16_t block_align = 2 * sizeof(int16_t);
    memcpy(p, &block_align, 2); p += 2;
    uint16_t bits_per_sample = 16;
    memcpy(p, &bits_per_sample, 2); p += 2;

    // data chunk
    memcpy(p, "data", 4); p += 4;
    uint32_t data_chunk_size = static_cast<uint32_t>(data_size);
    memcpy(p, &data_chunk_size, 4); p += 4;

    // PCM data
    memcpy(p, pcm.data(), data_size);

    return wav;
}

std::vector<uint8_t> normalizeWAVFile(const std::string& wav_path,
                                       std::string* error) {
    std::vector<uint8_t> result;

    // Read WAV file
    std::ifstream file(wav_path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error) *error = "Failed to open WAV file: " + wav_path;
        return result;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> wav_data(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(wav_data.data()), file_size)) {
        if (error) *error = "Failed to read WAV file";
        return result;
    }

    return normalizeWAVData(wav_data, error);
}

std::vector<uint8_t> normalizeWAVData(const std::vector<uint8_t>& wav_data,
                                       std::string* error) {
    std::vector<uint8_t> result;

    // Parse WAV header
    WAVHeader header = parseWAVHeader(wav_data.data(), wav_data.size());
    if (!header.valid) {
        if (error) *error = "Unsupported WAV format (need PCM 8/16/24/32-bit or 32-bit float, mono/stereo)";
        return result;
    }

    // Check if already in standard format
    if (header.sample_rate == 44100 && header.channels == 2 &&
        header.bits_per_sample == 16 && header.audio_format == 1) {
        // Already standard - return as-is
        return wav_data;
    }

    // Get PCM data pointer
    if (header.data_offset + header.data_size > wav_data.size()) {
        if (error) *error = "WAV file truncated";
        return result;
    }

    // Convert to standard 16-bit 44.1kHz stereo
    std::vector<int16_t> pcm_standard;
    if (!convertToStandardPCM(wav_data.data(), wav_data.size(), header, pcm_standard, error)) {
        return result;
    }

    // Build standard WAV file
    return buildStandardWAV(pcm_standard);
}
