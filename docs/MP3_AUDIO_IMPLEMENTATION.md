# MP3 Compressed Audio Implementation

This document covers MP3 audio support for both the fmconv converter and the FM-90s Teensy player.

## Overview

FM9 files support embedding audio in WAV or MP3 format. MP3 compression (default) reduces file sizes by ~10x while maintaining synchronized playback with VGM.

**Key Challenge:** The Teensy WAV player uses a rate adjustment trick (consuming 127/128/129 samples per 128-sample output block with linear interpolation) to maintain sample-accurate sync with no audible artifacts. MP3 decoding is frame-based (1152 samples per frame), so we must apply this trick *after* decoding.

---

## Part 1: fmconv Converter (IMPLEMENTED)

### 1.1 CLI Interface

MP3 compression is **enabled by default** at 192kbps. Options:

```
--uncompressed-audio    Embed audio as WAV (skip MP3 compression)
--audio-bitrate <kbps>  MP3 bitrate: 96, 128, 160, 192, 256, 320 (default: 192)
```

**Usage Examples:**

```bash
# Convert S3M with OPL+samples - audio auto-compressed to MP3
fmconv input.s3m -o output.fm9

# Use lower bitrate for smaller files
fmconv input.s3m -o output.fm9 --audio-bitrate 128

# Disable compression (embed as WAV)
fmconv input.s3m -o output.fm9 --uncompressed-audio

# Add external WAV audio - auto-compressed to MP3
fmconv input.vgm -o output.fm9 --audio background.wav

# Add external MP3 audio - embedded as-is
fmconv input.vgm -o output.fm9 --audio background.mp3
```

### 1.2 Dependencies

**libmp3lame** (LAME encoder):
- Cross-platform, well-tested
- Simple API for encoding PCM to MP3
- Optional - gracefully falls back to WAV if not available

**Installing LAME:**

Windows (vcpkg):
```bash
vcpkg install lame:x64-windows
cmake -B build -DLAME_ROOT=C:/vcpkg/installed/x64-windows
```

Linux:
```bash
sudo apt install libmp3lame-dev  # Debian/Ubuntu
sudo dnf install lame-devel      # Fedora
```

macOS:
```bash
brew install lame
```

### 1.3 Implementation

Files:
- `src/audio/mp3_encoder.h` - MP3 encoder wrapper class
- `src/audio/mp3_encoder.cpp` - Implementation using libmp3lame

Key functions:
- `MP3Encoder` class - Streaming encoder with initialize/encode/finish pattern
- `encodePCMtoMP3()` - One-shot encoding of PCM buffer
- `encodeWAVtoMP3()` - Load WAV file and encode to MP3
- `encodeWAVDataToMP3()` - Encode WAV data buffer to MP3

Integration points in `unified_converter.cpp`:
1. OpenMPT path: PCM from tracker samples → MP3 (default) or WAV
2. `--audio` option: WAV files → MP3 (default), MP3 files embedded as-is
   - Embed as MP3

### 1.4 Testing

- Verify MP3 output is valid (playable in standard players)
- Compare file sizes: WAV vs MP3 at various bitrates
- Ensure GD3 metadata and VGM data are unaffected

---

## Part 2: Teensy Player

### 2.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     AudioStreamFM9Mp3                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────┐    ┌──────────────┐    ┌──────────────────────┐  │
│  │ SD Card  │───►│ MP3 Frame    │───►│ Decoded PCM          │  │
│  │ (MP3     │    │ Buffer       │    │ Ring Buffer          │  │
│  │  stream) │    │ (~2KB)       │    │ (8192 samples PSRAM) │  │
│  └──────────┘    └──────────────┘    └──────────────────────┘  │
│                         │                      │                │
│                         ▼                      ▼                │
│                  ┌─────────────┐    ┌──────────────────────┐   │
│                  │ Helix MP3   │    │ Rate-Adjusted        │   │
│                  │ Decoder     │    │ Interpolation        │   │
│                  │ (per frame) │    │ (127/128/129 trick)  │   │
│                  └─────────────┘    └──────────────────────┘   │
│                                              │                  │
│                                              ▼                  │
│                                     ┌────────────────┐         │
│                                     │ Audio Output   │         │
│                                     │ (128 samples)  │         │
│                                     └────────────────┘         │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Key Differences from WAV Implementation

| Aspect | WAV (`AudioStreamFM9Wav`) | MP3 (`AudioStreamFM9Mp3`) |
|--------|---------------------------|---------------------------|
| Read unit | Any number of samples | 1152 samples (1 frame) |
| SD read | Raw PCM, direct to ring buffer | Compressed, decode first |
| Seek | `file.seek(sample * 4)` | Find frame boundary, decode, discard |
| CPU | Minimal | ~15-20% for decode |
| Extra RAM | None | ~30KB Helix decoder state |
| Rate adjust | On raw samples | On decoded samples (same trick) |

### 2.3 Helix MP3 Decoder Integration

Use the [Helix MP3 decoder](https://github.com/ultraembedded/libhelix-mp3) (fixed-point, optimized for ARM):

**Key API Functions:**
```c
// Initialize decoder
HMP3Decoder MP3InitDecoder(void);

// Find next frame sync (0xFF 0xFB/0xFA/etc)
int MP3FindSyncWord(unsigned char *buf, int nBytes);

// Get frame info without decoding
int MP3GetNextFrameInfo(HMP3Decoder hMP3Decoder, MP3FrameInfo *info, unsigned char *buf);

// Decode one frame (1152 samples for 44.1kHz)
int MP3Decode(HMP3Decoder hMP3Decoder,
              unsigned char **inbuf, int *bytesLeft,
              short *outbuf, int useSize);

// Free decoder
void MP3FreeDecoder(HMP3Decoder hMP3Decoder);
```

**Frame Info Structure:**
```c
typedef struct {
    int bitrate;        // kbps
    int nChans;         // 1 or 2
    int samprate;       // Hz (44100, 48000, etc)
    int outputSamps;    // samples per channel per frame (1152 for Layer 3)
    // ... other fields
} MP3FrameInfo;
```

### 2.4 Class Design: `AudioStreamFM9Mp3`

```cpp
class AudioStreamFM9Mp3 : public AudioStream {
public:
    AudioStreamFM9Mp3();
    ~AudioStreamFM9Mp3();

    // Load MP3 from offset within FM9 file
    bool loadFromOffset(const char* path, uint32_t mp3Offset, uint32_t mp3Size);

    // Playback control (same as WAV)
    void play();
    void stop();
    void pause();
    void resume();

    // Sync interface (same as WAV)
    void setTargetSample(uint32_t targetSample);
    int32_t getSyncDrift() const;

    // Buffer management
    void refillBuffer();  // Call from main loop
    bool needsRefill() const;

    // AudioStream interface
    void update() override;

private:
    // File state
    File file_;
    uint32_t baseOffset_;      // Where MP3 data starts in file
    uint32_t mp3Size_;         // Total MP3 data size
    uint32_t fileReadPos_;     // Current read position in MP3 stream

    // Decoder state
    HMP3Decoder decoder_;
    MP3FrameInfo frameInfo_;

    // MP3 frame buffer (compressed data from SD)
    static const size_t FRAME_BUFFER_SIZE = 2048;  // Max MP3 frame ~1440 bytes
    uint8_t* frameBuffer_;     // Circular buffer for compressed data
    size_t frameBufferFill_;

    // Decoded PCM ring buffer (same as WAV)
    static const size_t BUFFER_SAMPLES = 8192;
    int16_t* decodedBufferLeft_;   // PSRAM
    int16_t* decodedBufferRight_;  // PSRAM
    volatile size_t bufferReadPos_;
    volatile size_t bufferWritePos_;
    volatile size_t bufferAvailable_;

    // Sample tracking
    uint32_t currentSample_;      // Output sample position
    uint32_t totalDecodedSamples_;  // Total samples decoded so far

    // Sync state (identical to WAV)
    volatile uint32_t targetSample_;
    volatile int8_t syncMode_;    // -1, 0, +1
    volatile bool syncEnabled_;
    static const int32_t SYNC_DEAD_ZONE = 64;
    static const int32_t SYNC_MAX_DRIFT = 4410;  // 100ms

    // Seek state
    volatile bool seekRequested_;
    volatile uint32_t seekTargetSample_;

    // Private methods
    bool decodeNextFrame();       // Decode one MP3 frame to ring buffer
    bool seekToSample(uint32_t sample);  // Seek to approximate position
    void deinterleaveFrame(const int16_t* interleaved, size_t samples);
};
```

### 2.5 Critical Implementation Details

#### 2.5.1 Rate Adjustment (The Sync Trick)

The `update()` ISR applies the same 127/128/129 interpolation on the *decoded* PCM buffer:

```cpp
void AudioStreamFM9Mp3::update() {
    // ... (same preamble as WAV: check playing, allocate blocks, etc.)

    // Calculate sync drift
    int8_t newSyncMode = 0;
    if (syncEnabled_) {
        int32_t drift = (int32_t)currentSample_ - (int32_t)targetSample_;
        if (drift < -SYNC_DEAD_ZONE) newSyncMode = 1;       // Speed up
        else if (drift > SYNC_DEAD_ZONE) newSyncMode = -1;  // Slow down
    }
    syncMode_ = newSyncMode;

    int inputSamples = AUDIO_BLOCK_SAMPLES + syncMode_;  // 127, 128, or 129

    // Linear interpolation (IDENTICAL to WAV implementation)
    size_t startReadPos = bufferReadPos_;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        uint32_t pos_fixed = ((uint32_t)i * ((inputSamples - 1) << 16)) / (AUDIO_BLOCK_SAMPLES - 1);
        uint32_t idx = pos_fixed >> 16;
        uint32_t frac = pos_fixed & 0xFFFF;

        size_t pos0 = (startReadPos + idx) % BUFFER_SAMPLES;
        size_t pos1 = (startReadPos + idx + 1) % BUFFER_SAMPLES;

        // Interpolate left channel
        int16_t l0 = decodedBufferLeft_[pos0];
        int16_t l1 = decodedBufferLeft_[pos1];
        left->data[i] = l0 + (((int32_t)(l1 - l0) * (int32_t)frac) >> 16);

        // Interpolate right channel
        int16_t r0 = decodedBufferRight_[pos0];
        int16_t r1 = decodedBufferRight_[pos1];
        right->data[i] = r0 + (((int32_t)(r1 - r0) * (int32_t)frac) >> 16);
    }

    bufferReadPos_ = (startReadPos + inputSamples) % BUFFER_SAMPLES;
    bufferAvailable_ -= inputSamples;
    currentSample_ += inputSamples;

    transmit(left, 0);
    transmit(right, 1);
    // ...
}
```

**This is the key insight:** The rate adjustment happens on the decoded PCM, not the MP3 stream. The MP3 decoder's frame granularity doesn't matter for sync.

#### 2.5.2 Buffer Refill (Main Loop)

```cpp
void AudioStreamFM9Mp3::refillBuffer() {
    // Handle seek request first
    if (seekRequested_) {
        seekToSample(seekTargetSample_);
        seekRequested_ = false;
    }

    // Decode frames until ring buffer is sufficiently full
    while (bufferAvailable_ < BUFFER_SAMPLES - 2304) {  // Room for 2 frames
        if (!decodeNextFrame()) break;
    }
}

bool AudioStreamFM9Mp3::decodeNextFrame() {
    // 1. Ensure we have enough compressed data
    if (frameBufferFill_ < 1024) {
        // Read more from SD
        size_t toRead = FRAME_BUFFER_SIZE - frameBufferFill_;
        size_t remaining = mp3Size_ - fileReadPos_;
        toRead = min(toRead, remaining);

        if (toRead > 0) {
            file_.read(frameBuffer_ + frameBufferFill_, toRead);
            frameBufferFill_ += toRead;
            fileReadPos_ += toRead;
        }
    }

    if (frameBufferFill_ == 0) return false;  // EOF

    // 2. Find sync word
    int offset = MP3FindSyncWord(frameBuffer_, frameBufferFill_);
    if (offset < 0) {
        frameBufferFill_ = 0;  // No sync found, discard
        return false;
    }

    // Shift buffer to align sync word
    if (offset > 0) {
        memmove(frameBuffer_, frameBuffer_ + offset, frameBufferFill_ - offset);
        frameBufferFill_ -= offset;
    }

    // 3. Decode frame
    unsigned char* inPtr = frameBuffer_;
    int bytesLeft = frameBufferFill_;
    int16_t tempBuffer[2304];  // 1152 stereo samples

    int err = MP3Decode(decoder_, &inPtr, &bytesLeft, tempBuffer, 0);
    if (err != 0) {
        // Handle error: skip this frame
        if (frameBufferFill_ > 0) {
            memmove(frameBuffer_, frameBuffer_ + 1, frameBufferFill_ - 1);
            frameBufferFill_--;
        }
        return true;  // Try again
    }

    // Update buffer state
    size_t consumed = frameBufferFill_ - bytesLeft;
    memmove(frameBuffer_, inPtr, bytesLeft);
    frameBufferFill_ = bytesLeft;

    // 4. Get frame info
    MP3GetLastFrameInfo(decoder_, &frameInfo_);
    size_t samplesPerChannel = frameInfo_.outputSamps;

    // 5. Deinterleave and add to ring buffer
    __disable_irq();
    for (size_t i = 0; i < samplesPerChannel; i++) {
        size_t writePos = bufferWritePos_;

        if (frameInfo_.nChans == 2) {
            decodedBufferLeft_[writePos] = tempBuffer[i * 2];
            decodedBufferRight_[writePos] = tempBuffer[i * 2 + 1];
        } else {
            // Mono: duplicate to both channels
            decodedBufferLeft_[writePos] = tempBuffer[i];
            decodedBufferRight_[writePos] = tempBuffer[i];
        }

        bufferWritePos_ = (writePos + 1) % BUFFER_SAMPLES;
        bufferAvailable_++;
    }
    __enable_irq();

    totalDecodedSamples_ += samplesPerChannel;
    return true;
}
```

#### 2.5.3 Seeking

MP3 seeking is approximate due to variable bitrate and frame structure:

```cpp
bool AudioStreamFM9Mp3::seekToSample(uint32_t targetSample) {
    // 1. Estimate byte position (assuming ~constant bitrate)
    // For 128kbps stereo: ~4 bytes per sample pair
    // More accurate: bytes_per_frame / samples_per_frame
    float bytesPerSample = (frameInfo_.bitrate * 1000.0f / 8.0f) / frameInfo_.samprate;
    uint32_t estimatedOffset = (uint32_t)(targetSample * bytesPerSample);

    // Clamp to file bounds
    estimatedOffset = min(estimatedOffset, mp3Size_ - 1024);

    // 2. Seek file
    file_.seek(baseOffset_ + estimatedOffset);
    fileReadPos_ = estimatedOffset;

    // 3. Clear buffers
    __disable_irq();
    bufferReadPos_ = 0;
    bufferWritePos_ = 0;
    bufferAvailable_ = 0;
    __enable_irq();
    frameBufferFill_ = 0;

    // 4. Read and find sync
    file_.read(frameBuffer_, FRAME_BUFFER_SIZE);
    frameBufferFill_ = FRAME_BUFFER_SIZE;

    int offset = MP3FindSyncWord(frameBuffer_, frameBufferFill_);
    if (offset < 0) return false;

    // 5. Decode one frame to get accurate sample position
    // (We don't know exactly which sample this frame starts at)

    // 6. Set current position estimate
    // The rate adjustment will correct any error over time
    currentSample_ = targetSample;
    totalDecodedSamples_ = targetSample;

    // Pre-fill buffer
    for (int i = 0; i < 4; i++) {
        if (!decodeNextFrame()) break;
    }

    return true;
}
```

**Note:** Seeking is inherently approximate with MP3. The sync mechanism will correct any position error within a few hundred milliseconds. For loop points, there may be a brief (~10-50ms) desync that quickly corrects itself.

### 2.6 Memory Requirements

| Component | Size | Location |
|-----------|------|----------|
| Helix decoder state | ~30KB | DMAMEM or heap |
| Frame buffer | 2KB | DMAMEM |
| Decoded PCM buffer (L) | 16KB | PSRAM |
| Decoded PCM buffer (R) | 16KB | PSRAM |
| **Total** | ~64KB | Mixed |

Teensy 4.1 has 1MB RAM + 8MB PSRAM, so this is feasible.

### 2.7 FM9Player Integration

Modify `FM9Player` to detect audio format and use appropriate stream:

```cpp
// In fm9_player.cpp
if (fm9File_.hasAudio()) {
    if (fm9File_.getAudioFormat() == FM9_AUDIO_WAV) {
        // Use existing AudioStreamFM9Wav
        g_fm9WavStream->loadFromOffset(filename,
            fm9File_.getAudioOffset(), fm9File_.getAudioSize());
    } else if (fm9File_.getAudioFormat() == FM9_AUDIO_MP3) {
        // Use new AudioStreamFM9Mp3
        g_fm9Mp3Stream->loadFromOffset(filename,
            fm9File_.getAudioOffset(), fm9File_.getAudioSize());
    }
}
```

### 2.8 Audio System Wiring

Add the MP3 stream to the audio graph:

```cpp
// In main.cpp
AudioStreamFM9Mp3* g_fm9Mp3Stream = nullptr;

void setup() {
    // ...
    g_fm9Mp3Stream = new AudioStreamFM9Mp3();

    // Connect to same mixer as WAV stream
    new AudioConnection(*g_fm9Mp3Stream, 0, dacNesMixerLeft, FM9_WAV_MIXER_CHANNEL);
    new AudioConnection(*g_fm9Mp3Stream, 1, dacNesMixerRight, FM9_WAV_MIXER_CHANNEL);
}
```

---

## Part 3: Testing Plan

### 3.1 Converter Tests

1. **Basic encoding**: Convert WAV to MP3 at various bitrates
2. **OpenMPT path**: S3M with samples → FM9 with MP3 audio
3. **File size verification**: Compare WAV vs MP3 sizes
4. **Playback verification**: Output MP3 plays correctly in standard players

### 3.2 Teensy Tests

1. **Basic playback**: Load FM9 with MP3, verify audio plays
2. **Sync accuracy**: Compare VGM position to audio position
3. **Loop handling**: Test VGM loop detection and audio seek
4. **Buffer underrun**: Verify no glitches during SD contention
5. **CPU usage**: Profile with and without MP3 decode

### 3.3 Edge Cases

- Very short files (<1 second)
- VBR MP3 files
- Mono source audio
- Files with ID3 tags
- Seek during playback

---

## Part 4: Future Considerations

### 4.1 ADPCM Alternative

If MP3 proves problematic, ADPCM offers:
- Sample-accurate seeking
- Simpler decoder (~100 lines)
- 4:1 compression (vs 10:1 for MP3)
- No frame granularity issues

### 4.2 Streaming Improvements

- **Xing/LAME header**: For accurate seeking with VBR MP3
- **Frame index**: Pre-build index of frame positions for instant seeking
- **Gapless playback**: Handle encoder padding

---

## References

- [Frank Boesing's Arduino-Teensy-Codec-lib](https://github.com/FrankBoesing/Arduino-Teensy-Codec-lib)
- [Helix MP3 Decoder (RISC-V port)](https://github.com/ultraembedded/libhelix-mp3)
- [Phil Schatzmann's arduino-libhelix](https://github.com/pschatzmann/arduino-libhelix)
- [Silicon Labs AN1112: Helix MP3 Decoder](https://www.silabs.com/documents/public/application-notes/an1112-efm32-helix-mp3-decoder.pdf)
- [Adafruit MP3 Library](https://github.com/adafruit/Adafruit_MP3)
