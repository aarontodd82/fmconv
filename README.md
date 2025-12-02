# fmconv - FM9 Converter for FM-90s

Convert retro game music to FM9 format for playback on [FM-90s](https://github.com/aarontodd82/FM-90s), a chiptune player with hardware YMF262 (OPL2/OPL3), YM2612 (Genesis/Mega Drive), and SN76489 (PSG) chips, plus software emulation of SNES, Game Boy, and NES sound.

## Overview

fmconv converts **50+ audio formats** from classic DOS games to FM9, an extended VGM format that supports:

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

## Format Categories

### MIDI-Style Formats (libADLMIDI)

These formats contain note/program change messages and require an FM instrument bank:

| Extension | Format | Notes |
|-----------|--------|-------|
| `.mid`, `.midi`, `.smf` | Standard MIDI File | General MIDI |
| `.rmi` | RIFF MIDI | Windows MIDI format |
| `.kar` | Karaoke MIDI | MIDI with lyrics |
| `.xmi` | Extended MIDI | Miles Sound System (Origin, Westwood, etc.) |
| `.mus` | DMX Music | DOOM, Heretic, Hexen |
| `.hmp`, `.hmi` | HMI MIDI | Human Machine Interfaces (Descent, Duke3D) |
| `.klm` | Wacky Wheels Music | Apogee game |

**Bank selection matters!** These formats sound completely different with different banks because the FM instrument definitions come from the bank, not the file.

### Native OPL Formats (AdPlug)

These formats have **embedded FM instruments** - bank selection is not needed:

| Extension | Format | Games/Software |
|-----------|--------|----------------|
| `.rad` | Reality AdLib Tracker | Various shareware |
| `.a2m`, `.a2t` | Adlib Tracker 2 | Tracker music |
| `.imf`, `.wlf`, `.adlib` | id Software Music | Wolfenstein 3D, Commander Keen |
| `.dro` | DOSBox Raw OPL | DOSBox captures |
| `.cmf` | Creative Music File | Early Sound Blaster games |
| `.rol` | AdLib Visual Composer | Professional compositions |
| `.d00` | EdLib | Packed tracker format |
| `.s3m` | Scream Tracker 3 | OPL instrument modules |
| `.ksm` | Ken Silverman Music | Build engine games |
| `.laa` | LucasArts AdLib Audio | LucasArts adventures |
| `.hsc` | HSC-Tracker | Tracker music |
| `.lds` | LOUDNESS Sound System | Various games |
| `.adl` | Westwood ADL | Westwood Studios games |
| `.amd` | AMUSIC Adlib Tracker | Tracker music |
| `.bam` | Bob's Adlib Music | Various |
| `.cff` | Boomtracker 4.0 | Tracker music |
| `.dfm` | Digital-FM | Tracker music |
| `.dmo` | Twin TrackPlayer | Tracker music |
| `.dtm` | DeFy Adlib Tracker | Tracker music |
| `.got` | GOT Music | God of Thunder |
| `.hsp` | HSC Packed | Packed HSC |
| `.hsq`, `.sqx`, `.sdb`, `.agd`, `.ha2` | Herbulot AdLib System | Various games |
| `.jbm` | JBM Adlib Music | Various |
| `.mad` | Mlat Adlib Tracker | Tracker music |
| `.mdi` | AdLib MIDIPlay | AdLib MIDI variant |
| `.mkj` | MKJamz | Various |
| `.msc` | AdLib MSC | Various |
| `.mtk` | MPU-401 Trakker | Tracker music |
| `.mtr` | Master Tracker | Tracker music |
| `.pis` | Beni Tracker | Tracker music |
| `.plx` | PALLADIX Sound System | Various games |
| `.raw`, `.rac` | Raw AdLib Capture | Direct captures |
| `.rix`, `.mkf` | Softstar RIX | Chinese RPGs |
| `.sa2` | Surprise! Adlib Tracker 2 | Tracker music |
| `.sat` | Surprise! Adlib Tracker | Tracker music |
| `.sci` | Sierra SCI | Sierra games |
| `.sng` | Various | SNGPlay, Faust, etc. |
| `.sop` | Note Sequencer | sopepos software |
| `.xad`, `.bmf` | Various | FLASH, BMF, etc. |
| `.xms` | XMS-Tracker | Tracker music |
| `.xsm` | eXtra Simple Music | Tracker music |
| `.m` | Ultima 6 Music | Ultima 6 |
| `.mus`, `.mdy`, `.ims` | AdLib MIDI/IMS | Various |

### Tracker Formats (OpenMPT)

These formats are handled by OpenMPT for accurate playback. S3M can contain OPL instruments; others are sample-based:

| Extension | Format | OPL Support |
|-----------|--------|-------------|
| `.s3m` | Scream Tracker 3 | Yes (hybrid OPL+samples) |
| `.mod` | ProTracker | No (samples only) |
| `.xm` | FastTracker 2 | No (samples only) |
| `.it` | Impulse Tracker | No (samples only) |
| `.mptm` | OpenMPT Module | Yes |
| `.stm` | Scream Tracker 2 | No |
| `.669` | Composer 669 | No |
| `.mtm` | MultiTracker | No |
| `.med` | OctaMED | No |
| `.okt` | Oktalyzer | No |
| `.far` | Farandole Composer | No |
| `.mdl` | Digitrakker | No |
| `.ams` | Extreme's Tracker / Velvet Studio | No |
| `.dbm` | DigiBooster Pro | No |
| `.digi` | DigiBooster | No |
| `.dmf` | X-Tracker | No |
| `.dsm` | DSIK Format | No |
| `.umx` | Unreal Music | No |
| `.mt2` | MadTracker 2 | No |
| `.psm` | Epic Megagames MASI | No |
| `.j2b` | Jazz Jackrabbit 2 | No |
| `.mo3` | MO3 Compressed | No |

Plus 40+ additional formats. Use `--list-formats` for the complete list.

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

For MIDI-style formats, the instrument bank determines how every instrument sounds. Different banks are optimized for different games and sound systems.

### Complete Bank List (0-78)

| ID | Bank Name | Recommended For |
|----|-----------|-----------------|
| 0 | AIL (The Fat Man 2op set, default AIL) | SimCity 2000, Miles Sound System games |
| 1 | Bisqwit (selection of 4op and 2op) | General purpose |
| 2 | HMI (Descent, Asterix) | Descent, Asterix |
| 3 | HMI (Descent:: Int) | Descent International |
| 4 | HMI (Descent:: Ham) | Descent Ham version |
| 5 | HMI (Descent:: Rick) | Descent Rick version |
| 6 | HMI (Descent 2) | Descent 2 |
| 7 | HMI (Normality) | Normality |
| 8 | HMI (Shattered Steel) | Shattered Steel |
| 9 | HMI (Theme Park) | Theme Park |
| 10 | HMI (MegaPatch by LoudMouth) | General HMI games |
| 11 | HMI (Aces of the Deep) | Aces of the Deep |
| 12 | HMI (Earthsiege) | Earthsiege |
| 13 | HMI (Anvil of Dawn) | Anvil of Dawn |
| 14 | DMX (Bobby Prince v2) | DOOM variants |
| 15 | DMX (Cygnus Studios, default DMX) | DOOM, Heretic, Hexen |
| 16 | DMX (Bobby Prince v1) | **DOOM, Heretic, Hexen (recommended)** |
| 17 | AIL (Discworld, Grandest Fleet, etc.) | Discworld, Grandest Fleet |
| 18 | AIL (Warcraft 2) | Warcraft 2 |
| 19 | AIL (Syndicate) | Syndicate |
| 20 | AIL (Guilty, Orion Conspiracy, TNSFC ::4op) | Various |
| 21 | AIL (Magic Carpet 2) :NON-GM: | Magic Carpet 2 |
| 22 | AIL (Nemesis) | Nemesis |
| 23 | AIL (Jagged Alliance) :NON-GM: | Jagged Alliance |
| 24 | AIL (When Two Worlds War) :MISS-INS: | When Two Worlds War |
| 25 | AIL (Bards Tale Construction) :MISS-INS: | Bard's Tale Construction |
| 26 | AIL (Return to Zork) :NON-GM: | Return to Zork |
| 27 | AIL (Theme Hospital) | Theme Hospital |
| 28 | AIL (National Hockey League PA) | NHL PA |
| 29 | AIL (Inherit The Earth) :NON-GM: | Inherit The Earth |
| 30 | AIL (Inherit The Earth, file two) :NON-GM: | Inherit The Earth |
| 31 | AIL (Little Big Adventure) :4op: | Little Big Adventure |
| 32 | AIL (Heroes of Might and Magic II) :NON-GM: | Heroes II |
| 33 | AIL (Death Gate) | Death Gate |
| 34 | AIL (FIFA International Soccer) | FIFA |
| 35 | AIL (Starship Invasion) | Starship Invasion |
| 36 | AIL (Super Street Fighter 2 :4op:) | Super Street Fighter 2 |
| 37 | AIL (Lords of the Realm) :MISS-INS: | Lords of the Realm |
| 38 | AIL (SimFarm, SimHealth) :4op: | SimFarm, SimHealth |
| 39 | AIL (SimFarm, Settlers, Serf City) | SimFarm, Settlers |
| 40 | AIL (Caesar 2) :p4op: :MISS-INS: | Caesar 2 |
| 41 | AIL (Syndicate Wars) :NON-GM: | Syndicate Wars |
| 42 | AIL (LoudMouth by Probe Ent.) | LoudMouth games |
| 43 | AIL (Warcraft) :NON-GM: | Warcraft 1 |
| 44 | AIL (Terra Nova Strike Force Centuri) :p4op: | Terra Nova |
| 45 | AIL (System Shock) :p4op: | System Shock |
| 46 | AIL (Advanced Civilization) | Advanced Civilization |
| 47 | AIL (Battle Chess 4000) :p4op: :NON-GM: | Battle Chess 4000 |
| 48 | AIL (Ultimate Soccer Manager :p4op:) | Ultimate Soccer Manager |
| 49 | AIL (Air Bucks, Blue And The Gray, etc) :NON-GM: | Air Bucks |
| 50 | AIL (Ultima Underworld 2) :NON-GM: | Ultima Underworld 2 |
| 51 | AIL (FatMan MT32) :NON-GM: | MT-32 emulation |
| 52 | AIL (High Seas Trader) :MISS-INS: | High Seas Trader |
| 53 | AIL (Master of Magic) :4op: | Master of Magic |
| 54 | AIL (Master of Magic) :4op: orchestral drums | Master of Magic (alt) |
| 55 | SB (Action Soccer) | Action Soccer |
| 56 | SB (3d Cyberpuck :: melodic only) | 3D Cyberpuck |
| 57 | SB (Simon the Sorcerer :: melodic only) | Simon the Sorcerer |
| 58 | OP3 (The Fat Man 2op set; Win9x) | **General MIDI (recommended)** |
| 59 | OP3 (The Fat Man 4op set) | General MIDI 4-op |
| 60 | OP3 (JungleVision 2op set :: melodic only) | JungleVision |
| 61 | OP3 (Wallace 2op set, Nitemare 3D :: melodic only) | Nitemare 3D |
| 62 | TMB (Duke Nukem 3D) | **Duke Nukem 3D** |
| 63 | TMB (Shadow Warrior) | Shadow Warrior |
| 64 | DMX (Scott Host) | DMX variant |
| 65 | SB (Modded GMOPL by Wohlstand) | General purpose |
| 66 | SB (Jamie O'Connell's bank) | General purpose |
| 67 | TMB (Apogee Sound System Default bank) :broken drums: | Apogee games |
| 68 | WOPL (4op bank by James Alan Nguyen and Wohlstand) | Modern 4-op |
| 69 | TMB (Blood) | Blood |
| 70 | TMB (Rise of the Triad) | Rise of the Triad |
| 71 | TMB (Nam) | NAM |
| 72 | WOPL (DMXOPL3 bank by Sneakernets) | Modern DOOM |
| 73 | EA (Cartooners) | Cartooners |
| 74 | WOPL (Apogee IMF 90-ish) | **Wolfenstein 3D, Commander Keen** |
| 75 | AIL (The Lost Vikings) :NON-GM: | The Lost Vikings |
| 76 | DMX (Strife) | Strife |
| 77 | WOPL (MS-AdLib, Windows 3.x) | Windows 3.x |
| 78 | AIL (Monopoly Deluxe) | Monopoly Deluxe |

### Bank Tag Meanings

- **:4op:** - Uses 4-operator FM synthesis (richer sound)
- **:p4op:** - Partial 4-operator support
- **:NON-GM:** - Non-General MIDI, may have unusual instrument mappings
- **:MISS-INS:** - Some instruments may be missing

### Common Bank Recommendations

| Game Series | Recommended Bank |
|-------------|-----------------|
| DOOM, Heretic, Hexen | 16 (DMX Bobby Prince v1) |
| Descent series | 2-6 (HMI variants) |
| Duke Nukem 3D | 62 (TMB Duke Nukem 3D) |
| Wolfenstein 3D, Commander Keen | 74 (WOPL Apogee IMF) |
| General MIDI files | 58 (OP3 Fat Man 2op) |
| Miles Sound System games | 0 (AIL Fat Man) |

## Volume Models (MIDI-Style Only)

| ID | Model | Description |
|----|-------|-------------|
| 0 | AUTO | Automatically selected based on bank |
| 1 | Generic | Linear volume scaling |
| 2 | NativeOPL3 | Logarithmic (OPL3 native curve) |
| 3 | DMX | Logarithmic (DOOM-style) |
| 4 | APOGEE | Logarithmic (Apogee Sound System) |
| 5 | Win9x | Windows 9x driver style |
| 6 | DMX (Fixed AM) | DMX with fixed AM carriers |
| 7 | APOGEE (Fixed AM) | Apogee with fixed AM carriers |
| 8 | AIL | Audio Interface Library style |
| 9 | Win9x (Generic FM) | Windows with generic FM |
| 10 | HMI | HMI Sound Operating System |
| 11 | HMI_OLD | HMI (older variant) |

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

- **libADLMIDI** by Vitaly Novichkov (Wohlstand) - MIDI to OPL synthesis
- **AdPlug** by Simon Peter et al. - Native OPL format playback
- **libbinio** - Binary I/O library for AdPlug
- **OpenMPT** by OpenMPT Project Developers - Tracker format playback
- **miniz** by Rich Geldreich - Compression
- **stb_image** by Sean Barrett - Image loading
- FM instrument banks from various contributors

## License

This tool combines multiple open-source libraries:
- libADLMIDI: LGPLv2.1+
- AdPlug: LGPLv2.1
- libbinio: LGPLv2.1+
- OpenMPT/libopenmpt: BSD-3-Clause
- miniz: MIT
- stb_image: Public Domain / MIT

See individual library directories for full license terms.
