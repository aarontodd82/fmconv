# S3M and Tracker Format to FM9 Conversion

## Problem Statement

S3M (Scream Tracker 3) is unique among tracker formats - it can contain **both OPL/Adlib FM instruments AND sample-based instruments** in the same file. Our current AdPlug-based conversion only handles the OPL instruments and ignores samples entirely.

To create accurate FM9 files from S3M (and similar formats), we need to:
1. Export OPL register writes → VGM (for hardware OPL playback)
2. Export sample-based audio → WAV (for PCM playback alongside OPL)
3. Combine both into a single FM9 file

## Supported Formats

### Primary Target: S3M with OPL
- S3M files can mix OPL FM instruments with sample-based instruments
- OPL instruments use 12-byte patch definitions
- Channel flag `CHN_ADLIB = 0x200` indicates OPL activity

### Secondary Targets (Sample-only, no OPL)
These formats would export audio only (no VGM), useful for FM9's embedded audio feature:
- MOD (ProTracker)
- XM (FastTracker 2)
- IT (Impulse Tracker)
- MPTM (OpenMPT native)
- 60+ other formats supported by libopenmpt

## Architecture

### libopenmpt Integration

We will use [libopenmpt](https://lib.openmpt.org/libopenmpt/) as the core library:
- Cross-platform C/C++ library
- Loads S3M, MOD, XM, IT, and 60+ tracker formats
- Built-in OPL emulation for S3M Adlib instruments
- Renders audio to PCM (WAV export)

### Key libopenmpt Features

**Audio Rendering:**
```cpp
// Render to stereo float
size_t openmpt_module_read_interleaved_float_stereo(
    openmpt_module* mod,
    int32_t samplerate,
    size_t count,
    float* interleaved_stereo
);
```

**OPL Volume Control:**
```cpp
// Set OPL volume to 0 to mute FM instruments during sample export
openmpt_module_ctl_set_floatingpoint(mod, "render.opl.volume_factor", 0.0);
```

**Interactive Extension (for muting):**
```cpp
// Mute specific instruments
openmpt_module_ext_interface_interactive::set_instrument_mute_status(index, muted);
```

### OPL Register Capture

The OPL → VGM export requires capturing register writes. This code exists in OpenMPT's GUI application but is not exposed in libopenmpt.

**Source:** `mptrack/OPLExport.cpp` from [OpenMPT repository](https://github.com/OpenMPT/openmpt)

**Key Components:**

1. **IRegisterLogger Interface** (from `soundlib/OPL.h`):
```cpp
class IRegisterLogger {
public:
    virtual void Port(CHANNELINDEX c, Register reg, Value value) = 0;
    virtual void MoveChannel(CHANNELINDEX from, CHANNELINDEX to) = 0;
    virtual ~IRegisterLogger() {}
};
```

2. **OPLCapture Class** - Implements IRegisterLogger to collect register writes:
   - Stores `{sampleOffset, regLo, regHi, value}` tuples
   - Tracks previous register states to avoid redundant writes
   - Captures snapshot at loop points for seamless VGM loops

3. **VGM Writing** - Serializes captured data:
   - 256-byte VGM header with OPL3 clock (14,318,180 Hz)
   - Register write commands with timing delays
   - Loop point encoding
   - GD3 metadata block
   - Optional gzip compression for VGZ

### Detection: OPL vs Sample Instruments

**Channel-level detection:**
```cpp
// From soundlib/Snd_defs.h
enum ChannelFlags {
    CHN_ADLIB = 0x200,  // Adlib/OPL instrument active on this channel
    CHN_MUTE  = 0x400,  // Muted channel
    // ...
};
```

**Instrument-level detection:**
- OPL instruments have 12-byte patch data (`OPLPatch = std::array<uint8, 12>`)
- Sample instruments have waveform data pointers

## Implementation Plan

### Phase 1: libopenmpt Integration

1. Add libopenmpt as a dependency (or git submodule)
2. Create `src/openmpt_converter.cpp` with basic S3M loading
3. Implement WAV export using `openmpt_module_read_interleaved_float_stereo()`
4. Test with sample-only S3M files

### Phase 2: OPL Register Capture

1. Extract/adapt OPL capture logic from OpenMPT source
2. Implement `IRegisterLogger` to collect register writes
3. Hook into libopenmpt's playback loop (may require modifications)
4. Generate VGM from captured registers

**Challenge:** libopenmpt doesn't expose the OPL register interface. Options:
- Fork libopenmpt and add register logging callback
- Use OpenMPT's soundlib directly instead of libopenmpt
- Request feature addition from OpenMPT developers

### Phase 3: Dual Export Pipeline

1. **First pass:** Export with `render.opl.volume_factor = 1.0`, capture OPL → VGM
2. **Second pass:** Export with `render.opl.volume_factor = 0.0`, render samples → WAV
3. Combine VGM + WAV into FM9

### Phase 4: CLI Integration

Add new options to fmconv:
```
--openmpt           Force libopenmpt engine (auto-detected for S3M/MOD/XM/IT)
--samples-only      Export only sample audio (no OPL VGM)
--opl-only          Export only OPL VGM (no sample audio)
```

### Phase 5: Additional Formats

Extend support to other libopenmpt formats:
- MOD, XM, IT → WAV only (no OPL)
- MPTM with OPL → VGM + WAV (same as S3M)

## File Structure Changes

```
fmconv/
├── src/
│   ├── openmpt/
│   │   ├── openmpt_converter.cpp   # Main conversion logic
│   │   ├── openmpt_converter.h
│   │   ├── opl_capture.cpp         # OPL register logging
│   │   ├── opl_capture.h
│   │   └── vgm_writer.cpp          # VGM generation from registers
│   └── unified_converter.cpp       # Updated to route S3M to openmpt
├── libopenmpt/                     # Submodule or system library
└── ...
```

## Dependencies

### libopenmpt
- Source: https://lib.openmpt.org/libopenmpt/
- License: BSD-3-Clause
- Build: CMake compatible

### OpenMPT soundlib (for OPL capture)
- Source: https://github.com/OpenMPT/openmpt/tree/master/soundlib
- Files needed:
  - `OPL.h`, `OPL.cpp` - OPL emulation and IRegisterLogger
  - `Snd_defs.h` - Channel flags and definitions
- License: BSD-3-Clause

## Technical Notes

### OPL Clock Rate
VGM files use OPL3 clock of 14,318,180 Hz (NTSC colorburst × 4)

### Sample Rate Considerations
- libopenmpt recommends 48000 Hz for output
- VGM timing is in samples at 44100 Hz
- May need sample rate conversion or timing adjustment

### Loop Detection
- OpenMPT tracks order/row positions
- Loop point = when playback returns to a previously seen position
- VGM encodes loop as offset + sample count

### Dual OPL2 vs OPL3
- S3M originally targeted OPL2 (9 channels)
- OpenMPT emulates OPL3 (18 channels, more waveforms)
- VGM can represent both; detect from register usage

## Open Questions

1. **libopenmpt modification vs soundlib direct use?**
   - libopenmpt is cleaner API but hides OPL internals
   - soundlib gives full access but more complex integration

2. **Build complexity**
   - libopenmpt has many dependencies (zlib, mpg123, vorbis, etc.)
   - May want minimal build with only S3M/MOD/XM/IT support

3. **Synchronization**
   - VGM and WAV must be sample-accurate aligned
   - Both exports must use identical playback timing

4. **Memory usage**
   - Large modules with long playback = large WAV files
   - May need streaming export for FM9 audio section

## References

- [libopenmpt Documentation](https://lib.openmpt.org/doc/)
- [OpenMPT GitHub Repository](https://github.com/OpenMPT/openmpt)
- [S3M Format Specification](https://moddingwiki.shikadi.net/wiki/S3M_Format)
- [VGM Format Specification](https://vgmrips.net/wiki/VGM_Specification)
- [OPL3 Programming Guide](https://moddingwiki.shikadi.net/wiki/OPL_chip)
