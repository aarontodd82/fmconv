# fmconv Technical Documentation

This document describes the architecture and implementation of fmconv, a converter that transforms retro game music formats into FM9/VGM for hardware OPL playback on FM-90s.

## Architecture Overview

fmconv uses three conversion engines, selected automatically based on input format:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         unified_converter.cpp                        │
│                                                                      │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│   │  MIDI-style  │  │  Native OPL  │  │   Tracker Formats        │  │
│   │  (libADLMIDI)│  │  (AdPlug)    │  │   (OpenMPT)              │  │
│   └──────┬───────┘  └──────┬───────┘  └────────────┬─────────────┘  │
│          │                 │                       │                 │
│          ▼                 ▼                       ▼                 │
│   ┌──────────────────────────────────────────────────────────────┐  │
│   │                    VGM Generation                             │  │
│   │  (OPL register writes with timing)                            │  │
│   └──────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              ▼                                       │
│   ┌──────────────────────────────────────────────────────────────┐  │
│   │                    FM9Writer                                  │  │
│   │  VGM + Audio + FX + Cover Image → Gzipped FM9                │  │
│   └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

## Conversion Engines

### 1. libADLMIDI (MIDI-style formats)

**Formats:** MIDI, XMI, MUS, HMP/HMI, KLM

**Process:**
1. Load MIDI-style file via libADLMIDI
2. Select FM instrument bank (auto-detected or user-specified)
3. Replace internal OPL emulator with `VGMOPL3` capture class
4. Play through file, capturing all OPL register writes with sample-accurate timing
5. Generate VGM from captured data

**Key files:**
- `src/vgm_writer/vgm_chip.cpp` - `VGMOPL3` class implements libADLMIDI's chip interface
- `src/detection/bank_detector.cpp` - Auto-detects appropriate FM bank from filename/content

### 2. AdPlug (Native OPL formats)

**Formats:** RAD, IMF, DRO, CMF, A2M, D00, and 40+ others

**Process:**
1. Create `CVgmOpl` instance (captures OPL writes)
2. Load file via AdPlug's factory pattern
3. Play through file tick-by-tick, capturing register writes
4. Detect loop points by tracking pattern positions
5. Generate VGM with loop encoding

**Key files:**
- `src/adplug_vgm/vgm_opl.cpp` - `CVgmOpl` implements AdPlug's `Copl` interface

### 3. OpenMPT (Tracker formats)

**Formats:** S3M, MOD, XM, IT, and 60+ others

**Process:**
1. Load module via OpenMPT's `CSoundFile`
2. Detect instrument types (OPL vs samples)
3. **OPL export:** Inject `OPLCaptureLogger` into OpenMPT's OPL subsystem, play through capturing register writes
4. **Sample export:** Reset module, disable OPL, render audio to PCM buffer
5. Generate VGM from OPL data, embed sample audio as WAV in FM9

**Key insight:** OpenMPT's `OPL::IRegisterLogger` interface allows intercepting all OPL register writes during playback. We implement this interface to capture writes with sample-accurate timing.

**Key files:**
- `src/openmpt/openmpt_export.cpp` - Compiled as part of libopenmpt to access internal APIs
- `src/openmpt/openmpt_export.h` - C API for fmconv to call

## OPL Register Capture

All three engines use the same fundamental approach: intercept OPL register writes during playback and record them with timing information.

### Register Write Structure

```cpp
struct RegisterWrite {
    uint64_t sample_offset;  // Sample position (at 44100 Hz)
    uint8_t reg_lo;          // Register number (0x00-0xFF)
    uint8_t reg_hi;          // Bank: 0 = low (0x000-0x0FF), 1 = high (0x100-0x1FF)
    uint8_t value;           // Value written
};
```

### VGM Command Generation

OPL register writes are encoded as VGM commands:

| Command | Bytes | Description |
|---------|-------|-------------|
| `0x5E rr vv` | 3 | OPL3 port 0 write (registers 0x00-0xFF) |
| `0x5F rr vv` | 3 | OPL3 port 1 write (registers 0x100-0x1FF) |
| `0x61 nn nn` | 3 | Wait N samples (16-bit little-endian) |
| `0x62` | 1 | Wait 735 samples (1/60 second at 44100 Hz) |
| `0x63` | 1 | Wait 882 samples (1/50 second at 44100 Hz) |
| `0x70-0x7F` | 1 | Wait 1-16 samples |
| `0x66` | 1 | End of sound data |

### Optimization

To minimize file size, we:
- Skip redundant writes (same value to same register)
- Use short wait commands when possible (0x62, 0x63, 0x70-0x7F)
- Combine consecutive delays into single wait commands

## FM9 File Format

FM9 extends VGM to support embedded audio, effects automation, and cover art.

### File Structure

```
┌─────────────────────────────────────┐
│         GZIP COMPRESSED             │
│  ┌───────────────────────────────┐  │
│  │  VGM Data                     │  │
│  │  (header + commands + GD3)    │  │
│  ├───────────────────────────────┤  │
│  │  FM9 Header (24 bytes)        │  │
│  ├───────────────────────────────┤  │
│  │  FX Chunk (optional JSON)     │  │
│  └───────────────────────────────┘  │
├─────────────────────────────────────┤
│       UNCOMPRESSED (raw)            │
│  ┌───────────────────────────────┐  │
│  │  Audio (WAV or MP3)           │  │
│  ├───────────────────────────────┤  │
│  │  Cover Image (RGB565)         │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

**Design rationale:** Audio and image are stored uncompressed after the gzip section. This allows FM-90s to:
1. Stream-decompress VGM data (same as VGZ playback)
2. Stream-copy audio directly without decompression
3. Read cover image directly for display

### FM9 Header

```c
struct FM9Header {
    char     magic[4];        // "FM90"
    uint8_t  version;         // Format version (1)
    uint8_t  flags;           // Bit 0: audio, Bit 1: FX, Bit 2: image
    uint8_t  audio_format;    // 0=none, 1=WAV, 2=MP3
    uint8_t  reserved;
    uint32_t audio_offset;    // (not used - audio follows gzip)
    uint32_t audio_size;      // Size of audio data
    uint32_t fx_offset;       // Offset to FX JSON from header start
    uint32_t fx_size;         // Size of FX JSON
};
// Total: 24 bytes
```

### Cover Image Format

- **Dimensions:** 100x100 pixels (fixed)
- **Format:** RGB565 (16-bit: 5R, 6G, 5B)
- **Size:** 20,000 bytes
- **Processing:** Bilinear scaling, optional 16-color Bayer dithering

## S3M Hybrid Processing

S3M files are unique because they can contain both OPL FM instruments and PCM sample instruments in the same song. fmconv handles this with a dual-pass approach:

### Detection

```cpp
for (SAMPLEINDEX smp = 1; smp <= sndFile->GetNumSamples(); ++smp) {
    const auto& sample = sndFile->GetSample(smp);
    if (sample.uFlags[CHN_ADLIB]) {
        has_opl = true;
    } else if (sample.HasSampleData()) {
        has_samples = true;
    }
}
```

### Pass 1: OPL Capture

1. Inject `OPLCaptureLogger` into OpenMPT's OPL subsystem
2. Play through the entire module
3. Capture all OPL register writes with timing
4. Generate VGM from captured data

### Pass 2: Sample Render

1. Reload module (fresh state)
2. Disable OPL: `sndFile->m_opl.reset()`
3. Render audio to stereo 16-bit PCM buffer
4. Wrap as WAV for FM9 embedding

### Sample-Only Fallback

For tracker files with no OPL instruments (MOD, XM, IT, etc.):

1. Detect no OPL content
2. Render samples to PCM
3. Generate "timing-only" VGM with just wait commands matching audio duration
4. Embed audio in FM9

The timing VGM ensures FM-90s knows the track duration and can sync playback.

## Build Configuration

### CMake Structure

```
fmconv/
├── CMakeLists.txt              # Main build
├── cmake/
│   └── libopenmpt.cmake        # OpenMPT build config
├── libADLMIDI/                 # Submodule
├── adplug/                     # Submodule
├── libbinio/                   # Submodule
├── openmpt/                    # Submodule
└── src/
    ├── unified_converter.cpp   # Main entry point
    ├── vgm_writer/             # VGM generation
    ├── adplug_vgm/             # AdPlug OPL capture
    ├── fm9_writer/             # FM9 output
    └── openmpt/                # OpenMPT integration
```

### OpenMPT Integration

`openmpt_export.cpp` is compiled as part of libopenmpt (not fmconv) because it needs access to OpenMPT internal APIs:

```cmake
# cmake/libopenmpt.cmake
set(OPENMPT_EXPORT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/openmpt/openmpt_export.cpp
)

add_library(libopenmpt STATIC
    ${LIBOPENMPT_SOURCES}
    ${SOUNDLIB_SOURCES}
    ...
    ${OPENMPT_EXPORT_SOURCES}  # Our code, compiled in OpenMPT context
)
```

### Compile Definitions

```cmake
target_compile_definitions(libopenmpt PRIVATE
    LIBOPENMPT_BUILD
    MPT_WITH_MINIZ           # Compression
    MPT_WITH_MINIMP3         # MP3 samples
    MPT_WITH_STBVORBIS       # Vorbis samples
    NO_PLUGINS               # Disable VST
    NO_DMO                   # Disable DirectX
    NO_VST                   # Disable VST
)
```

## Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| libADLMIDI | MIDI synthesis with FM banks | LGPLv2.1+ |
| AdPlug | Native OPL format playback | LGPLv2.1 |
| libbinio | Binary I/O for AdPlug | LGPLv2.1+ |
| OpenMPT | Tracker format playback | BSD-3-Clause |
| miniz | Gzip compression | MIT |
| stb_image | Image loading | Public Domain |
| minimp3 | MP3 decoding (for OpenMPT) | CC0 |
| stb_vorbis | Vorbis decoding (for OpenMPT) | Public Domain |

## VGM Format Details

fmconv generates VGM 1.51 files with YMF262 (OPL3) chip header.

### Header Layout (256 bytes)

| Offset | Size | Description |
|--------|------|-------------|
| 0x00 | 4 | "Vgm " magic |
| 0x04 | 4 | EOF offset (relative to 0x04) |
| 0x08 | 4 | Version (0x00000151) |
| 0x18 | 4 | Total samples |
| 0x1C | 4 | Loop offset (relative to 0x1C) |
| 0x20 | 4 | Loop samples |
| 0x34 | 4 | VGM data offset (relative to 0x34) |
| 0x5C | 4 | YMF262 clock (14318180 Hz) |

### Timing

- Sample rate: 44100 Hz
- OPL3 clock: 14318180 Hz (NTSC colorburst × 4)
- All timing is in samples (not OPL clock cycles)
