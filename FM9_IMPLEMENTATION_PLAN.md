# FM9 Format Implementation Plan

## Overview

FM9 is an extended VGM format designed for the Teensy OPL3 player. It bundles standard VGM data with optional embedded audio (WAV/MP3) and real-time effects automation.

**Goals:**
- Default output format for fmconv
- Backwards compatible structure (VGM data remains parseable)
- Minimal changes to existing Teensy VGM player
- Optional audio and FX - FM9 without extras is just a renamed VGZ
- **Streamable audio** - Audio data is NOT compressed, allowing Teensy to stream-copy directly to temp file without needing RAM for decompression

---

## File Structure

```
┌─────────────────────────────────────┐
│         GZIP COMPRESSED             │
│  ┌───────────────────────────────┐  │
│  │  Standard VGM Data            │  │
│  │  (header + commands + GD3)    │  │
│  ├───────────────────────────────┤  │
│  │  FM9 Extension Header         │  │
│  ├───────────────────────────────┤  │
│  │  FX Chunk (optional)          │  │
│  │  (JSON effects timeline)      │  │
│  └───────────────────────────────┘  │
├─────────────────────────────────────┤
│       UNCOMPRESSED (raw)            │
│  ┌───────────────────────────────┐  │
│  │  Audio Chunk (optional)       │  │
│  │  (WAV or MP3 data)            │  │
│  ├───────────────────────────────┤  │
│  │  Cover Image (optional)       │  │
│  │  (100x100 RGB565, 20KB)       │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

**Important:** The audio and cover image chunks are stored AFTER the gzip-compressed section, uncompressed. This allows the Teensy to:
1. Stream-decompress the VGM data (same as existing VGZ playback - no full RAM load needed)
2. Stream-copy the audio directly from SD to a temp file without decompression
3. Read the cover image directly and display it without decompression (raw RGB565)

The VGM portion is played using the existing streaming gzip decompression - the FM9 header and FX data are encountered during streaming and parsed as they flow through.

The FM9 extension data appears after the GD3 tag (or after VGM data if no GD3). Standard VGM players will ignore it since they stop at the `0x66` end command.

---

## FM9 Extension Header

Located immediately after GD3 tag (or VGM data end if no GD3).

```c
struct FM9Header {
    char     magic[4];        // "FM90"
    uint8_t  version;         // Format version (1)
    uint8_t  flags;           // Bit flags (see below)
    uint8_t  audio_format;    // 0=none, 1=WAV, 2=MP3
    uint8_t  reserved;        // Padding
    uint32_t audio_offset;    // Offset from FM9 header start to audio data
    uint32_t audio_size;      // Size of audio data in bytes
    uint32_t fx_offset;       // Offset from FM9 header start to FX data
    uint32_t fx_size;         // Size of FX JSON in bytes
};
// Total: 24 bytes
```

**Flags (bit field):**
- Bit 0: Has audio chunk
- Bit 1: Has FX chunk
- Bit 2: Has cover image (always 20000 bytes when present)
- Bits 3-7: Reserved

**Audio Format:**
- 0 = No audio
- 1 = WAV (PCM, any sample rate/bit depth Teensy supports)
- 2 = MP3

---

## FX Chunk Format (JSON)

The FX chunk contains a JSON object with an array of timed effect events. Effects are applied to the master output (affects both VGM/OPL audio and embedded audio).

```json
{
  "version": 1,
  "events": [
    {
      "time_ms": 0,
      "effects": {
        "reverb": {
          "enabled": true,
          "room_size": 0.6,
          "damping": 0.55,
          "wet_mix": 0.25
        }
      }
    },
    {
      "time_ms": 30000,
      "effects": {
        "reverb": {
          "wet_mix": 0.4
        },
        "delay": {
          "enabled": true,
          "time_ms": 250,
          "feedback": 0.3,
          "wet_mix": 0.2
        }
      }
    },
    {
      "time_ms": 60000,
      "effects": {
        "chorus": {
          "enabled": true,
          "rate_hz": 1.5,
          "depth": 0.3,
          "wet_mix": 0.25
        }
      }
    }
  ]
}
```

**Supported Effects (initial set):**

| Effect | Parameters | Range | Notes |
|--------|-----------|-------|-------|
| reverb | enabled, room_size, damping, wet_mix | 0.0-1.0 | Freeverb |
| delay | enabled, time_ms, feedback, wet_mix | time: 0-1000ms | Simple delay |
| chorus | enabled, rate_hz, depth, wet_mix | rate: 0.1-5.0 | Modulated delay |
| eq_low | gain_db | -12 to +12 | Low shelf |
| eq_mid | gain_db, freq_hz | -12 to +12 | Parametric |
| eq_high | gain_db | -12 to +12 | High shelf |
| master_volume | level | 0.0-1.0 | Overall volume |

**Event behavior:**
- Events are cumulative - only specified parameters change
- `enabled: false` bypasses the effect
- Time 0 sets initial state
- Effects interpolate smoothly when possible (volume, wet_mix)

---

## Cover Image Format

The cover image is an optional 100x100 pixel image stored as raw RGB565 data after the audio chunk (if present) or directly after the gzip-compressed section.

### Specifications

- **Dimensions:** Fixed 100x100 pixels
- **Format:** RGB565 (16-bit: 5 bits red, 6 bits green, 5 bits blue)
- **Size:** Exactly 20,000 bytes (100 × 100 × 2)
- **Byte order:** Little-endian (matches Teensy display)
- **Location:** After audio chunk (uncompressed section)

### Input Image Processing (fmconv)

Accepts PNG, JPEG, or GIF input images with the following processing:

1. **Size validation:** Source image must be ≤4096x4096 pixels and ≤10MB file size
2. **Aspect ratio preservation:** Scale to fit within 100x100 while maintaining aspect ratio
3. **Letterboxing/pillarboxing:** Center scaled image on black (0x0000) background
4. **Dithering (default):** Apply selective 16-color palette + ordered Bayer dithering for 90s DOS aesthetic
5. **Color conversion:** Convert to RGB565 for output (Teensy-friendly)

**Dithering modes:**
- **Default:** Selective 16-color palette (median cut) + 4x4 Bayer ordered dithering. Gives that classic DOS game box art look.
- **`--no-dither`:** Clean scaling and RGB565 conversion only. Modern/clean look.

**Scaling algorithm:**
```cpp
// Calculate scale factor to fit within 100x100
float scale = std::min(100.0f / width, 100.0f / height);
int scaled_width = (int)(width * scale);
int scaled_height = (int)(height * scale);

// Center on 100x100 black canvas
int offset_x = (100 - scaled_width) / 2;
int offset_y = (100 - scaled_height) / 2;
```

**Bayer 4x4 ordered dithering:**
```cpp
const int bayer4x4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

// For each pixel, add threshold before quantizing to 16-color palette
int threshold = (bayer4x4[y % 4][x % 4] - 8) * 16;  // -128 to +112 range
r = clamp(r + threshold, 0, 255);
g = clamp(g + threshold, 0, 255);
b = clamp(b + threshold, 0, 255);
// Then find nearest color in 16-color palette
```

**16-color palette selection (median cut):**
```cpp
// 1. Collect all unique colors from scaled image
// 2. Use median cut algorithm to find optimal 16 colors
// 3. For each pixel, find nearest palette color (after dither threshold applied)
// 4. Convert final palette color to RGB565 for output
```

**RGB565 conversion:**
```cpp
uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
```

### Teensy Display

- Image is read directly from FM9 file offset (no decompression needed)
- Displayed on the "Now Playing" screen before playback begins
- Uses TFT display's native RGB565 format for direct buffer copy

---

## Converter Implementation (fmconv)

### New Command Line Options

```
fmconv <input> [options] -o <output>

Output Format:
  --format fm9|vgz|vgm    Output format (default: fm9)

FM9 Options:
  --audio <file>          Embed audio file (WAV or MP3)
  --fx <file>             Embed effects JSON file
  --image <file>          Embed cover image (PNG, JPEG, or GIF, max 4096x4096)
  --no-dither             Disable 16-color dithering on cover image (clean output)

Existing options remain unchanged.
```

### Implementation Tasks

#### 1. Add FM9 writer module
**File:** `src/fm9_writer/fm9_writer.h`, `src/fm9_writer/fm9_writer.cpp`

```cpp
class FM9Writer {
public:
    // Set components before writing
    void setVGMData(const std::vector<uint8_t>& vgm_data);
    void setAudioFile(const std::string& path);  // WAV or MP3
    void setFXFile(const std::string& path);      // JSON
    void setImageFile(const std::string& path);   // PNG, JPEG, or GIF

    // Write complete FM9 (gzipped)
    bool write(const std::string& output_path);

private:
    std::vector<uint8_t> vgm_data_;
    std::vector<uint8_t> audio_data_;
    std::vector<uint8_t> fx_data_;
    std::vector<uint8_t> image_data_;  // 100x100 RGB565 (20000 bytes)
    uint8_t audio_format_ = 0;

    uint8_t detectAudioFormat(const std::string& path);
    bool loadAudioFile(const std::string& path);
    bool loadFXFile(const std::string& path);
    bool loadImageFile(const std::string& path);  // Loads, scales, converts to RGB565
    void buildFM9Header(FM9Header& header);
};
```

#### 2. Modify unified_converter.cpp

- Add `--format`, `--audio`, `--fx`, `--image` argument parsing
- Default output extension to `.fm9`
- After VGM generation, pass to FM9Writer if format is fm9
- If format is vgm/vgz, use existing output path

**Pseudocode:**
```cpp
// After VGM data is generated...
if (output_format == "fm9") {
    FM9Writer writer;
    writer.setVGMData(vgm_buffer);
    if (!audio_path.empty()) writer.setAudioFile(audio_path);
    if (!fx_path.empty()) writer.setFXFile(fx_path);
    if (!image_path.empty()) writer.setImageFile(image_path);
    writer.write(output_path);
} else {
    // Existing VGM/VGZ output logic
}
```

#### 3. Update file extension handling

- `.fm9` always gzipped (no uncompressed option)
- `--format vgm` produces uncompressed `.vgm`
- `--format vgz` produces compressed `.vgz`
- `--format fm9` produces compressed `.fm9` (default)

#### 4. Validation

- Validate audio file exists and is WAV or MP3
- Validate FX JSON syntax before embedding
- Warn if audio file is very large (>10MB?)
- Validate image file exists and is PNG, JPEG, or GIF
- Reject images larger than 4096x4096 pixels or 10MB file size
- Warn if image fails to load (invalid format, corrupt file)

#### 5. Image Processing Dependencies

**New dependency:** `stb_image.h` (single-header library)
- Download from: https://github.com/nothings/stb
- Place in `src/` directory
- Handles PNG, JPEG, GIF decoding

**Image processing implementation:**
```cpp
bool FM9Writer::loadImageFile(const std::string& path) {
    // Check file size (reject >10MB)
    // Load with stbi_load()
    // Check dimensions (reject >4096x4096)
    // Calculate scale to fit 100x100 preserving aspect ratio
    // Allocate 100x100 RGB565 buffer (20000 bytes)
    // Fill with black (0x0000)
    // Scale and center image using nearest-neighbor or bilinear
    // Convert each pixel: RGB888 -> RGB565
    // Store in image_data_
    return true;
}
```

---

## Teensy Player Implementation

### Overview

The FM9Player wraps the existing VGMPlayer. When loading a file:

1. Check extension (`.fm9` vs `.vgm`/`.vgz`)
2. For FM9: Stream-decompress to find FM9 header, parse FX events, locate audio/image offsets
3. If cover image present, read and display it on "Now Playing" screen (must complete before playback starts)
4. Load WAV directly from FM9 file offset (no temp file extraction)
5. Play VGM portion using VGMPlayer (FM9 files are valid VGZ - VGMPlayer ignores FM9 extensions)
6. Simultaneously stream audio with bidirectional sync and apply FX events

### New Files

```
src/
├── fm9_player.h
├── fm9_player.cpp
├── fm9_file.h
├── fm9_file.cpp
├── fx_engine.h
├── fx_engine.cpp
├── audio_stream_fm9_wav.h      # Custom AudioStream for synced WAV playback
└── audio_stream_fm9_wav.cpp
```

### Implementation Tasks

#### 1. FM9 File Handling

**File:** `src/fm9_file.h`, `src/fm9_file.cpp`

FM9 files use the same streaming gzip decompression as VGZ files - no full file load to RAM.

**Key insight:** The FM9 header and FX data appear AFTER the VGM `0x66` end command in the decompressed stream. During `loadFromFile()`:
1. Stream-decompress the entire gzip section using uzlib with a sliding window scanner
2. Detect "FM90" magic after VGM data ends (using 4-byte slide window)
3. Parse FM9Header (28 bytes) and FX JSON (buffered to RAM - small)
4. Track where gzip section ends in the file (`gzipEndOffset_`) - audio starts here
5. **No temp file extraction** - audio and image are streamed directly from the FM9 file
6. Image offset = gzipEndOffset_ + audio_size (image follows audio)

```cpp
struct FM9Header {
    char magic[4];            // "FM90"
    uint8_t version;          // Format version (1)
    uint8_t flags;            // Bit flags (FM9_FLAG_HAS_AUDIO, FM9_FLAG_HAS_FX, FM9_FLAG_HAS_IMAGE)
    uint8_t audio_format;     // 0=none, 1=WAV, 2=MP3
    uint8_t reserved;
    uint32_t audio_offset;    // Not used (audio is after gzip)
    uint32_t audio_size;      // Size of audio data in bytes
    uint32_t fx_offset;       // Offset from FM9 header to FX data
    uint32_t fx_size;         // Size of FX JSON in bytes
};
// Total: 24 bytes

// Flag constants
constexpr uint8_t FM9_FLAG_HAS_AUDIO = 0x01;
constexpr uint8_t FM9_FLAG_HAS_FX    = 0x02;
constexpr uint8_t FM9_FLAG_HAS_IMAGE = 0x04;

// Audio format constants
constexpr uint8_t FM9_AUDIO_NONE = 0;
constexpr uint8_t FM9_AUDIO_WAV  = 1;
constexpr uint8_t FM9_AUDIO_MP3  = 2;

// Cover image constants
constexpr uint32_t FM9_IMAGE_WIDTH  = 100;
constexpr uint32_t FM9_IMAGE_HEIGHT = 100;
constexpr uint32_t FM9_IMAGE_SIZE   = 100 * 100 * 2;  // 20000 bytes (RGB565)

class FM9File {
public:
    FM9File();
    ~FM9File();

    // Load FM9 file - stream decompresses to find FM9 header
    bool loadFromFile(const char* filename, FileSource* fileSource);
    void clear();

    // FM9 extension info
    bool hasFM9Extension() const { return hasFM9Header_; }
    bool hasAudio() const { return hasFM9Header_ && (fm9Header_.flags & FM9_FLAG_HAS_AUDIO); }
    bool hasFX() const { return hasFM9Header_ && (fm9Header_.flags & FM9_FLAG_HAS_FX); }
    bool hasImage() const { return hasFM9Header_ && (fm9Header_.flags & FM9_FLAG_HAS_IMAGE); }
    uint8_t getAudioFormat() const { return fm9Header_.audio_format; }
    uint32_t getAudioSize() const { return fm9Header_.audio_size; }
    uint32_t getAudioOffset() const { return gzipEndOffset_; }  // Where audio starts in file
    uint32_t getImageOffset() const { return gzipEndOffset_ + fm9Header_.audio_size; }  // After audio
    // Image size is always FM9_IMAGE_SIZE (20000 bytes) when hasImage() is true

    // FX data (null-terminated)
    const char* getFXJson() const;
    size_t getFXJsonSize() const { return fxJsonSize_; }

    // Original path (for VGMPlayer to load VGM portion)
    const char* getOriginalPath() const { return originalPath_; }

private:
    FM9Header fm9Header_;
    bool hasFM9Header_;
    char* fxJsonData_;         // Heap allocated, small
    size_t fxJsonSize_;
    char originalPath_[128];
    uint32_t gzipEndOffset_;   // Where gzip ends = where audio starts
    FileSource* fileSource_;

    bool streamDecompressAndParse(File& file);  // Uses uzlib streaming
};
```

**Streaming Decompression Pattern:**
```cpp
// Same callback pattern as VGMFile
static int fm9StreamingReadCallback(struct uzlib_uncomp* uncomp) {
    // Refill compressed buffer from file when needed
    // Return next byte or -1 for EOF
}

bool FM9File::streamDecompressAndParse(File& file) {
    // Allocate buffers: compressed (4KB), decompressed (8KB), dictionary (32KB)
    // Initialize uzlib with callback
    // Decompress chunks, scan for "FM90" magic with sliding window
    // When found: capture header + FX JSON
    // When TINF_DONE: calculate gzipEndOffset_ from file position + 8 (trailer)
}
```

#### 2. AudioStreamFM9Wav - Synchronized WAV Streaming

**Files:** `src/audio_stream_fm9_wav.h`, `src/audio_stream_fm9_wav.cpp`

**CRITICAL:** Must be in its own translation unit to avoid ODR violations with Audio Library.

Custom AudioStream that reads WAV directly from FM9 file with bidirectional sync to VGM playback.

```cpp
class AudioStreamFM9Wav : public AudioStream {
public:
    AudioStreamFM9Wav();
    virtual ~AudioStreamFM9Wav();

    // Load WAV from offset within FM9 file (no temp file needed)
    bool loadFromOffset(const char* path, uint32_t audioOffset, uint32_t audioSize);
    void closeFile();
    bool isLoaded() const { return fileLoaded_; }

    // Playback control
    void play();
    void stop();
    void pause();
    void resume();
    bool isPlaying() const { return playing_ && !paused_; }

    // Position tracking
    uint32_t getPositionSamples() const;
    uint32_t getTotalSamples() const;
    uint32_t getDurationMs() const;

    // BIDIRECTIONAL SYNCHRONIZATION
    // Called from FM9Player::update() with VGM's current sample position
    void setTargetSample(uint32_t targetSample);
    int32_t getSyncDrift() const;  // Positive = ahead, negative = behind

    // Buffer management (call from main loop)
    void refillBuffer();
    bool needsRefill() const;
    size_t getBufferLevel() const;

    // Audio Library ISR callback
    void update() override;

    // Diagnostics
    uint32_t getUnderruns() const { return underruns_; }

private:
    // File state
    File file_;
    bool fileLoaded_;
    uint32_t baseOffset_;       // Where WAV starts in FM9 file
    uint32_t dataStartOffset_;  // Where PCM data begins (after WAV header)
    uint32_t totalSamples_;
    uint32_t currentSample_;

    // Playback state
    volatile bool playing_;
    volatile bool paused_;

    // PSRAM ring buffer (~186ms at 44.1kHz)
    static const size_t BUFFER_SAMPLES = 8192;
    static const size_t REFILL_THRESHOLD = 4096;
    int16_t* readBufferLeft_;   // PSRAM
    int16_t* readBufferRight_;  // PSRAM
    volatile size_t bufferReadPos_;
    volatile size_t bufferWritePos_;
    volatile size_t bufferAvailable_;

    // Sync state
    volatile uint32_t targetSample_;
    volatile bool seekRequested_;
    volatile uint32_t seekTargetSample_;

    uint32_t underruns_;
};
```

**Global Instance (main.cpp):**
```cpp
// Static stack allocation for Audio Library registration
static AudioStreamFM9Wav g_fm9WavStream_obj;
AudioStreamFM9Wav* g_fm9WavStream = &g_fm9WavStream_obj;

// Audio connections to dacNesMixer channel 3
AudioConnection patchFM9WavL(g_fm9WavStream_obj, 0, dacNesMixerLeft, 3);
AudioConnection patchFM9WavR(g_fm9WavStream_obj, 1, dacNesMixerRight, 3);
```

#### 3. FX Engine
**Files:** `src/fx_engine.h`, `src/fx_engine.cpp`

Manages effect automation timeline. Uses `_changed` flags instead of NaN checks.

```cpp
struct FXEvent {
    uint32_t time_ms;

    // Effect parameters (NAN = no change from previous)
    float reverb_room_size;
    float reverb_damping;
    float reverb_wet;
    bool reverb_enabled;
    bool reverb_changed;        // True if any reverb param in this event

    float delay_time_ms;
    float delay_feedback;
    float delay_wet;
    bool delay_enabled;
    bool delay_changed;

    float chorus_rate;
    float chorus_depth;
    float chorus_wet;
    bool chorus_enabled;
    bool chorus_changed;

    float eq_low_gain;
    float eq_mid_gain;
    float eq_mid_freq;
    float eq_high_gain;
    bool eq_changed;

    float master_volume;
    bool master_volume_changed;

    FXEvent() {
        // Initialize all floats to NAN, all _changed flags to false
    }
};

class FXEngine {
public:
    bool loadFromJson(const char* json, size_t length);
    void clear();
    void reset();  // Reset to beginning of timeline

    // Call from player's update() with current position
    void update(uint32_t position_ms);

    bool hasEvents() const { return eventCount_ > 0; }
    size_t getEventCount() const { return eventCount_; }

private:
    static const size_t MAX_EVENTS = 64;
    FXEvent events_[MAX_EVENTS];  // Fixed array (no heap)
    size_t eventCount_;
    size_t currentEventIndex_;
    FXEvent currentState_;        // Cumulative state

    void applyEvent(const FXEvent& event);
    // Skeleton implementations - print to Serial for now
    void applyReverb(const FXEvent& event);
    void applyDelay(const FXEvent& event);
    void applyChorus(const FXEvent& event);
    void applyEQ(const FXEvent& event);
    void applyMasterVolume(const FXEvent& event);
};
```

#### 4. FM9 Player
**Files:** `src/fm9_player.h`, `src/fm9_player.cpp`

Wraps VGMPlayer and adds audio sync + FX automation.

```cpp
class FM9Player : public IAudioPlayer {
public:
    explicit FM9Player(const PlayerConfig& config);
    ~FM9Player();

    // IAudioPlayer interface - delegates to VGMPlayer
    bool loadFile(const char* filename) override;
    void play() override;
    void stop() override;
    void pause() override;
    void resume() override;
    void update() override;
    void setCompletionCallback(CompletionCallback callback) override;

    PlayerState getState() const override;
    bool isPlaying() const override;
    bool isPaused() const override;
    bool isStopped() const override;

    uint32_t getDurationMs() const override;
    uint32_t getPositionMs() const override;
    float getProgress() const override;
    const char* getFileName() const override;
    FileFormat getFormat() const override { return FileFormat::FM9; }

    // FM9-specific
    bool hasAudio() const { return fm9File_.hasAudio(); }
    bool hasFX() const { return fm9File_.hasFX(); }
    bool hasImage() const { return fm9File_.hasImage(); }
    ChipType getChipType() const;  // Delegates to VGMPlayer

private:
    PlayerConfig config_;
    FileSource* fileSource_;

    VGMPlayer* vgmPlayer_;      // Owned - handles VGM playback
    FM9File fm9File_;           // FM9 extension parsing
    FXEngine fxEngine_;         // FX automation

    // Audio state (uses global g_fm9WavStream)
    bool audioPlaying_;

    // Mixer control for WAV gain
    AudioMixer4* dacNesMixerLeft_;
    AudioMixer4* dacNesMixerRight_;
    static const int FM9_WAV_MIXER_CHANNEL = 3;
    float wavGain_;  // Default 0.8

    char currentFileName_[64];
    CompletionCallback completionCallback_;
    bool completionFired_;

    void startAudioPlayback();
    void stopAudioPlayback();
    void pauseAudioPlayback();
    void resumeAudioPlayback();
};
```

**Key Implementation Details:**

```cpp
bool FM9Player::loadFile(const char* filename) {
    // 1. Load FM9 file (finds header, locates audio/image offsets)
    fm9File_.loadFromFile(filename, fileSource_);

    // 2. Display cover image if present (MUST complete before playback starts)
    if (fm9File_.hasImage()) {
        displayCoverImage(filename, fm9File_.getImageOffset());
    }

    // 3. Create VGMPlayer and load VGM portion
    //    VGMPlayer treats FM9 as VGZ - ignores FM9 extensions after 0x66
    vgmPlayer_ = new VGMPlayer(config_);
    vgmPlayer_->loadFile(filename);

    // 4. Load FX timeline if present
    if (fm9File_.hasFX()) {
        fxEngine_.loadFromJson(fm9File_.getFXJson(), fm9File_.getFXJsonSize());
    }

    // 5. Load WAV directly from FM9 file offset (no temp file!)
    if (fm9File_.hasAudio() && fm9File_.getAudioFormat() == FM9_AUDIO_WAV) {
        g_fm9WavStream->loadFromOffset(filename,
                                        fm9File_.getAudioOffset(),
                                        fm9File_.getAudioSize());
    }
}

void FM9Player::displayCoverImage(const char* filename, uint32_t imageOffset) {
    // Open FM9 file and seek to image offset
    File file = SD.open(filename, FILE_READ);
    file.seek(imageOffset);

    // Read 20000 bytes (100x100 RGB565) directly to display
    // Option 1: Read row-by-row (2KB at a time) for lower memory usage
    uint16_t rowBuffer[FM9_IMAGE_WIDTH];
    for (int y = 0; y < FM9_IMAGE_HEIGHT; y++) {
        file.read(rowBuffer, FM9_IMAGE_WIDTH * 2);
        // Draw row to TFT at appropriate position on "Now Playing" screen
        tft.drawRGBBitmap(imageX, imageY + y, rowBuffer, FM9_IMAGE_WIDTH, 1);
    }

    file.close();
}

void FM9Player::update() {
    vgmPlayer_->update();

    // SYNCHRONIZE WAV with VGM sample position
    if (audioPlaying_ && g_fm9WavStream->isPlaying()) {
        uint32_t vgmSamplePos = vgmPlayer_->getCurrentSample();
        g_fm9WavStream->setTargetSample(vgmSamplePos);

        if (g_fm9WavStream->needsRefill()) {
            g_fm9WavStream->refillBuffer();
        }
    }

    // Update FX engine
    if (fxEngine_.hasEvents()) {
        fxEngine_.update(getPositionMs());
    }

    // Check for natural completion
    if (!vgmPlayer_->isPlaying() && !completionFired_ && audioPlaying_) {
        completionFired_ = true;
        stopAudioPlayback();
        if (completionCallback_) completionCallback_();
    }
}
```

#### 5. Audio Routing

FM9 WAV audio routes through the existing submixer architecture:

```
WAV Stream → dacNesMixer ch3 → submixer → main mixer → output
```

**PlayerConfig additions:**
```cpp
struct PlayerConfig {
    // ... existing members ...
    AudioMixer4* dacNesMixerLeft;   // For FM9 WAV gain control
    AudioMixer4* dacNesMixerRight;
};
```

#### 6. Modify PlayerManager

Update format detection:
```cpp
FileFormat PlayerManager::detectFormat(const char* path) {
    const char* ext = getExtension(path);
    if (strcasecmp(ext, "fm9") == 0) return FileFormat::FM9;
    // ... existing formats
}
```

Add FM9Player creation:
```cpp
IAudioPlayer* PlayerManager::createPlayer(FileFormat format) {
    switch (format) {
        case FileFormat::FM9:
            return new FM9Player(config_);
        // ... existing cases
    }
}
```

#### 7. Update FileBrowser

Add `.fm9` to supported extensions:
```cpp
bool isPlayableFile(const char* filename) {
    const char* ext = getExtension(filename);
    return strcasecmp(ext, "fm9") == 0 ||
           strcasecmp(ext, "vgm") == 0 ||
           // ... other formats
}
```

---

## Testing Plan

### Converter Tests

1. **Basic FM9 output** - Convert MIDI to FM9 without audio/fx/image, verify it plays as VGM
2. **FM9 with WAV** - Convert with `--audio test.wav`, verify audio embedded
3. **FM9 with MP3** - Convert with `--audio test.mp3`, verify audio embedded
4. **FM9 with FX** - Convert with `--fx effects.json`, verify FX embedded
5. **FM9 with image** - Convert with `--image cover.png`, verify 20KB RGB565 appended
6. **Image scaling** - Test various aspect ratios (square, portrait, landscape)
7. **Image format support** - Test PNG, JPEG, GIF inputs
8. **Image size limits** - Verify rejection of images >4096x4096 or >10MB
9. **Image dithering** - Verify default 16-color Bayer dithering looks correct
10. **Image no-dither** - Verify `--no-dither` produces clean RGB565 output
11. **FM9 with all** - Audio + FX + image together
12. **VGZ fallback** - `--format vgz` produces standard VGZ
13. **VGM fallback** - `--format vgm` produces uncompressed VGM

### Teensy Tests

1. **VGM/VGZ unchanged** - Existing files still play correctly
2. **Basic FM9** - FM9 without audio/fx/image plays VGM portion
3. **FM9 + audio sync** - Audio starts and stays in sync with VGM
4. **FM9 + FX** - Effect changes occur at correct timestamps
5. **FX smooth transitions** - No pops/clicks when effects change
6. **FM9 + image** - Cover image displays on Now Playing screen before playback
7. **Image display timing** - Verify image fully renders before audio starts
8. **Large audio files** - Test with longer audio (3+ minutes)
9. **Error handling** - Corrupt/missing chunks handled gracefully

---

## Migration Path

1. **Phase 1: Converter** - Add FM9 output to fmconv, test thoroughly ✅
2. **Phase 1b: Cover image** - Add `--image` option and RGB565 conversion ✅
3. **Phase 2: Teensy FM9File** - Parse FM9, extract audio/image offsets, ignore FX
4. **Phase 3: Teensy FM9Player** - Integrate VGM + audio playback + cover image display
5. **Phase 4: Teensy FXEngine** - Add effect automation (stubs first)
6. **Phase 5: Real effects** - Implement actual Teensy Audio effects

Each phase is independently testable.

---

## Open Questions / Future Enhancements

- **Looping**: How should loops interact with audio/FX? Reset audio? Loop FX timeline?
- **Seeking**: Can we seek within FM9? Audio seeking is tricky with MP3.
- **Multiple audio tracks**: Future version could support stem mixing
- **Visualizations**: Embed beat markers or note data for display sync
- ~~**Metadata**: Extended metadata beyond GD3 (album art, lyrics)~~ **DONE** - Cover image support added

---

## File Size Estimates

| Content | Approximate Size |
|---------|-----------------|
| VGM data (3 min song) | 50-200 KB compressed |
| FM9 header | 24 bytes |
| FX JSON (typical) | 1-5 KB |
| Audio WAV (3 min, 44.1k stereo) | ~30 MB |
| Audio MP3 (3 min, 192kbps) | ~4 MB |
| Cover image (100x100 RGB565) | 20 KB (fixed) |

Recommendation: Use MP3 for embedded audio unless lossless is required. Cover images add minimal overhead (20 KB).
