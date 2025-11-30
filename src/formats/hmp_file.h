/*
 * HMP File Parser
 * Human Machine Interfaces MIDI Format
 *
 * Supports HMP v1 (Descent) and HMP v2 (013195) formats
 * Based on WildMIDI f_hmp.c and foo_midi implementations (BSD licensed)
 *
 * Format Details:
 * - Used in: Descent, Descent 2, Duke Nukem 3D, and other HMI games
 * - MIDI-like format with custom variable-length encoding
 * - Chunk-based structure (each chunk = MIDI track)
 * - Hardcoded PPQN of 60 (per WildMIDI spec)
 */

#ifndef HMP_FILE_H
#define HMP_FILE_H

#include <stdint.h>
#include <vector>
#include <string>

// Forward declaration for libADLMIDI
namespace MidiSequencer {
    struct MidiTrackRow;
}

struct HMPFileInfo
{
    bool is_hmp2;              // true = HMP v2 (013195), false = HMP v1
    uint32_t file_length;      // File size from header
    uint32_t num_chunks;       // Number of track chunks
    uint32_t bpm;              // Beats per minute
    uint32_t song_time;        // Song duration in seconds
    uint32_t tempo;            // Calculated tempo (microseconds per quarter note)
    uint16_t ppqn;             // Ticks per quarter note (always 60)
};

struct HMPChunkHeader
{
    uint32_t chunk_num;        // Chunk number/ID
    uint32_t chunk_length;     // Total chunk size including header
    uint32_t track_id;         // Track identifier
};

class HMPFile
{
public:
    HMPFile();
    ~HMPFile();

    /**
     * Load and parse HMP file
     * Returns true on success, false on error
     */
    bool load(const char *filepath);

    /**
     * Check if data is a valid HMP file
     */
    static bool isHMP(const uint8_t *data, size_t size);

    /**
     * Get file information
     */
    const HMPFileInfo& getInfo() const { return info_; }

    /**
     * Get number of tracks
     */
    size_t getTrackCount() const { return tracks_.size(); }

    /**
     * Get track data (for libADLMIDI integration)
     * Returns pointer to track events or nullptr if invalid
     */
    const std::vector<MidiSequencer::MidiTrackRow>* getTrack(size_t index) const;

    /**
     * Get last error message
     */
    const std::string& getError() const { return error_; }

private:
    HMPFileInfo info_;
    std::vector<std::vector<MidiSequencer::MidiTrackRow>> tracks_;
    std::string error_;

    // Parsing helpers
    bool parseHeader(const uint8_t *data, size_t size, size_t &pos);
    bool parseChunk(const uint8_t *data, size_t size, size_t &pos);

    /**
     * Read HMP variable-length delta time
     *
     * CRITICAL: HMP uses DIFFERENT varlen encoding than MIDI!
     * - Bytes < 0x80: continuation bytes (more follow)
     * - Byte >= 0x80: terminating byte (use bits 0-6)
     *
     * Accumulates: value |= (byte & 0x7F) << shift; shift += 7
     */
    uint32_t readVarLen(const uint8_t *data, size_t size, size_t &pos);

    /**
     * Parse MIDI event from HMP chunk
     * Returns bytes consumed (0 = error)
     */
    size_t parseEvent(const uint8_t *data, size_t size, size_t pos,
                      uint8_t status, uint32_t delta_time,
                      std::vector<MidiSequencer::MidiTrackRow> &track);

    // Inline helpers
    inline uint32_t readU32LE(const uint8_t *data) const {
        return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    }
};

#endif // HMP_FILE_H
