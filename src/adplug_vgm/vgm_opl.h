/*
 * VGM OPL - Copl implementation that captures OPL writes to VGM format
 * For use with AdPlug library
 *
 * Auto-detects chip type (OPL2, Dual OPL2, OPL3) based on register writes.
 * Buffers all writes during playback, then generates VGM with correct header.
 */

#ifndef VGM_OPL_H
#define VGM_OPL_H

#include <stdint.h>
#include <vector>
#include <string>

// AdPlug's OPL base class
#include "adplug/opl.h"

// Forward declaration
struct GD3Tag;

// Buffered OPL write command
struct OplWrite {
    uint32_t delay_samples;  // Samples to wait before this write
    uint16_t reg;            // Register (0x000-0x1FF for OPL3)
    uint8_t val;             // Value
    uint8_t chip;            // Chip number (0 or 1 for dual)
};

class CVgmOpl : public Copl
{
public:
    CVgmOpl();
    virtual ~CVgmOpl();

    // Copl interface
    void write(int reg, int val) override;
    void init() override;
    void setchip(int n) override;

    // Timing control - call after each update()
    // samples = 44100 / refresh_rate
    void advanceSamples(uint32_t samples);

    // Mark the current position as the loop point
    // Call this when the song loops back
    void markLoopPoint();

    // Set loop point at a previously recorded position
    // Use this when you detect the loop retroactively
    void setLoopPoint(size_t write_index, uint32_t sample_pos);

    // Check if loop point has been marked
    bool hasLoopPoint() const { return loop_point_marked_; }

    // Generate final VGM file after playback complete
    // Returns the VGM data
    std::vector<uint8_t> generateVgm(GD3Tag* gd3_tag = nullptr);

    // Get detected chip type (valid after playback)
    ChipType getDetectedType() const { return detected_type_; }

    // Get total samples
    uint32_t getTotalSamples() const { return total_samples_; }

    // Get current write count (for tracking loop point position)
    size_t getWriteCount() const { return writes_.size(); }

    // Get chip type as string
    const char* getChipTypeString() const;

private:
    // Buffered writes (we don't know chip type until we see all writes)
    std::vector<OplWrite> writes_;
    uint32_t pending_samples_;
    uint32_t total_samples_;

    // Detection flags
    bool used_opl3_regs_;      // Wrote to 0x100-0x1FF range
    bool used_opl3_mode_;      // Wrote 0x01 to register 0x105
    bool used_second_chip_;    // Called setchip(1)

    ChipType detected_type_;

    // Loop point tracking
    bool loop_point_marked_;
    size_t loop_write_index_;    // Index into writes_ where loop starts
    uint32_t loop_sample_pos_;   // Sample position where loop starts

    // Track register state to avoid redundant writes (optimization)
    uint8_t reg_state_[2][256];  // [chip][reg]
    bool reg_written_[2][256];

    void flushPendingSamples();
    void detectChipType();
    void writeVgmHeader(std::vector<uint8_t>& buffer);
    void writeVgmData(std::vector<uint8_t>& buffer, uint32_t& loop_byte_offset);
    void writeVgmCommand(std::vector<uint8_t>& buffer, uint8_t cmd, uint8_t reg, uint8_t val);
    void writeVgmDelay(std::vector<uint8_t>& buffer, uint32_t samples);
    void writeGD3Tag(std::vector<uint8_t>& buffer, GD3Tag* gd3_tag);
};

#endif // VGM_OPL_H
