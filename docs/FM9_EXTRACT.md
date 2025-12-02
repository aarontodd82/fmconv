# FM9 Extraction Tool

`fm9_extract.py` is a Python utility for extracting and inspecting FM9 files.

## Usage

```bash
python tools/fm9_extract.py <input.fm9> [output_dir]
```

## Extracted Files

The tool extracts all components from an FM9 file:

| File | Description |
|------|-------------|
| `<name>.vgm` | Uncompressed VGM data |
| `<name>_audio.wav` or `<name>_audio.mp3` | Embedded audio (if present) |
| `<name>_fx.json` | Effects automation (if present) |
| `<name>_cover.png` | Cover image converted to PNG (if present) |
| `<name>_info.txt` | Metadata and file information |

## Info File Contents

The `_info.txt` file contains comprehensive metadata extracted from the FM9:

```
============================================================
FM9 File Information
============================================================

Source Format: S3M (Scream Tracker 3)

----------------------------------------
Metadata (GD3 Tag)
----------------------------------------
Title:       Song Name
Album/Game:  Game Title
System:      OPL3
Author:      Composer Name
Date:        1995
Converted:   fmconv (Schism Tracker)
Notes:       Additional notes here

----------------------------------------
VGM Information
----------------------------------------
VGM Version: 1.51
Duration:    4:13.44 (11176704 samples)
Loop:        Yes (starts at 0:32.50)

----------------------------------------
Sound Chips
----------------------------------------
YMF262 (OPL3): 14.318 MHz
YM3812 (OPL2): 3.579 MHz x2 (dual)

----------------------------------------
FM9 Container
----------------------------------------
Version:     1
Audio:       Yes (WAV, 44706860 bytes)
Effects:     No
Cover Image: Yes (100x100 RGB565)

============================================================
```

### Source Format

Shows what format the file was originally converted from (MOD, S3M, RAD, MID, etc.).

### GD3 Metadata

Standard VGM metadata fields:
- **Title**: Track name (English and Japanese if different)
- **Album/Game**: Game or album name
- **System**: Original hardware system
- **Author**: Composer name
- **Date**: Release date
- **Converted**: Tool used for conversion
- **Notes**: Additional information

### VGM Information

- **Version**: VGM format version (typically 1.51)
- **Duration**: Total playback time in mm:ss and samples
- **Loop**: Whether the track loops, and loop start time

### Sound Chips

Lists all sound chips used in the VGM with:
- Chip name and type (e.g., YMF262 is OPL3)
- Clock frequency
- Dual chip indicator if two chips of same type

Common chips:
| Chip | Type | Description |
|------|------|-------------|
| YM3812 | OPL2 | AdLib, Sound Blaster |
| YMF262 | OPL3 | Sound Blaster Pro 2, AWE32 |
| YM2413 | OPLL | MSX, Sega Master System |
| YM2612 | OPN2 | Sega Genesis/Mega Drive |
| YM2151 | OPM | Arcade, Sharp X68000 |
| SN76489 | PSG | Sega Master System, Genesis |

### FM9 Container

- **Version**: FM9 format version
- **Audio**: Whether embedded audio is present, format and size
- **Effects**: Whether effects automation JSON is present
- **Cover Image**: Whether cover art is embedded

## Examples

### Basic Extraction

```bash
# Extract to same directory as input
python tools/fm9_extract.py music.fm9

# Extract to specific directory
python tools/fm9_extract.py music.fm9 ./extracted/
```

### Sample Output

```
Input: C:\music\track.fm9
File size: 44747309 bytes

VGM:    C:\music\track.vgm (392879 bytes)

FM9 version: 1
Flags: 0x05
  Has audio: True
  Has FX:    False
  Has image: True

Source format: S3M (Scream Tracker 3)

Audio:  C:\music\track_audio.wav (44706860 bytes)
Image:  C:\music\track_cover.png (20000 bytes RGB565 -> PNG)
Info:   C:\music\track_info.txt

Extracted 4 file(s)
```

## Requirements

- Python 3.6+
- No external dependencies (uses only standard library)

## Notes

- The tool handles both FM9 files and plain VGZ files
- Cover images are converted from RGB565 to PNG for compatibility
- Info file is always generated when metadata is available
