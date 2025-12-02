/*
 * MP3 Encoder - Wrapper for LAME MP3 encoding
 *
 * Encodes PCM audio to MP3 format for embedding in FM9 files.
 * Uses libmp3lame for encoding.
 */

#ifndef MP3_ENCODER_H
#define MP3_ENCODER_H

#include <vector>
#include <cstdint>
#include <string>

// Forward declaration to avoid including lame.h in header
struct lame_global_struct;
typedef struct lame_global_struct* lame_t;

struct MP3EncoderConfig {
    int sample_rate = 44100;
    int channels = 2;
    int bitrate_kbps = 128;  // 96, 128, 160, 192, 256, 320
};

class MP3Encoder {
public:
    MP3Encoder();
    ~MP3Encoder();

    // Initialize encoder with given config
    // Returns false on error (check getError())
    bool initialize(const MP3EncoderConfig& config);

    // Encode PCM samples (interleaved stereo, 16-bit signed)
    // Returns encoded MP3 data (may be empty if buffering)
    std::vector<uint8_t> encode(const int16_t* pcm, size_t sample_count);

    // Flush encoder and return final MP3 data
    // Must be called after all encode() calls to get remaining data
    std::vector<uint8_t> finish();

    // Check if encoder is initialized
    bool isInitialized() const { return lame_ != nullptr; }

    // Get last error message
    const std::string& getError() const { return error_; }

    // Get estimated output size for given input samples
    // Useful for pre-allocating buffers
    static size_t estimateOutputSize(size_t sample_count, int bitrate_kbps, int sample_rate);

    // Prevent copying
    MP3Encoder(const MP3Encoder&) = delete;
    MP3Encoder& operator=(const MP3Encoder&) = delete;

private:
    lame_t lame_ = nullptr;
    std::string error_;
    MP3EncoderConfig config_;
};

// Convenience function: encode entire PCM buffer to MP3 in one call
// Returns empty vector on error
std::vector<uint8_t> encodePCMtoMP3(const int16_t* pcm, size_t sample_count,
                                     const MP3EncoderConfig& config,
                                     std::string* error = nullptr);

// Convenience function: load WAV file and encode to MP3
// Returns empty vector on error
std::vector<uint8_t> encodeWAVtoMP3(const std::string& wav_path,
                                     int bitrate_kbps,
                                     std::string* error = nullptr);

// Convenience function: encode WAV data buffer to MP3
// Returns empty vector on error
std::vector<uint8_t> encodeWAVDataToMP3(const std::vector<uint8_t>& wav_data,
                                         int bitrate_kbps,
                                         std::string* error = nullptr);

// Convert any WAV file to standard 16-bit 44.1kHz stereo WAV
// Supports 8/16/24/32-bit PCM and 32-bit float input
// Returns empty vector on error
std::vector<uint8_t> normalizeWAVFile(const std::string& wav_path,
                                       std::string* error = nullptr);

// Convert any WAV data to standard 16-bit 44.1kHz stereo WAV
// Returns empty vector on error
std::vector<uint8_t> normalizeWAVData(const std::vector<uint8_t>& wav_data,
                                       std::string* error = nullptr);

#endif // MP3_ENCODER_H
