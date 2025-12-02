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
#include "source_format.h"

// FM9 Extension Header (24 bytes)
struct FM9Header {
    char     magic[4];        // "FM90"
    uint8_t  version;         // Format version (1)
    uint8_t  flags;           // Bit flags
    uint8_t  audio_format;    // 0=none, 1=WAV, 2=MP3
    uint8_t  source_format;   // Original file format (see source_format.h)
    uint32_t audio_offset;    // Offset from FM9 header start to audio data
    uint32_t audio_size;      // Size of audio data in bytes
    uint32_t fx_offset;       // Offset from FM9 header start to FX data
    uint32_t fx_size;         // Size of FX JSON in bytes
};

// Flag bits
constexpr uint8_t FM9_FLAG_HAS_AUDIO = 0x01;
constexpr uint8_t FM9_FLAG_HAS_FX    = 0x02;
constexpr uint8_t FM9_FLAG_HAS_IMAGE = 0x04;

// Cover image constants
constexpr uint32_t FM9_IMAGE_WIDTH  = 100;
constexpr uint32_t FM9_IMAGE_HEIGHT = 100;
constexpr uint32_t FM9_IMAGE_SIZE   = FM9_IMAGE_WIDTH * FM9_IMAGE_HEIGHT * 2;  // 20000 bytes (RGB565)

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

    // Set source format (original file type before conversion)
    void setSourceFormat(SourceFormat fmt) { source_format_ = fmt; }
    void setSourceFormat(const std::string& extension);
    SourceFormat getSourceFormat() const { return source_format_; }

    // Set optional audio file (WAV or MP3)
    // Returns false if file cannot be loaded or format not recognized
    bool setAudioFile(const std::string& path);

    // Set audio data directly (for programmatically generated audio)
    // Format: FM9_AUDIO_WAV or FM9_AUDIO_MP3
    void setAudioData(const std::vector<uint8_t>& data, uint8_t format);

    // Set optional FX file (JSON)
    // Returns false if file cannot be loaded
    bool setFXFile(const std::string& path);

    // Set optional cover image (PNG, JPEG, or GIF)
    // Image will be scaled to 100x100 and converted to RGB565
    // Returns false if file cannot be loaded or processed
    bool setImageFile(const std::string& path, bool dither = true);

    // Write complete FM9 file (always gzip compressed)
    // Returns bytes written, or 0 on error
    size_t write(const std::string& output_path);

    // Get last error message
    const std::string& getError() const { return error_; }

    // Check if audio/fx/image were set
    bool hasAudio() const { return !audio_data_.empty(); }
    bool hasFX() const { return !fx_data_.empty(); }
    bool hasImage() const { return !image_data_.empty(); }

private:
    std::vector<uint8_t> vgm_data_;
    std::vector<uint8_t> audio_data_;
    std::vector<uint8_t> fx_data_;
    std::vector<uint8_t> image_data_;  // 100x100 RGB565 (20000 bytes when set)
    uint8_t audio_format_ = FM9_AUDIO_NONE;
    SourceFormat source_format_ = SRC_UNKNOWN;
    std::string error_;

    // Detect audio format from file extension and magic bytes
    uint8_t detectAudioFormat(const std::string& path);

    // Load file into vector
    bool loadFile(const std::string& path, std::vector<uint8_t>& data);

    // Build the FM9 header
    FM9Header buildHeader() const;

    // Image processing helpers
    bool processImage(const uint8_t* pixels, int width, int height, bool dither);
    void scaleImage(const uint8_t* src, int src_w, int src_h,
                    uint8_t* dst, int dst_w, int dst_h, int dst_x, int dst_y);
    void applyDithering(uint8_t* pixels, int width, int height);
    uint16_t rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b);
};

#endif // FM9_WRITER_H
