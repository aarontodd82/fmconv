/*
 * VGM Chip - OPL3 chip emulator that outputs VGM format + PCM audio
 * Part of fmconv converter
 *
 * This chip class intercepts OPL register writes and:
 * 1. Encodes them into VGM file format
 * 2. Forwards them to a real OPL3 emulator for PCM audio generation
 */

#ifndef VGM_CHIP_H
#define VGM_CHIP_H

#include <stdint.h>
#include <vector>
#include <string>
#include "../../libADLMIDI/src/chips/opl_chip_base.h"
#include "../../libADLMIDI/src/chips/dosbox_opl3.h"

// Forward declaration
struct GD3Tag;

class VGMOPL3 final : public OPLChipBaseT<VGMOPL3>
{
public:
    VGMOPL3(std::vector<uint8_t> &vgm_buffer, GD3Tag *gd3_tag = nullptr);
    ~VGMOPL3() override;

    // OPLChipBase interface
    bool canRunAtPcmRate() const override;
    void writeReg(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override;
    void nativePostGenerate() override;
    void nativeGenerate(int16_t *frame) override;
    const char* emulatorName() override;
    ChipType chipType() override;
    bool hasFullPanning() override;

    // VGM-specific methods
    void finalize();
    uint32_t getTotalSamples() const { return total_samples_; }
    void accumulateDelay(uint32_t samples);

    // PCM audio methods
    bool savePCMtoWAV(const char* filepath);
    const std::vector<int16_t>& getPCMBuffer() const { return pcm_buffer_; }

private:
    std::vector<uint8_t> &vgm_buffer_;
    GD3Tag *gd3_tag_;
    uint32_t total_samples_;
    uint32_t pending_samples_;
    uint8_t reg_state_[512];  // Track register state to avoid redundant writes

    // Real OPL3 emulator for PCM generation
    DosBoxOPL3 *real_chip_;
    std::vector<int16_t> pcm_buffer_;  // Stereo PCM: L, R, L, R, ...

    void initializeHeader();
    void initializeOPL3();
    void updateHeader();
    void flushDelay();
    void writeGD3Tag();
};

#endif // VGM_CHIP_H
