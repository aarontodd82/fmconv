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

    magic_bytes, version, flags, audio_format, reserved, \
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
