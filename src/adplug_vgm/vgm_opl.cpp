/*
 * VGM OPL - Copl implementation that captures OPL writes to VGM format
 * Implementation with auto-detection of chip type
 */

#include "vgm_opl.h"
#include "../vgm_writer/gd3_tag.h"
#include <cstring>
#include <cstdio>

// VGM commands
#define CMD_OPL2        0x5A  // YM3812 write
#define CMD_OPL2_2ND    0xAA  // YM3812 second chip write
#define CMD_OPL3_PORT0  0x5E  // YMF262 port 0 write
#define CMD_OPL3_PORT1  0x5F  // YMF262 port 1 write
#define CMD_WAIT        0x61  // Wait N samples
#define CMD_WAIT_735    0x62  // Wait 735 samples (1/60 sec)
#define CMD_WAIT_882    0x63  // Wait 882 samples (1/50 sec)
#define CMD_END         0x66  // End of data
#define CMD_WAIT_N      0x70  // Wait 1-16 samples (0x70-0x7F)

// VGM header offsets
#define VGM_OFF_EOF         0x04
#define VGM_OFF_VERSION     0x08
#define VGM_OFF_GD3         0x14
#define VGM_OFF_SAMPLES     0x18
#define VGM_OFF_LOOP_OFFSET 0x1C
#define VGM_OFF_LOOP_SAMPLES 0x20
#define VGM_OFF_DATA        0x34
#define VGM_OFF_YM3812      0x50
#define VGM_OFF_YMF262      0x5C

// Chip clocks
#define CLOCK_YM3812    3579545
#define CLOCK_YMF262    14318180
#define VGM_DUAL_BIT    0x40000000

CVgmOpl::CVgmOpl()
    : Copl()
    , pending_samples_(0)
    , total_samples_(0)
    , used_opl3_regs_(false)
    , used_opl3_mode_(false)
    , used_second_chip_(false)
    , detected_type_(TYPE_OPL2)
    , loop_point_marked_(false)
    , loop_write_index_(0)
    , loop_sample_pos_(0)
{
    std::memset(reg_state_, 0, sizeof(reg_state_));
    std::memset(reg_written_, 0, sizeof(reg_written_));

    // Reserve space for typical song (~100K writes)
    writes_.reserve(100000);
}

CVgmOpl::~CVgmOpl()
{
}

void CVgmOpl::init()
{
    // Reset register state tracking
    std::memset(reg_state_, 0, sizeof(reg_state_));
    std::memset(reg_written_, 0, sizeof(reg_written_));

    // Note: We don't clear writes_ here because init() may be called
    // multiple times during playback (e.g., at song start)
}

void CVgmOpl::setchip(int n)
{
    Copl::setchip(n);

    // Track if second chip is ever used
    if (n == 1)
    {
        used_second_chip_ = true;
    }
}

void CVgmOpl::write(int reg, int val)
{
    // Detect OPL3 usage
    if (reg >= 0x100)
    {
        used_opl3_regs_ = true;

        // Check for OPL3 mode enable (register 0x105, value with bit 0 set)
        if (reg == 0x105 && (val & 0x01))
        {
            used_opl3_mode_ = true;
        }
    }

    // Calculate register index for state tracking
    int chip = currChip;
    int reg_low = reg & 0xFF;

    // For OPL3 registers 0x100+, use chip 1 slot for state tracking
    if (reg >= 0x100)
    {
        chip = 1;
    }

    // Skip redundant writes (optimization)
    // But always allow key-on/off and volume changes
    bool is_key_or_volume = (reg_low >= 0xA0 && reg_low <= 0xBF) ||
                            (reg_low >= 0x40 && reg_low <= 0x55);

    if (reg_written_[chip][reg_low] &&
        reg_state_[chip][reg_low] == (uint8_t)val &&
        !is_key_or_volume)
    {
        return;
    }

    reg_state_[chip][reg_low] = (uint8_t)val;
    reg_written_[chip][reg_low] = true;

    // Buffer this write
    OplWrite w;
    w.delay_samples = pending_samples_;
    w.reg = (uint16_t)reg;
    w.val = (uint8_t)val;
    w.chip = (uint8_t)currChip;

    writes_.push_back(w);

    // Accumulate total samples and reset pending
    total_samples_ += pending_samples_;
    pending_samples_ = 0;
}

void CVgmOpl::advanceSamples(uint32_t samples)
{
    pending_samples_ += samples;
}

void CVgmOpl::markLoopPoint()
{
    if (loop_point_marked_)
        return;  // Only mark once

    loop_point_marked_ = true;
    loop_write_index_ = writes_.size();  // Next write will be first in loop
    loop_sample_pos_ = total_samples_ + pending_samples_;  // Current sample position

    printf("Loop point marked at sample %u (write index %zu)\n",
           loop_sample_pos_, loop_write_index_);
}

void CVgmOpl::setLoopPoint(size_t write_index, uint32_t sample_pos)
{
    if (loop_point_marked_)
        return;  // Only mark once

    loop_point_marked_ = true;
    loop_write_index_ = write_index;
    loop_sample_pos_ = sample_pos;

    printf("Loop point set at sample %u (write index %zu)\n",
           loop_sample_pos_, loop_write_index_);
}

void CVgmOpl::detectChipType()
{
    // Priority: OPL3 > Dual OPL2 > OPL2

    if (used_opl3_regs_ || used_opl3_mode_)
    {
        // Any use of 0x100+ registers or OPL3 mode enable = OPL3
        detected_type_ = TYPE_OPL3;
    }
    else if (used_second_chip_)
    {
        // Used setchip(1) but no OPL3 registers = Dual OPL2
        detected_type_ = TYPE_DUAL_OPL2;
    }
    else
    {
        // Single chip, no OPL3 features = OPL2
        detected_type_ = TYPE_OPL2;
    }
}

const char* CVgmOpl::getChipTypeString() const
{
    switch (detected_type_)
    {
    case TYPE_OPL3:      return "OPL3 (YMF262)";
    case TYPE_DUAL_OPL2: return "Dual OPL2 (2x YM3812)";
    case TYPE_OPL2:
    default:             return "OPL2 (YM3812)";
    }
}

std::vector<uint8_t> CVgmOpl::generateVgm(GD3Tag* gd3_tag)
{
    // Add any remaining pending samples to total
    total_samples_ += pending_samples_;
    pending_samples_ = 0;

    // Detect chip type based on what was written
    detectChipType();

    // Generate VGM
    std::vector<uint8_t> vgm;
    vgm.reserve(writes_.size() * 4 + 256);  // Rough estimate

    writeVgmHeader(vgm);

    // Write data and get loop byte offset
    uint32_t loop_byte_offset = 0;
    writeVgmData(vgm, loop_byte_offset);

    // End marker
    vgm.push_back(CMD_END);

    // GD3 tag
    if (gd3_tag)
    {
        // Update GD3 offset in header
        auto* u32 = reinterpret_cast<uint32_t*>(vgm.data());
        u32[VGM_OFF_GD3 / 4] = vgm.size() - VGM_OFF_GD3;

        writeGD3Tag(vgm, gd3_tag);
    }

    // Update header with final values
    auto* u32 = reinterpret_cast<uint32_t*>(vgm.data());
    u32[VGM_OFF_EOF / 4] = vgm.size() - VGM_OFF_EOF;
    u32[VGM_OFF_SAMPLES / 4] = total_samples_;

    // Write loop info if we have a loop point
    // Loop offset is relative to offset 0x1C, so: actual_position - 0x1C
    // Loop samples = total_samples - loop_sample_pos (samples in the looped section)
    if (loop_point_marked_ && loop_byte_offset > 0)
    {
        u32[VGM_OFF_LOOP_OFFSET / 4] = loop_byte_offset - VGM_OFF_LOOP_OFFSET;
        u32[VGM_OFF_LOOP_SAMPLES / 4] = total_samples_ - loop_sample_pos_;

        printf("VGM loop: offset=0x%X (relative: 0x%X), loop_samples=%u\n",
               loop_byte_offset, loop_byte_offset - VGM_OFF_LOOP_OFFSET,
               total_samples_ - loop_sample_pos_);
    }

    return vgm;
}

void CVgmOpl::writeVgmHeader(std::vector<uint8_t>& buffer)
{
    // VGM header is 0x100 bytes for version 1.51+
    buffer.resize(0x100, 0);

    auto* u32 = reinterpret_cast<uint32_t*>(buffer.data());

    // Magic: "Vgm "
    u32[0x00 / 4] = 0x206d6756;

    // Version 1.51
    u32[VGM_OFF_VERSION / 4] = 0x00000151;

    // Data offset (relative to 0x34)
    u32[VGM_OFF_DATA / 4] = 0x100 - VGM_OFF_DATA;

    // Set chip clock based on detected type
    switch (detected_type_)
    {
    case TYPE_OPL2:
        u32[VGM_OFF_YM3812 / 4] = CLOCK_YM3812;
        break;

    case TYPE_DUAL_OPL2:
        u32[VGM_OFF_YM3812 / 4] = CLOCK_YM3812 | VGM_DUAL_BIT;
        break;

    case TYPE_OPL3:
        u32[VGM_OFF_YMF262 / 4] = CLOCK_YMF262;
        break;
    }
}

void CVgmOpl::writeVgmData(std::vector<uint8_t>& buffer, uint32_t& loop_byte_offset)
{
    loop_byte_offset = 0;

    for (size_t i = 0; i < writes_.size(); i++)
    {
        const auto& w = writes_[i];

        // Mark loop byte offset when we reach the loop write index
        if (loop_point_marked_ && i == loop_write_index_)
        {
            loop_byte_offset = buffer.size();
        }

        // Write delay before this command
        if (w.delay_samples > 0)
        {
            writeVgmDelay(buffer, w.delay_samples);
        }

        // Write the register command
        switch (detected_type_)
        {
        case TYPE_OPL2:
            writeVgmCommand(buffer, CMD_OPL2, w.reg & 0xFF, w.val);
            break;

        case TYPE_DUAL_OPL2:
            if (w.chip == 0)
                writeVgmCommand(buffer, CMD_OPL2, w.reg & 0xFF, w.val);
            else
                writeVgmCommand(buffer, CMD_OPL2_2ND, w.reg & 0xFF, w.val);
            break;

        case TYPE_OPL3:
            if (w.reg >= 0x100)
                writeVgmCommand(buffer, CMD_OPL3_PORT1, w.reg & 0xFF, w.val);
            else
                writeVgmCommand(buffer, CMD_OPL3_PORT0, w.reg & 0xFF, w.val);
            break;
        }
    }
}

void CVgmOpl::writeVgmCommand(std::vector<uint8_t>& buffer, uint8_t cmd, uint8_t reg, uint8_t val)
{
    buffer.push_back(cmd);
    buffer.push_back(reg);
    buffer.push_back(val);
}

void CVgmOpl::writeVgmDelay(std::vector<uint8_t>& buffer, uint32_t samples)
{
    while (samples > 0)
    {
        if (samples == 735)
        {
            buffer.push_back(CMD_WAIT_735);
            samples = 0;
        }
        else if (samples == 882)
        {
            buffer.push_back(CMD_WAIT_882);
            samples = 0;
        }
        else if (samples <= 16)
        {
            buffer.push_back(CMD_WAIT_N + (samples - 1));
            samples = 0;
        }
        else
        {
            uint16_t wait = (samples > 65535) ? 65535 : (uint16_t)samples;
            buffer.push_back(CMD_WAIT);
            buffer.push_back(wait & 0xFF);
            buffer.push_back(wait >> 8);
            samples -= wait;
        }
    }
}

void CVgmOpl::writeGD3Tag(std::vector<uint8_t>& buffer, GD3Tag* gd3_tag)
{
    if (!gd3_tag)
        return;

    std::vector<uint8_t> gd3_data = gd3_tag->serialize();
    buffer.insert(buffer.end(), gd3_data.begin(), gd3_data.end());
}
