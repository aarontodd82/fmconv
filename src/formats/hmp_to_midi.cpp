/*
 * HMP to Standard MIDI File Converter - Implementation
 *
 * Converts HMP (Human Machine Interfaces MIDI) format to Standard MIDI File (SMF) format
 * Based on Python implementation in tools/mms/parsers/hmp_parser.py
 *
 * CRITICAL NOTES:
 * 1. HMP variable-length encoding is INVERSE of MIDI!
 * 2. Miles loop markers (CC 110/111 with value >127) must be filtered
 * 3. PPQN is always 60 (hardcoded per WildMIDI spec)
 * 4. Tempo comes from BPM field in header
 */

#include "hmp_to_midi.h"
#include <cstring>
#include <stdio.h>
#include <algorithm>

// MIDI File Format helper class
class MIDIWriter
{
public:
    std::vector<uint8_t> &data;

    MIDIWriter(std::vector<uint8_t> &out) : data(out) {}

    // Write 4-byte big-endian
    void writeU32BE(uint32_t value)
    {
        data.push_back((value >> 24) & 0xFF);
        data.push_back((value >> 16) & 0xFF);
        data.push_back((value >> 8) & 0xFF);
        data.push_back(value & 0xFF);
    }

    // Write 2-byte big-endian
    void writeU16BE(uint16_t value)
    {
        data.push_back((value >> 8) & 0xFF);
        data.push_back(value & 0xFF);
    }

    // Write single byte
    void writeByte(uint8_t value)
    {
        data.push_back(value);
    }

    // Write MIDI variable-length quantity (STANDARD MIDI VARLEN!)
    void writeVarLen(uint32_t value)
    {
        /*
         * Standard MIDI varlen encoding (NOT HMP varlen!):
         * - Split into 7-bit chunks
         * - MSB set on all bytes except last
         * - Send highest bits first
         */
        uint32_t buffer = value & 0x7F;

        while ((value >>= 7) > 0)
        {
            buffer <<= 8;
            buffer |= 0x80;
            buffer += (value & 0x7F);
        }

        while (true)
        {
            writeByte(buffer & 0xFF);
            if (buffer & 0x80)
                buffer >>= 8;
            else
                break;
        }
    }

    // Write bytes
    void writeBytes(const uint8_t *bytes, size_t count)
    {
        for (size_t i = 0; i < count; i++)
            data.push_back(bytes[i]);
    }
};

// Read little-endian 32-bit
static inline uint32_t readU32LE(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Read HMP variable-length (INVERSE of MIDI varlen!)
static uint32_t readHMPVarLen(const uint8_t *data, size_t size, size_t &pos)
{
    /*
     * HMP varlen encoding (from WildMIDI):
     * - Bytes < 0x80: continuation bytes (more follow)
     * - Byte >= 0x80: terminating byte
     *
     * Accumulate: value |= (byte & 0x7F) << shift; shift += 7
     */
    if (pos >= size)
        return 0;

    uint32_t value = 0;
    uint32_t shift = 0;

    // Read continuation bytes (< 0x80)
    while (pos < size && data[pos] < 0x80)
    {
        value |= (data[pos] & 0x7F) << shift;
        shift += 7;
        pos++;
    }

    // Read terminating byte (>= 0x80)
    if (pos < size)
    {
        value |= (data[pos] & 0x7F) << shift;
        pos++;
    }

    return value;
}

// Parse single HMP event, returns bytes consumed
static size_t parseHMPEvent(const uint8_t *data, size_t size, size_t pos,
                             uint8_t status, uint32_t delta_time,
                             MIDIWriter &midi,
                             bool &should_write)
{
    uint8_t st_hi = status & 0xF0;
    size_t bytes_consumed = 0;
    should_write = true;  // Default: write this event

    // Parse event and collect data
    std::vector<uint8_t> event_data;

    switch (st_hi)
    {
    case 0x80:  // Note Off
    case 0x90:  // Note On
    case 0xA0:  // Polyphonic Aftertouch
    case 0xB0:  // Control Change
    case 0xE0:  // Pitch Bend
        // Two data bytes
        if (pos + 2 > size) return 0;
        event_data.push_back(data[pos] & 0x7F);
        event_data.push_back(data[pos + 1] & 0x7F);
        bytes_consumed = 2;
        break;

    case 0xC0:  // Program Change
    case 0xD0:  // Channel Pressure
        // One data byte
        if (pos + 1 > size) return 0;
        event_data.push_back(data[pos] & 0x7F);
        bytes_consumed = 1;
        break;

    case 0xF0:  // System / Meta events
        if (status == 0xFF)  // Meta event
        {
            if (pos + 1 > size) return 0;

            uint8_t meta_type = data[pos];

            if (meta_type == 0x2F)  // End of track
            {
                event_data.push_back(meta_type);
                event_data.push_back(0x00);  // Length = 0
                bytes_consumed = 3;          // Skip 0xFF 0x2F 0x00 in HMP
            }
            else if (meta_type == 0x51)  // Tempo
            {
                // Skip tempo events - we use file-level tempo
                should_write = false;
                bytes_consumed = 6;  // 0xFF 0x51 0x03 + 3 bytes in HMP
            }
            else
            {
                // Unknown meta - skip
                should_write = false;
                bytes_consumed = 2;
            }
        }
        else
        {
            // System exclusive - not supported in HMP, shouldn't happen
            should_write = false;
            bytes_consumed = 1;
        }
        break;

    default:
        // Unknown event
        should_write = false;
        bytes_consumed = 1;
        break;
    }

    // Write event to MIDI if we should
    if (should_write)
    {
        midi.writeVarLen(delta_time);
        midi.writeByte(status);
        midi.writeBytes(event_data.data(), event_data.size());
    }

    return bytes_consumed;
}

bool convertHMPtoMIDI(const uint8_t *hmp_data, size_t hmp_size,
                      std::vector<uint8_t> &midi_out,
                      std::string &error_msg)
{
    size_t pos = 0;

    // === PARSE HMP HEADER ===

    // Check signature: "HMIMIDIP"
    if (pos + 8 > hmp_size || memcmp(&hmp_data[pos], "HMIMIDIP", 8) != 0)
    {
        error_msg = "Not a valid HMP file (missing HMIMIDIP signature)";
        return false;
    }
    pos += 8;

    // Check for HMP v2: "013195"
    bool is_hmp2 = false;
    if (pos + 6 <= hmp_size && memcmp(&hmp_data[pos], "013195", 6) == 0)
    {
        is_hmp2 = true;
        pos += 6;
    }

    // Skip zero padding (18 for HMP2, 24 for HMP1)
    uint32_t zero_count = is_hmp2 ? 18 : 24;
    if (pos + zero_count > hmp_size)
    {
        error_msg = "Truncated HMP header";
        return false;
    }
    pos += zero_count;

    // Read header fields (all little-endian)
    if (pos + 32 > hmp_size)
    {
        error_msg = "Truncated HMP header";
        return false;
    }

    uint32_t file_length = readU32LE(&hmp_data[pos]);
    pos += 4;
    pos += 12;  // Skip unknown

    uint32_t num_chunks = readU32LE(&hmp_data[pos]);
    pos += 4;
    pos += 4;  // Skip unknown

    uint32_t bpm = readU32LE(&hmp_data[pos]);
    pos += 4;

    uint32_t song_time = readU32LE(&hmp_data[pos]);
    pos += 4;

    // Calculate tempo (microseconds per quarter note)
    uint32_t tempo = (bpm == 0) ? 500000 : (60000000 / bpm);

    // PPQN is always 60 for HMP (WildMIDI spec)
    uint16_t ppqn = 60;

    // Skip large padding (840 for HMP2, 712 for HMP1)
    uint32_t skip_bytes = is_hmp2 ? 840 : 712;
    if (pos + skip_bytes > hmp_size)
    {
        error_msg = "Truncated HMP header";
        return false;
    }
    pos += skip_bytes;

    // === WRITE MIDI HEADER ===

    MIDIWriter midi(midi_out);

    // MThd chunk
    midi.writeBytes((const uint8_t*)"MThd", 4);
    midi.writeU32BE(6);              // Header length
    midi.writeU16BE(1);              // Format 1 (multi-track)
    midi.writeU16BE(num_chunks);     // Number of tracks
    midi.writeU16BE(ppqn);           // Ticks per quarter note

    // === PARSE AND CONVERT CHUNKS ===

    for (uint32_t track_num = 0; track_num < num_chunks; track_num++)
    {
        // Check for chunk header
        if (pos + 12 > hmp_size)
            break;

        // Read chunk header
        uint32_t chunk_num = readU32LE(&hmp_data[pos]);
        uint32_t chunk_length = readU32LE(&hmp_data[pos + 4]);
        uint32_t track_id = readU32LE(&hmp_data[pos + 8]);

        // Validate chunk
        if (pos + chunk_length > hmp_size)
        {
            error_msg = "Chunk extends beyond file";
            return false;
        }

        // Start of track data
        size_t chunk_pos = pos + 12;
        size_t chunk_end = pos + chunk_length;

        // Start building MIDI track
        std::vector<uint8_t> track_data;
        MIDIWriter track_midi(track_data);

        // Add tempo to first track
        if (track_num == 0)
        {
            track_midi.writeVarLen(0);  // Delta time = 0
            track_midi.writeByte(0xFF);  // Meta event
            track_midi.writeByte(0x51);  // Tempo
            track_midi.writeByte(0x03);  // Length = 3
            track_midi.writeByte((tempo >> 16) & 0xFF);
            track_midi.writeByte((tempo >> 8) & 0xFF);
            track_midi.writeByte(tempo & 0xFF);
        }

        // Parse events
        uint32_t absolute_time = 0;
        uint32_t prev_time = 0;
        uint8_t running_status = 0;
        bool has_end_marker = false;

        // Read initial delta
        uint32_t delta = readHMPVarLen(hmp_data, hmp_size, chunk_pos);
        absolute_time += delta;

        while (chunk_pos < chunk_end && chunk_pos < hmp_size)
        {
            // Get status byte
            uint8_t status;
            if (hmp_data[chunk_pos] >= 0x80)
            {
                status = hmp_data[chunk_pos];
                running_status = status;
                chunk_pos++;
            }
            else if (running_status != 0)
            {
                status = running_status;
            }
            else
            {
                break;  // Invalid - no status
            }

            // Filter Miles loop markers (CC 110/111 with value > 127)
            if ((status & 0xF0) == 0xB0)  // Control Change
            {
                if (chunk_pos + 1 < hmp_size)
                {
                    uint8_t cc_num = hmp_data[chunk_pos];
                    uint8_t cc_val = hmp_data[chunk_pos + 1];

                    if ((cc_num == 110 || cc_num == 111) && cc_val > 0x7F)
                    {
                        // Skip loop marker
                        chunk_pos += 2;
                        if (chunk_pos < chunk_end)
                        {
                            delta = readHMPVarLen(hmp_data, hmp_size, chunk_pos);
                            absolute_time += delta;
                        }
                        continue;
                    }
                }
            }

            // Parse event
            uint32_t delta_time = absolute_time - prev_time;
            bool should_write = false;
            size_t bytes_consumed = parseHMPEvent(hmp_data, hmp_size, chunk_pos,
                                                   status, delta_time, track_midi,
                                                   should_write);

            if (bytes_consumed == 0)
                break;  // Parse error

            chunk_pos += bytes_consumed;

            // Only update prev_time if we actually wrote the event
            if (should_write)
                prev_time = absolute_time;

            // Check for end of track
            if (status == 0xFF && chunk_pos > bytes_consumed)
            {
                uint8_t meta_type = hmp_data[chunk_pos - bytes_consumed];
                if (meta_type == 0x2F)
                {
                    has_end_marker = true;
                    break;
                }
            }

            // Read next delta
            if (chunk_pos < chunk_end)
            {
                delta = readHMPVarLen(hmp_data, hmp_size, chunk_pos);
                absolute_time += delta;
            }
        }

        // Add end of track if not present
        if (!has_end_marker)
        {
            track_midi.writeVarLen(0);
            track_midi.writeByte(0xFF);
            track_midi.writeByte(0x2F);
            track_midi.writeByte(0x00);
        }

        // Write MTrk chunk
        midi.writeBytes((const uint8_t*)"MTrk", 4);
        midi.writeU32BE(track_data.size());
        midi.writeBytes(track_data.data(), track_data.size());

        // Move to next chunk
        pos += chunk_length;
    }

    return true;
}

bool loadHMPasMIDI(const char *filepath,
                   std::vector<uint8_t> &midi_out,
                   std::string &error_msg)
{
    // Read file
    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        error_msg = "Failed to open file";
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> hmp_data(filesize);
    size_t read_bytes = fread(hmp_data.data(), 1, filesize, f);
    fclose(f);

    if (read_bytes != filesize)
    {
        error_msg = "Failed to read file";
        return false;
    }

    // Convert
    return convertHMPtoMIDI(hmp_data.data(), filesize, midi_out, error_msg);
}
