# fmconv - FM9 Converter for FM-90s

Convert retro music to FM9 format for playback on [FM-90s](https://github.com/aarontodd82/FM-90s), a chiptune player with hardware YMF262 (OPL2/OPL3), YM2612 (Genesis/Mega Drive), and SN76489 (PSG) chips, plus software emulation of SNES, Game Boy, and NES sound.

## Overview

fmconv converts **100+ audio formats** to FM9, an extended VGM format that supports:

- **FM synthesis** - Hardware playback via OPL2/OPL3 or Genesis chips
- **Embedded PCM audio** - Layer WAV or MP3 audio alongside FM synthesis
- **Cover art** - Embed album/game artwork for display
- **Effects automation** - JSON-based timeline for reverb, delay, chorus, and EQ

The converter uses three engines:
1. **libADLMIDI** - MIDI-style formats (MIDI, XMI, MUS, HMP/HMI) with 79 selectable FM instrument banks
2. **AdPlug** - Native OPL tracker formats (RAD, IMF, DRO, etc.) with embedded instruments
3. **OpenMPT** - Tracker formats (S3M, MOD, XM, IT, etc.) with hybrid OPL+sample support

VGM and VGZ output is also supported for use with other players.

### S3M Hybrid Support

S3M files can contain both OPL FM instruments and PCM samples in the same song. fmconv handles this by:
- Capturing OPL register writes for hardware playback
- Rendering sample instruments to audio embedded in the FM9 file

The FM-90s player then plays both simultaneously - real OPL3 hardware for the FM parts alongside the sample audio. Sample-only tracker files (MOD, XM, IT, etc.) are also supported and output as FM9 with embedded audio.

## Quick Start

```bash
# Convert to FM9 (default)
fmconv game_music.mid

# Convert S3M with OPL+samples (auto-detects hybrid content)
fmconv hybrid_track.s3m

# Add PCM audio track alongside FM
fmconv chiptune.rad --audio drums.wav

# Add effects automation
fmconv tune.mid --fx effects.json

# Convert existing VGM/VGZ to FM9 with audio
fmconv existing.vgz --audio vocals.mp3

# Output as VGZ instead of FM9
fmconv doom.mus --vgz

# Use specific instrument bank for MIDI
fmconv doom.mus --bank 16
```

## Supported Formats

| Engine | Formats | Notes |
|--------|---------|-------|
| **libADLMIDI** | MIDI, XMI, MUS, HMP/HMI | Requires FM bank selection |
| **AdPlug** | RAD, IMF, DRO, CMF, ROL, 40+ more | Embedded FM instruments |
| **OpenMPT** | S3M, MOD, XM, IT, 60+ more | S3M supports OPL; others render to audio |

See [docs/SUPPORTED_FORMATS.md](docs/SUPPORTED_FORMATS.md) for the complete list, or use `--list-formats`.

## Command-Line Options

### General Options

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output filename (default: input name with .fm9) |
| `-y, --yes` | Non-interactive mode (skip prompts) |
| `--no-suffix` | Don't add format suffix (_RAD, _MID, etc.) to filename |
| `--verbose` | Enable verbose output |
| `-h, --help` | Show help message |

### Output Format Options

| Option | Description |
|--------|-------------|
| `--format <fmt>` | Output format: `fm9` (default), `vgz`, or `vgm` |
| `--vgz` | Shorthand for `--format vgz` |
| `--vgm` | Shorthand for `--format vgm` |

### FM9 Options

| Option | Description |
|--------|-------------|
| `--audio <file>` | Embed WAV or MP3 audio file for PCM playback alongside FM |
| `--audio-bitrate <kbps>` | MP3 bitrate: 96, 128, 160, 192, 256, 320 (default: 192) |
| `--uncompressed-audio` | Embed audio as WAV instead of MP3 |
| `--fx <file>` | Embed effects automation JSON file |
| `--image <file>` | Embed cover image (PNG, JPEG, or GIF) for display on FM-90s |
| `--no-dither` | Disable retro styling on cover image |

### MIDI-Style Format Options

These options only apply to MIDI, XMI, MUS, HMP/HMI files:

| Option | Description |
|--------|-------------|
| `-b, --bank <N>` | FM instrument bank (0-78, default: auto-detect) |
| `-v, --vol-model <N>` | Volume model (0-11, default: 0 = auto) |

### Native OPL Format Options

These options only apply to AdPlug formats (RAD, IMF, DRO, etc.):

| Option | Description |
|--------|-------------|
| `-s, --subsong <N>` | Subsong number for multi-song files |
| `-l, --length <sec>` | Maximum playback length (default: 600 seconds) |
| `--no-loop` | Disable loop detection, play to max length instead |

**Loop detection:** For tracker formats that support it, fmconv automatically detects when the song loops back to an earlier position and encodes this as a VGM loop point. Use `--no-loop` to disable this and record the full length instead.

### Metadata Options (All Formats)

| Option | Description |
|--------|-------------|
| `--title <text>` | Track title (stored in VGM GD3 tag) |
| `--author <text>` | Composer name |
| `--album <text>` | Album or game name |
| `--system <text>` | Original system |
| `--date <text>` | Release date |
| `--notes <text>` | Additional notes |

### Information Options

| Option | Description |
|--------|-------------|
| `--list-banks` | Show all 79 FM instrument banks |
| `--list-vol-models` | Show all volume models |
| `--list-formats` | Show all supported formats |

## FM Instrument Banks (MIDI-Style Only)

For MIDI-style formats, the instrument bank determines how every instrument sounds.

| Use Case | Recommended Bank |
|----------|-----------------|
| DOOM, Heretic, Hexen | 16 (DMX Bobby Prince v1) |
| Descent series | 2-6 (HMI variants) |
| Duke Nukem 3D | 62 |
| Wolfenstein 3D, Commander Keen | 74 (WOPL Apogee IMF) |
| General MIDI | 58 (OP3 Fat Man 2op) |
| Miles Sound System | 0 (AIL Fat Man) |

See [docs/MIDI_BANKS.md](docs/MIDI_BANKS.md) for the full list of 79 banks and volume models, or use `--list-banks`.

## Technical Details

### FM9 Format

FM9 is an extended VGM format designed for the FM-90s hardware player:

```
[GZIP: VGM + FM9 Header + FX JSON] + [RAW: Audio data] + [RAW: Cover image]
```

- VGM data and metadata are gzip compressed for efficient storage
- Audio data (WAV/MP3) is stored uncompressed after the gzip section for streaming playback
- Cover images are scaled to 100x100 and stored as RGB565 (20KB) for direct display
- Standard VGM players ignore the FM9 extension (they stop at the `0x66` end command)
- Source format metadata identifies the original file type (MOD, S3M, RAD, MID, etc.)

See [docs/FM9_FORMAT.md](docs/FM9_FORMAT.md) for complete format specification.

### FM9 Extraction Tool

Extract and inspect FM9 files with the included Python tool:

```bash
python tools/fm9_extract.py music.fm9
```

Extracts:
- VGM data (uncompressed)
- Embedded audio (WAV/MP3)
- Cover image (converted to PNG)
- Info text file with all metadata, chip info, and duration

See [docs/FM9_EXTRACT.md](docs/FM9_EXTRACT.md) for full documentation.

### Conversion

- VGM 1.51 format with YM3812 (OPL2) or YMF262 (OPL3) chip headers
- MIDI-style formats use libADLMIDI to convert note events to OPL3 register writes
- Native OPL formats use AdPlug with embedded instruments; chip type (OPL2/Dual OPL2/OPL3) is auto-detected from register usage

## Examples

### FM9 with Embedded Audio and Cover Art

```bash
# Convert tracker with PCM drums
fmconv chiptune.rad --audio drums.wav

# Add MP3 audio to existing VGM
fmconv existing.vgz --audio vocals.mp3

# Add cover art
fmconv song.mid --image cover.jpg

# Full production with audio, effects, and cover art
fmconv song.mid --audio backing.wav --fx effects.json --image album.png
```

### Basic Conversion

```bash
# Convert to FM9 (default)
fmconv game_music.mid

# Convert to VGZ for other players
fmconv game_music.mid --vgz

# Specify output file
fmconv input.rad output.fm9
```

### MIDI Conversion

```bash
# DOOM music with correct bank
fmconv e1m1.mus --bank 16

# Descent HMP with HMI bank
fmconv level01.hmp --bank 2

# General MIDI file
fmconv classical.mid --bank 58

# Non-interactive mode (use auto-detected bank)
fmconv music.mid -y
```

### Native Format Conversion

```bash
# Reality AdLib Tracker
fmconv chiptune.rad

# Wolfenstein 3D IMF
fmconv song.imf

# DOSBox capture
fmconv recording.dro

# Multi-song file, play subsong 2
fmconv game.ksm --subsong 2

# Limit length to 3 minutes
fmconv long_song.rad --length 180

# Play full song without stopping at loop
fmconv looping.rad --no-loop --length 300
```

### Tracker Conversion (OpenMPT)

```bash
# S3M with OPL + samples - OPL plays on hardware, samples embedded as audio
fmconv hybrid_track.s3m

# Sample-only tracker - rendered to embedded audio
fmconv demo.xm
fmconv music.it
fmconv track.mod
```

### Adding Metadata

```bash
fmconv doom_e1m1.mus \
    --bank 16 \
    --title "At Doom's Gate" \
    --author "Bobby Prince" \
    --album "DOOM" \
    --system "IBM PC" \
    --date "1993"
```

### Modifying Existing VGM/FM9 Metadata

When processing VGM/VGZ/FM9 files, existing GD3 metadata is preserved. CLI options selectively override individual fields:

```bash
# Change only the title, keep existing author/album/etc
fmconv existing.vgm --title "New Title"

# Override multiple fields
fmconv existing.vgz --title "New Title" --author "New Author"

# Add metadata to VGM that has none
fmconv bare.vgm --title "Song Name" --author "Composer"
```

### Batch Conversion

```bash
# Convert all MID files in directory (Unix)
for f in *.mid; do fmconv -y "$f"; done

# Convert all MID files (Windows PowerShell)
Get-ChildItem *.mid | ForEach-Object { fmconv -y $_.Name }
```

## Distribution

### Windows

The `fmconv.exe` is a standalone executable (~1MB). Just copy it wherever you need it.

**Requirement:** The [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) must be installed. Most Windows 10/11 machines already have this - if you get a "VCRUNTIME140.dll not found" error, install the redistributable.

### Linux / macOS

Build from source (see below). The resulting binary has no special dependencies beyond standard system libraries.

## Building from Source

### Requirements

- CMake 3.16 or higher
- C++17 compatible compiler
- Git (for submodules)

### Build Steps

```bash
# Clone with submodules
git clone --recursive https://github.com/yourusername/fmconv.git
cd fmconv

# Build
cmake -B build
cmake --build build --config Release

# The executable will be in build/Release/fmconv.exe (Windows)
# or build/fmconv (Linux/macOS)
```

### Build Options

```bash
# Disable AdPlug support (MIDI-only converter)
cmake -B build -DBUILD_ADPLUG=OFF
```

## Troubleshooting

### "AdPlug could not load file"

- File may be corrupt or truncated
- Format may not be recognized (check extension)
- Try a different file of the same format

### "Wrong instruments" for MIDI files

- Try a different bank with `--bank N`
- Check bank recommendations for your game
- Use `--list-banks` to see all options

### "Loop detected too early" / "Song cuts off"

- Use `--no-loop` to disable loop detection
- Set explicit length with `--length N`

### "Unknown format"

- Unknown extensions are tried with AdPlug
- If AdPlug fails, the format isn't supported
- Check `--list-formats` for supported types

## Credits

- **[libADLMIDI](https://github.com/Wohlstand/libADLMIDI)** by Vitaly Novichkov (Wohlstand) - MIDI to OPL synthesis
- **[AdPlug](https://adplug.github.io/)** by Simon Peter et al. - Native OPL format playback
- **[libbinio](https://github.com/adplug/libbinio)** - Binary I/O library for AdPlug
- **[OpenMPT](https://openmpt.org/)** by OpenMPT Project Developers and Olivier Lapicque - Tracker format playback
- **[LAME](https://lame.sourceforge.io/)** - MP3 encoding
- **[miniz](https://github.com/richgel999/miniz)** by Rich Geldreich - Compression
- **[stb_image](https://github.com/nothings/stb)** by Sean Barrett - Image loading

## License

This tool combines multiple open-source libraries:
- libADLMIDI: LGPLv3+
- AdPlug: LGPLv2.1
- libbinio: LGPLv2.1+
- OpenMPT/libopenmpt: BSD-3-Clause
- LAME: LGPLv2
- miniz: MIT
- stb_image: Public Domain / MIT

See individual library directories for full license terms.
