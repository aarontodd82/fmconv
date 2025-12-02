# Supported Formats

fmconv supports 100+ retro music formats through three conversion engines.

## MIDI-Style Formats (libADLMIDI)

These formats contain note/program change messages and require an FM instrument bank.

| Extension | Format |
|-----------|--------|
| `.mid`, `.midi`, `.smf` | Standard MIDI File |
| `.rmi` | RIFF MIDI |
| `.kar` | Karaoke MIDI |
| `.xmi` | Extended MIDI (Miles Sound System) |
| `.mus` | DMX Music (DOOM, Heretic, Hexen) |
| `.hmp`, `.hmi` | HMI MIDI (Descent, Duke3D) |
| `.klm` | Wacky Wheels Music |

## Native OPL Formats (AdPlug)

These formats have **embedded FM instruments** - bank selection is not needed.

| Extension | Format |
|-----------|--------|
| `.rad` | Reality AdLib Tracker |
| `.a2m`, `.a2t` | Adlib Tracker 2 |
| `.imf`, `.wlf`, `.adlib` | id Software Music |
| `.dro` | DOSBox Raw OPL |
| `.cmf` | Creative Music File |
| `.rol` | AdLib Visual Composer |
| `.d00` | EdLib |
| `.s3m` | Scream Tracker 3 (OPL instruments) |
| `.ksm` | Ken Silverman Music |
| `.laa` | LucasArts AdLib Audio |
| `.hsc` | HSC-Tracker |
| `.lds` | LOUDNESS Sound System |
| `.adl` | Westwood ADL |
| `.amd` | AMUSIC Adlib Tracker |
| `.bam` | Bob's Adlib Music |
| `.cff` | Boomtracker 4.0 |
| `.dfm` | Digital-FM |
| `.dmo` | Twin TrackPlayer |
| `.dtm` | DeFy Adlib Tracker |
| `.got` | GOT Music |
| `.hsp` | HSC Packed |
| `.hsq`, `.sqx`, `.sdb`, `.agd`, `.ha2` | Herbulot AdLib System |
| `.jbm` | JBM Adlib Music |
| `.mad` | Mlat Adlib Tracker |
| `.mdi` | AdLib MIDIPlay |
| `.mkj` | MKJamz |
| `.msc` | AdLib MSC |
| `.mtk` | MPU-401 Trakker |
| `.mtr` | Master Tracker |
| `.pis` | Beni Tracker |
| `.plx` | PALLADIX Sound System |
| `.raw`, `.rac` | Raw AdLib Capture |
| `.rix`, `.mkf` | Softstar RIX |
| `.sa2` | Surprise! Adlib Tracker 2 |
| `.sat` | Surprise! Adlib Tracker |
| `.sci` | Sierra SCI |
| `.sng` | Various (SNGPlay, Faust, etc.) |
| `.sop` | Note Sequencer |
| `.xad`, `.bmf` | Various (FLASH, BMF, etc.) |
| `.xms` | XMS-Tracker |
| `.xsm` | eXtra Simple Music |
| `.m` | Ultima 6 Music |
| `.mus`, `.mdy`, `.ims` | AdLib MIDI/IMS |

## Tracker Formats (OpenMPT)

S3M can contain OPL instruments; others are sample-based and rendered to embedded audio.

| Extension | Format | OPL Support |
|-----------|--------|-------------|
| `.s3m` | Scream Tracker 3 | Yes (hybrid OPL+samples) |
| `.mod` | ProTracker | No |
| `.xm` | FastTracker 2 | No |
| `.it` | Impulse Tracker | No |
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

Plus 40+ additional tracker formats supported by OpenMPT.
