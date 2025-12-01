# FM9 Format Implementation Plan

## Overview

FM9 is an extended VGM format designed for the Teensy OPL3 player. It bundles standard VGM data with optional embedded audio (WAV/MP3) and real-time effects automation. FM9 files are always gzip compressed.

**Goals:**
- Default output format for fmconv
- Backwards compatible structure (VGM data remains parseable)
- Minimal changes to existing Teensy VGM player
- Optional audio and FX - FM9 without extras is just a renamed VGZ

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
│  │  Audio Chunk (optional)       │  │
│  │  (WAV or MP3 data)            │  │
│  ├───────────────────────────────┤  │
│  │  FX Chunk (optional)          │  │
│  │  (JSON effects timeline)      │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

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
- Bits 2-7: Reserved

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

## Converter Implementation (fmconv)

### New Command Line Options

```
fmconv <input> [options] -o <output>

Output Format:
  --format fm9|vgz|vgm    Output format (default: fm9)

FM9 Options:
  --audio <file>          Embed audio file (WAV or MP3)
  --fx <file>             Embed effects JSON file

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

    // Write complete FM9 (gzipped)
    bool write(const std::string& output_path);

private:
    std::vector<uint8_t> vgm_data_;
    std::vector<uint8_t> audio_data_;
    std::vector<uint8_t> fx_data_;
    uint8_t audio_format_ = 0;

    uint8_t detectAudioFormat(const std::string& path);
    bool loadAudioFile(const std::string& path);
    bool loadFXFile(const std::string& path);
    void buildFM9Header(FM9Header& header);
};
```

#### 2. Modify unified_converter.cpp

- Add `--format`, `--audio`, `--fx` argument parsing
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

---

## Teensy Player Implementation

### Overview

The FM9Player extends or wraps the existing VGMPlayer. When loading a file:

1. Check extension (`.fm9` vs `.vgm`/`.vgz`)
2. For FM9: Parse FM9 header, extract audio to temp file, load FX events
3. Play VGM portion using existing VGM playback code
4. Simultaneously play audio file and apply FX events

### New Files

```
src/
├── fm9_player.h
├── fm9_player.cpp
├── fm9_file.h
├── fm9_file.cpp
├── fx_engine.h
└── fx_engine.cpp
```

### Implementation Tasks

#### 1. FM9 File Parser
**Files:** `src/fm9_file.h`, `src/fm9_file.cpp`

```cpp
struct FM9Header {
    char magic[4];
    uint8_t version;
    uint8_t flags;
    uint8_t audio_format;
    uint8_t reserved;
    uint32_t audio_offset;
    uint32_t audio_size;
    uint32_t fx_offset;
    uint32_t fx_size;
};

class FM9File {
public:
    bool load(const char* path);

    // Access VGM portion (for existing VGM player)
    const uint8_t* getVGMData() const;
    size_t getVGMSize() const;

    // FM9 extensions
    bool hasAudio() const;
    bool hasFX() const;
    uint8_t getAudioFormat() const;  // 0=none, 1=WAV, 2=MP3

    // Extract audio to temp file for playback
    bool extractAudioToFile(const char* temp_path);

    // Get FX JSON string
    const char* getFXJson() const;
    size_t getFXJsonSize() const;

private:
    std::vector<uint8_t> decompressed_data_;
    FM9Header fm9_header_;
    size_t vgm_end_offset_;      // Where VGM data ends
    size_t fm9_header_offset_;   // Where FM9 header starts
    bool has_fm9_extension_ = false;

    bool findFM9Header();
};
```

#### 2. FX Engine
**Files:** `src/fx_engine.h`, `src/fx_engine.cpp`

Manages effect automation timeline.

```cpp
struct FXEvent {
    uint32_t time_ms;
    // Effect states (NaN = no change)
    float reverb_room_size;
    float reverb_damping;
    float reverb_wet;
    bool reverb_enabled;

    float delay_time_ms;
    float delay_feedback;
    float delay_wet;
    bool delay_enabled;

    float chorus_rate;
    float chorus_depth;
    float chorus_wet;
    bool chorus_enabled;

    float eq_low_gain;
    float eq_mid_gain;
    float eq_mid_freq;
    float eq_high_gain;

    float master_volume;
};

class FXEngine {
public:
    bool loadFromJson(const char* json, size_t length);

    // Call each frame with current playback position
    void update(uint32_t position_ms);

    // Apply current effect state to audio system
    void applyToAudioSystem(AudioSystem& audio);

private:
    std::vector<FXEvent> events_;
    size_t current_event_index_ = 0;
    FXEvent current_state_;

    void applyEvent(const FXEvent& event);
    bool parseJson(const char* json, size_t length);
};
```

#### 3. FM9 Player
**Files:** `src/fm9_player.h`, `src/fm9_player.cpp`

```cpp
class FM9Player : public IAudioPlayer {
public:
    FM9Player(PlayerConfig& config);
    ~FM9Player();

    // IAudioPlayer interface
    bool loadFile(const char* path) override;
    void play() override;
    void stop() override;
    void pause() override;
    void resume() override;
    void update() override;

    PlayerState getState() const override;
    bool isPlaying() const override;
    bool isPaused() const override;
    bool isStopped() const override;

    uint32_t getDurationMs() const override;
    uint32_t getPositionMs() const override;
    float getProgress() const override;
    const char* getFileName() const override;

private:
    PlayerConfig& config_;
    FM9File fm9_file_;
    FXEngine fx_engine_;

    // Reuse existing VGM playback logic
    // (either embed VGMPlayer or extract common code)
    VGMPlayer* vgm_player_ = nullptr;

    // Audio playback
    AudioPlaySdWav* wav_player_ = nullptr;
    AudioPlaySdMp3* mp3_player_ = nullptr;
    bool has_audio_ = false;
    uint8_t audio_format_ = 0;

    static constexpr const char* TEMP_AUDIO_PATH = "/TEMP/fm9_audio";

    void cleanupTempFiles();
};
```

#### 4. Modify PlayerManager

Update format detection to recognize `.fm9`:

```cpp
FileFormat PlayerManager::detectFormat(const char* path) {
    const char* ext = getExtension(path);
    if (strcasecmp(ext, "fm9") == 0) return FileFormat::FM9;
    if (strcasecmp(ext, "vgm") == 0) return FileFormat::VGM;
    if (strcasecmp(ext, "vgz") == 0) return FileFormat::VGM;
    // ... existing formats
}
```

Add FM9Player creation:

```cpp
IAudioPlayer* PlayerManager::createPlayer(FileFormat format) {
    switch (format) {
        case FileFormat::FM9:
            return new FM9Player(config_);
        case FileFormat::VGM:
            return new VGMPlayer(config_);
        // ... existing cases
    }
}
```

#### 5. Add Teensy Audio effects (placeholder stubs)

For effects not yet implemented, create stub classes:

```cpp
// src/audio_effects_stub.h
class AudioEffectDelayStub : public AudioStream {
public:
    void setTime(float ms) { /* TODO */ }
    void setFeedback(float fb) { /* TODO */ }
    void setWetMix(float wet) { /* TODO */ }
};

class AudioEffectChorusStub : public AudioStream {
public:
    void setRate(float hz) { /* TODO */ }
    void setDepth(float d) { /* TODO */ }
    void setWetMix(float wet) { /* TODO */ }
};
```

The FXEngine can call these stubs; they do nothing until implemented.

#### 6. Update FileBrowser

Add `.fm9` to supported extensions filter:

```cpp
bool isPlayableFile(const char* filename) {
    const char* ext = getExtension(filename);
    return strcasecmp(ext, "fm9") == 0 ||
           strcasecmp(ext, "vgm") == 0 ||
           strcasecmp(ext, "vgz") == 0 ||
           // ... other formats
}
```

---

## Testing Plan

### Converter Tests

1. **Basic FM9 output** - Convert MIDI to FM9 without audio/fx, verify it plays as VGM
2. **FM9 with WAV** - Convert with `--audio test.wav`, verify audio embedded
3. **FM9 with MP3** - Convert with `--audio test.mp3`, verify audio embedded
4. **FM9 with FX** - Convert with `--fx effects.json`, verify FX embedded
5. **FM9 with all** - Audio + FX together
6. **VGZ fallback** - `--format vgz` produces standard VGZ
7. **VGM fallback** - `--format vgm` produces uncompressed VGM

### Teensy Tests

1. **VGM/VGZ unchanged** - Existing files still play correctly
2. **Basic FM9** - FM9 without audio/fx plays VGM portion
3. **FM9 + audio sync** - Audio starts and stays in sync with VGM
4. **FM9 + FX** - Effect changes occur at correct timestamps
5. **FX smooth transitions** - No pops/clicks when effects change
6. **Large audio files** - Test with longer audio (3+ minutes)
7. **Error handling** - Corrupt/missing chunks handled gracefully

---

## Migration Path

1. **Phase 1: Converter** - Add FM9 output to fmconv, test thoroughly
2. **Phase 2: Teensy FM9File** - Parse FM9, extract audio, ignore FX
3. **Phase 3: Teensy FM9Player** - Integrate VGM + audio playback
4. **Phase 4: Teensy FXEngine** - Add effect automation (stubs first)
5. **Phase 5: Real effects** - Implement actual Teensy Audio effects

Each phase is independently testable.

---

## Open Questions / Future Enhancements

- **Looping**: How should loops interact with audio/FX? Reset audio? Loop FX timeline?
- **Seeking**: Can we seek within FM9? Audio seeking is tricky with MP3.
- **Multiple audio tracks**: Future version could support stem mixing
- **Visualizations**: Embed beat markers or note data for display sync
- **Metadata**: Extended metadata beyond GD3 (album art, lyrics)

---

## File Size Estimates

| Content | Approximate Size |
|---------|-----------------|
| VGM data (3 min song) | 50-200 KB compressed |
| FM9 header | 24 bytes |
| FX JSON (typical) | 1-5 KB |
| Audio WAV (3 min, 44.1k stereo) | ~30 MB |
| Audio MP3 (3 min, 192kbps) | ~4 MB |

Recommendation: Use MP3 for embedded audio unless lossless is required.
