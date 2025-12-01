/*
 * FM9 Writer - Extended VGM format with audio and effects
 *
 * FM9 file structure:
 * [Gzipped VGM+GD3] + [FM9 Header] + [Audio Chunk] + [FX Chunk]
 *
 * The FM9 extension appears after the GD3 tag. Standard VGM players
 * will ignore it since they stop at the 0x66 end command.
 */

#ifndef FM9_WRITER_H
#define FM9_WRITER_H

#include <string>
#include <vector>
#include <cstdint>

// FM9 Extension Header (24 bytes)
struct FM9Header {
    char     magic[4];        // "FM90"
    uint8_t  version;         // Format version (1)
    uint8_t  flags;           // Bit flags
    uint8_t  audio_format;    // 0=none, 1=WAV, 2=MP3
    uint8_t  reserved;        // Padding
    uint32_t audio_offset;    // Offset from FM9 header start to audio data
    uint32_t audio_size;      // Size of audio data in bytes
    uint32_t fx_offset;       // Offset from FM9 header start to FX data
    uint32_t fx_size;         // Size of FX JSON in bytes
};

// Flag bits
constexpr uint8_t FM9_FLAG_HAS_AUDIO = 0x01;
constexpr uint8_t FM9_FLAG_HAS_FX    = 0x02;

// Audio format values
constexpr uint8_t FM9_AUDIO_NONE = 0;
constexpr uint8_t FM9_AUDIO_WAV  = 1;
constexpr uint8_t FM9_AUDIO_MP3  = 2;

class FM9Writer {
public:
    FM9Writer();
    ~FM9Writer();

    // Set VGM data (required)
    void setVGMData(const std::vector<uint8_t>& vgm_data);

    // Set optional audio file (WAV or MP3)
    // Returns false if file cannot be loaded or format not recognized
    bool setAudioFile(const std::string& path);

    // Set optional FX file (JSON)
    // Returns false if file cannot be loaded
    bool setFXFile(const std::string& path);

    // Write complete FM9 file (always gzip compressed)
    // Returns bytes written, or 0 on error
    size_t write(const std::string& output_path);

    // Get last error message
    const std::string& getError() const { return error_; }

    // Check if audio/fx were set
    bool hasAudio() const { return !audio_data_.empty(); }
    bool hasFX() const { return !fx_data_.empty(); }

private:
    std::vector<uint8_t> vgm_data_;
    std::vector<uint8_t> audio_data_;
    std::vector<uint8_t> fx_data_;
    uint8_t audio_format_ = FM9_AUDIO_NONE;
    std::string error_;

    // Detect audio format from file extension and magic bytes
    uint8_t detectAudioFormat(const std::string& path);

    // Load file into vector
    bool loadFile(const std::string& path, std::vector<uint8_t>& data);

    // Build the FM9 header
    FM9Header buildHeader() const;
};

#endif // FM9_WRITER_H
