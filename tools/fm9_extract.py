#!/usr/bin/env python3
"""
Extract components from FM9 file.

Usage: python fm9_extract.py <input.fm9> [output_dir]

Extracts:
  - VGM data (uncompressed .vgm file)
  - Audio file (if present, .wav or .mp3)
  - Effects JSON (if present, .json)
  - Cover image (if present, .png)
"""

import sys
import zlib
import struct
from pathlib import Path

# FM9 constants
FM9_FLAG_HAS_AUDIO = 0x01
FM9_FLAG_HAS_FX = 0x02
FM9_FLAG_HAS_IMAGE = 0x04

FM9_IMAGE_WIDTH = 100
FM9_IMAGE_HEIGHT = 100
FM9_IMAGE_SIZE = FM9_IMAGE_WIDTH * FM9_IMAGE_HEIGHT * 2  # 20000 bytes

AUDIO_FORMAT_NONE = 0
AUDIO_FORMAT_WAV = 1
AUDIO_FORMAT_MP3 = 2

# Source format lookup table (must match source_format.h)
SOURCE_FORMATS = {
    # Pass-through
    0x01: ("VGM", "Video Game Music"),
    0x02: ("VGZ", "Video Game Music (compressed)"),
    0x03: ("FM9", "FM9"),

    # MIDI-style (0x10-0x1F)
    0x10: ("MID", "Standard MIDI"),
    0x11: ("KAR", "Karaoke MIDI"),
    0x12: ("RMI", "RIFF MIDI"),
    0x13: ("XMI", "Miles XMI"),
    0x14: ("MUS", "DMX MUS"),
    0x15: ("HMP", "HMI HMP"),
    0x16: ("HMI", "HMI"),
    0x17: ("KLM", "Wacky Wheels"),

    # AdPlug Native OPL (0x20-0x5F)
    0x20: ("RAD", "Reality AdLib Tracker"),
    0x21: ("IMF", "id Software IMF"),
    0x22: ("ADLIB", "id Software ADLIB"),
    0x23: ("DRO", "DOSBox Raw OPL"),
    0x24: ("CMF", "Creative Music File"),
    0x25: ("A2M", "Adlib Tracker 2"),
    0x26: ("A2T", "Adlib Tracker 2"),
    0x27: ("AMD", "AMUSIC"),
    0x28: ("XMS", "XMS-Tracker"),
    0x29: ("BAM", "Bob's Adlib Music"),
    0x2A: ("CFF", "Boomtracker"),
    0x2B: ("D00", "EdLib"),
    0x2C: ("DFM", "Digital-FM"),
    0x2D: ("HSC", "HSC-Tracker"),
    0x2E: ("HSP", "HSC Packed"),
    0x2F: ("KSM", "Ken Silverman Music"),
    0x30: ("MAD", "Mlat Adlib Tracker"),
    0x31: ("MKJ", "MKJamz"),
    0x32: ("DTM", "DeFy Adlib Tracker"),
    0x33: ("MTK", "MPU-401 Trakker"),
    0x34: ("MTR", "Master Tracker"),
    0x35: ("SA2", "Surprise! Adlib Tracker 2"),
    0x36: ("SAT", "Surprise! Adlib Tracker"),
    0x37: ("XAD", "XAD"),
    0x38: ("BMF", "BMF Adlib Tracker"),
    0x39: ("LDS", "LOUDNESS"),
    0x3A: ("PLX", "PALLADIX"),
    0x3B: ("XSM", "eXtra Simple Music"),
    0x3C: ("PIS", "Beni Tracker"),
    0x3D: ("MSC", "AdLib MSC"),
    0x3E: ("SNG", "SNGPlay"),
    0x3F: ("JBM", "JBM Adlib Music"),
    0x40: ("GOT", "God of Thunder"),
    0x41: ("SOP", "sopepos Sequencer"),
    0x42: ("ROL", "AdLib Visual Composer"),
    0x43: ("RAW", "Raw AdLib"),
    0x44: ("RAC", "Raw AdLib"),
    0x45: ("LAA", "LucasArts AdLib"),
    0x46: ("SCI", "Sierra SCI"),
    0x47: ("MDI", "AdLib MIDIPlay"),
    0x48: ("MDY", "AdLib MDY"),
    0x49: ("IMS", "AdLib IMS"),
    0x4A: ("ADL", "Westwood ADL"),
    0x4B: ("ADL", "Coktel Vision"),
    0x4C: ("DMO", "TwinTeam"),
    0x4D: ("RIX", "Softstar RIX"),
    0x4E: ("MKF", "Softstar RIX"),
    0x4F: ("M", "Ultima 6"),
    0x50: ("HSQ", "Herbulot AdLib"),
    0x51: ("SQX", "Herbulot AdLib"),
    0x52: ("SDB", "Herbulot AdLib"),
    0x53: ("AGD", "Herbulot AdLib"),
    0x54: ("HA2", "Herbulot AdLib"),

    # OpenMPT Tracker (0x60-0x9F)
    0x60: ("MOD", "ProTracker"),
    0x61: ("S3M", "Scream Tracker 3"),
    0x62: ("XM", "FastTracker 2"),
    0x63: ("IT", "Impulse Tracker"),
    0x64: ("MPTM", "OpenMPT"),
    0x65: ("STM", "Scream Tracker 2"),
    0x66: ("STX", "Scream Tracker Ext"),
    0x67: ("STP", "Scream Tracker Project"),
    0x68: ("669", "Composer 669"),
    0x69: ("667", "Composer 667"),
    0x6A: ("C67", "Composer 667"),
    0x6B: ("MTM", "MultiTracker"),
    0x6C: ("MED", "OctaMED"),
    0x6D: ("OKT", "Oktalyzer"),
    0x6E: ("FAR", "Farandole"),
    0x6F: ("FMT", "Farandole"),
    0x70: ("MDL", "Digitrakker"),
    0x71: ("AMS", "Velvet Studio"),
    0x72: ("DBM", "DigiBooster Pro"),
    0x73: ("DIGI", "DigiBooster"),
    0x74: ("DMF", "X-Tracker"),
    0x75: ("DSM", "DSIK"),
    0x76: ("DSYM", "DSIK Symbol"),
    0x77: ("DTM", "DeFy Adlib Tracker"),
    0x78: ("AMF", "ASYLUM"),
    0x79: ("PSM", "Epic MASI"),
    0x7A: ("MT2", "MadTracker 2"),
    0x7B: ("UMX", "Unreal Music"),
    0x7C: ("J2B", "Jazz Jackrabbit 2"),
    0x7D: ("PTM", "PolyTracker"),
    0x7E: ("PPM", "Packed PolyTracker"),
    0x7F: ("PLM", "Plastic Music"),
    0x80: ("SFX", "Startracker"),
    0x81: ("SFX2", "Startracker 2"),
    0x82: ("NST", "NoiseTracker"),
    0x83: ("WOW", "Grave Composer"),
    0x84: ("ULT", "UltraTracker"),
    0x85: ("GDM", "GEMINI"),
    0x86: ("MO3", "MO3"),
    0x87: ("OXM", "OXM"),
    0x88: ("RTM", "Real Tracker"),
    0x89: ("PT36", "ProTracker 3.6"),
    0x8A: ("M15", "15-instrument MOD"),
    0x8B: ("STK", "Soundtracker"),
    0x8C: ("ST26", "SoundTracker 2.6"),
    0x8D: ("UNIC", "UNIC Tracker"),
    0x8E: ("ICE", "ICE Tracker"),
    0x8F: ("MMCMP", "MMCMP"),
    0x90: ("XPK", "XPK"),
    0x91: ("MMS", "MMS"),
    0x92: ("CBA", "CBA"),
    0x93: ("ETX", "EMU Tracker"),
    0x94: ("FC", "Future Composer"),
    0x95: ("FC13", "Future Composer 1.3"),
    0x96: ("FC14", "Future Composer 1.4"),
    0x97: ("FST", "Future Sound Tracker"),
    0x98: ("FTM", "FamiTracker"),
    0x99: ("GMC", "Game Music Creator"),
    0x9A: ("GTK", "Graoumf Tracker"),
    0x9B: ("GT2", "Graoumf Tracker 2"),
    0x9C: ("PUMA", "PumaTracker"),
    0x9D: ("SMOD", "SMOD"),
    0x9E: ("SYMMOD", "Symbolic"),
    0x9F: ("TCB", "TCB Tracker"),
    0xA0: ("XMF", "XMF"),
}

def get_source_format_name(code):
    """Get human-readable name for source format code."""
    if code == 0:
        return None, None
    if code in SOURCE_FORMATS:
        return SOURCE_FORMATS[code]
    return "???", f"Unknown (0x{code:02x})"


def rgb565_to_rgb888(pixel):
    """Convert RGB565 (16-bit) to RGB888 (24-bit)."""
    r = ((pixel >> 11) & 0x1F) << 3
    g = ((pixel >> 5) & 0x3F) << 2
    b = (pixel & 0x1F) << 3
    # Expand to full range
    r |= r >> 5
    g |= g >> 6
    b |= b >> 5
    return r, g, b


def parse_fm9(data):
    """Parse FM9 header from decompressed data. Returns (header_dict, header_position, vgm_data)."""
    magic = b'FM90'
    pos = data.find(magic)

    if pos == -1:
        # No FM9 header - this is just a plain VGM/VGZ
        return None, -1, data

    # VGM data is everything before the FM9 header
    vgm_data = data[:pos]

    # Parse header (24 bytes)
    header_data = data[pos:pos + 24]
    if len(header_data) < 24:
        return None, pos, vgm_data

    magic_bytes, version, flags, audio_format, source_format, \
        audio_offset, audio_size, fx_offset, fx_size = struct.unpack(
            '<4sBBBBIIII', header_data
        )

    # FX data follows header in the compressed section
    fx_data = None
    if flags & FM9_FLAG_HAS_FX and fx_size > 0:
        fx_start = pos + fx_offset
        fx_data = data[fx_start:fx_start + fx_size]

    return {
        'magic': magic_bytes,
        'version': version,
        'flags': flags,
        'audio_format': audio_format,
        'source_format': source_format,
        'audio_size': audio_size,
        'fx_size': fx_size,
        'fx_data': fx_data,
    }, pos, vgm_data


def extract_fm9(fm9_path, output_dir=None):
    """Extract all components from FM9 file."""
    fm9_path = Path(fm9_path)

    if output_dir is None:
        output_dir = fm9_path.parent
    else:
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

    base_name = fm9_path.stem

    # Read entire FM9 file
    with open(fm9_path, 'rb') as f:
        file_data = f.read()

    print(f"Input: {fm9_path}")
    print(f"File size: {len(file_data)} bytes")
    print()

    # Check for gzip magic
    if file_data[:2] != b'\x1f\x8b':
        print("Error: File is not gzip compressed")
        return False

    # FM9 structure: [gzip(VGM + FM9 header + FX)] + [raw audio] + [raw image]
    decompressor = zlib.decompressobj(wbits=31)  # 31 = gzip format
    try:
        decompressed = decompressor.decompress(file_data)
        trailing_data = decompressor.unused_data
    except Exception as e:
        print(f"Error decompressing: {e}")
        return False

    gzip_end_offset = len(file_data) - len(trailing_data)

    # Parse FM9 header
    header, header_pos, vgm_data = parse_fm9(decompressed)

    extracted = []

    # Always extract VGM
    vgm_path = output_dir / f"{base_name}.vgm"
    with open(vgm_path, 'wb') as f:
        f.write(vgm_data)
    print(f"VGM:    {vgm_path} ({len(vgm_data)} bytes)")
    extracted.append(vgm_path)

    if header is None:
        print("\nNo FM9 extension found (plain VGZ file)")
        return True

    print(f"\nFM9 version: {header['version']}")
    print(f"Flags: 0x{header['flags']:02x}")

    has_audio = bool(header['flags'] & FM9_FLAG_HAS_AUDIO)
    has_fx = bool(header['flags'] & FM9_FLAG_HAS_FX)
    has_image = bool(header['flags'] & FM9_FLAG_HAS_IMAGE)

    print(f"  Has audio: {has_audio}")
    print(f"  Has FX:    {has_fx}")
    print(f"  Has image: {has_image}")

    # Display source format
    src_short, src_name = get_source_format_name(header['source_format'])
    if src_short:
        print(f"\nSource format: {src_short} ({src_name})")
    print()

    # Extract FX JSON (from compressed section)
    if has_fx and header['fx_data']:
        fx_path = output_dir / f"{base_name}_fx.json"
        with open(fx_path, 'wb') as f:
            f.write(header['fx_data'])
        print(f"FX:     {fx_path} ({len(header['fx_data'])} bytes)")
        extracted.append(fx_path)

    # Calculate offsets for raw data after gzip
    audio_offset = gzip_end_offset
    audio_size = header['audio_size']
    image_offset = audio_offset + audio_size

    # Extract audio (from raw section after gzip)
    if has_audio and audio_size > 0:
        audio_ext = '.wav' if header['audio_format'] == AUDIO_FORMAT_WAV else '.mp3'
        audio_path = output_dir / f"{base_name}_audio{audio_ext}"
        audio_data = file_data[audio_offset:audio_offset + audio_size]
        with open(audio_path, 'wb') as f:
            f.write(audio_data)
        print(f"Audio:  {audio_path} ({len(audio_data)} bytes)")
        extracted.append(audio_path)

    # Extract image (from raw section after audio)
    if has_image:
        image_data = file_data[image_offset:image_offset + FM9_IMAGE_SIZE]
        if len(image_data) == FM9_IMAGE_SIZE:
            # Convert RGB565 to RGB888
            pixels = []
            for i in range(0, FM9_IMAGE_SIZE, 2):
                pixel = struct.unpack('<H', image_data[i:i+2])[0]
                r, g, b = rgb565_to_rgb888(pixel)
                pixels.extend([r, g, b])

            image_path = output_dir / f"{base_name}_cover.png"
            save_png(image_path, FM9_IMAGE_WIDTH, FM9_IMAGE_HEIGHT, bytes(pixels))
            print(f"Image:  {image_path} ({FM9_IMAGE_SIZE} bytes RGB565 -> PNG)")
            extracted.append(image_path)
        else:
            print(f"Warning: Image data truncated ({len(image_data)} bytes, expected {FM9_IMAGE_SIZE})")

    print(f"\nExtracted {len(extracted)} file(s)")
    return True


def save_png(path, width, height, rgb_data):
    """Save RGB data as PNG file (minimal implementation)."""
    import zlib

    def png_chunk(chunk_type, data):
        chunk = chunk_type + data
        crc = zlib.crc32(chunk) & 0xffffffff
        return struct.pack('>I', len(data)) + chunk + struct.pack('>I', crc)

    # PNG signature
    signature = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    ihdr = png_chunk(b'IHDR', ihdr_data)

    # IDAT chunk (image data)
    raw_data = b''
    for y in range(height):
        raw_data += b'\x00'  # Filter type: None
        row_start = y * width * 3
        raw_data += rgb_data[row_start:row_start + width * 3]

    compressed = zlib.compress(raw_data, 9)
    idat = png_chunk(b'IDAT', compressed)

    # IEND chunk
    iend = png_chunk(b'IEND', b'')

    with open(path, 'wb') as f:
        f.write(signature + ihdr + idat + iend)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    input_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else None

    if not extract_fm9(input_path, output_dir):
        sys.exit(1)


if __name__ == '__main__':
    main()
