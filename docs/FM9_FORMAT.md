# FM9 File Format Specification

FM9 is an extended VGM format designed for the FM-90s hardware chiptune player. It embeds optional PCM audio, effects automation, cover art, and source format metadata alongside standard VGM data.

## Design Goals

- **Backward compatible** - Standard VGM players ignore the FM9 extension
- **Streaming friendly** - Audio data stored uncompressed for direct playback
- **Compact** - VGM and metadata are gzip compressed
- **Self-describing** - Contains source format info for display

## File Structure

```
┌─────────────────────────────────────────┐
│  GZIP COMPRESSED SECTION                │
│  ┌───────────────────────────────────┐  │
│  │ VGM Data (with GD3 tag)           │  │
│  │ - Standard VGM 1.51 format        │  │
│  │ - Ends with 0x66 command          │  │
│  │ - Optional GD3 metadata tag       │  │
│  ├───────────────────────────────────┤  │
│  │ FM9 Header (24 bytes)             │  │
│  │ - Magic "FM90"                    │  │
│  │ - Version, flags, offsets         │  │
│  ├───────────────────────────────────┤  │
│  │ FX Data (optional)                │  │
│  │ - JSON effects automation         │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│  RAW (UNCOMPRESSED) SECTION             │
│  ┌───────────────────────────────────┐  │
│  │ Audio Data (optional)             │  │
│  │ - WAV or MP3 format               │  │
│  │ - Stored uncompressed for         │  │
│  │   streaming playback              │  │
│  ├───────────────────────────────────┤  │
│  │ Cover Image (optional)            │  │
│  │ - 100x100 RGB565                  │  │
│  │ - 20,000 bytes fixed size         │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## FM9 Header

The FM9 header appears immediately after the VGM data (including any GD3 tag) within the gzip-compressed section.

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | magic | "FM90" (0x46 0x4D 0x39 0x30) |
| 0x04 | 1 | version | Format version (currently 1) |
| 0x05 | 1 | flags | Bit flags (see below) |
| 0x06 | 1 | audio_format | 0=none, 1=WAV, 2=MP3 |
| 0x07 | 1 | source_format | Original file format code |
| 0x08 | 4 | audio_offset | Offset from header start to audio |
| 0x0C | 4 | audio_size | Audio data size in bytes |
| 0x10 | 4 | fx_offset | Offset from header start to FX |
| 0x14 | 4 | fx_size | FX JSON size in bytes |

**Total header size: 24 bytes**

### Flag Bits

| Bit | Mask | Name | Description |
|-----|------|------|-------------|
| 0 | 0x01 | HAS_AUDIO | Audio data present after gzip section |
| 1 | 0x02 | HAS_FX | Effects JSON present in gzip section |
| 2 | 0x04 | HAS_IMAGE | Cover image present after audio |

### Source Format Codes

The `source_format` byte indicates what format the file was converted from:

| Range | Category | Examples |
|-------|----------|----------|
| 0x00 | Unknown | - |
| 0x01-0x0F | Pass-through | VGM, VGZ, FM9 |
| 0x10-0x1F | MIDI-style | MID, XMI, MUS, HMP, HMI |
| 0x20-0x5F | Native OPL (AdPlug) | RAD, IMF, DRO, CMF, A2M, etc. |
| 0x60-0x9F | Tracker (OpenMPT) | MOD, S3M, XM, IT, etc. |

Common codes:

| Code | Extension | Format Name |
|------|-----------|-------------|
| 0x01 | VGM | Video Game Music |
| 0x02 | VGZ | Video Game Music (compressed) |
| 0x10 | MID | Standard MIDI |
| 0x14 | MUS | DMX MUS (DOOM) |
| 0x15 | HMP | HMI HMP |
| 0x20 | RAD | Reality AdLib Tracker |
| 0x21 | IMF | id Software IMF |
| 0x23 | DRO | DOSBox Raw OPL |
| 0x24 | CMF | Creative Music File |
| 0x60 | MOD | ProTracker |
| 0x61 | S3M | Scream Tracker 3 |
| 0x62 | XM | FastTracker 2 |
| 0x63 | IT | Impulse Tracker |

See `src/fm9_writer/source_format.h` for the complete list of ~120 format codes.

## VGM Data

Standard VGM 1.51 format with:
- YM3812 (OPL2) or YMF262 (OPL3) chip headers
- Optional GD3 metadata tag
- Loop points encoded when detected

### GD3 Tag

The GD3 tag (if present) contains UTF-16LE metadata:
- Track title (English and native)
- Album/game name (English and native)
- System name (English and native)
- Author/composer (English and native)
- Release date
- Converter info
- Notes

## Audio Data

When present (HAS_AUDIO flag set), raw audio data appears immediately after the gzip section:
- **WAV**: Standard RIFF WAV format
- **MP3**: MPEG-1 Audio Layer 3

Audio is stored uncompressed to allow streaming playback without decompressing the entire file.

## Cover Image

When present (HAS_IMAGE flag set), cover art appears after audio data:
- **Size**: 100x100 pixels
- **Format**: RGB565 (16-bit, little-endian)
- **Total bytes**: 20,000 (100 × 100 × 2)

RGB565 format:
```
Bit:  15 14 13 12 11 | 10 9 8 7 6 5 | 4 3 2 1 0
      R  R  R  R  R  | G  G G G G G | B B B B B
```

## Effects Data (FX)

When present (HAS_FX flag set), JSON effects automation is stored within the gzip section after the FM9 header. Format TBD.

## Reading FM9 Files

1. Check for gzip magic (0x1F 0x8B)
2. Decompress the gzip section
3. Parse VGM header and data
4. Search for "FM90" magic after VGM end
5. Parse FM9 header to locate optional components
6. Read raw audio/image data from after gzip section

## Compatibility

- **Standard VGM players**: Will play the VGM portion and ignore FM9 extension
- **FM-90s player**: Uses all FM9 features including audio mixing and cover display
- **fm9_extract.py**: Extracts all components to separate files

## Example

A typical FM9 file converted from S3M with OPL instruments and sample audio:

```
[GZIP START]
  VGM header (256 bytes)
  VGM commands (OPL3 register writes)
  VGM end command (0x66)
  GD3 tag (title, author, etc.)
  FM9 header (24 bytes)
    magic: "FM90"
    version: 1
    flags: 0x05 (HAS_AUDIO | HAS_IMAGE)
    audio_format: 1 (WAV)
    source_format: 0x61 (S3M)
    audio_offset: 24
    audio_size: 2048000
    fx_offset: 0
    fx_size: 0
[GZIP END]
[RAW WAV audio data - 2MB]
[RAW RGB565 image - 20KB]
```
