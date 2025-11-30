/*
 * VGM Chip - OPL3 chip emulator that outputs VGM format
 * Implementation
 */

#include "vgm_chip.h"
#include "gd3_tag.h"
#include <cstring>
#include <cassert>
#include <cstdio>

VGMOPL3::VGMOPL3(std::vector<uint8_t> &vgm_buffer, GD3Tag *gd3_tag)
    : OPLChipBaseT<VGMOPL3>()
    , vgm_buffer_(vgm_buffer)
    , gd3_tag_(gd3_tag)
    , total_samples_(0)
    , pending_samples_(0)
    , real_chip_(nullptr)
{
    std::memset(reg_state_, 0, sizeof(reg_state_));

    // Create real OPL3 emulator for PCM generation
    real_chip_ = new DosBoxOPL3();
    real_chip_->setRate(44100);

    // Reserve space for ~5 minutes of stereo PCM (44100 Hz * 2 channels * 300 sec)
    pcm_buffer_.reserve(44100 * 2 * 300);

    initializeHeader();
    initializeOPL3();
}

VGMOPL3::~VGMOPL3()
{
    delete real_chip_;
}

bool VGMOPL3::canRunAtPcmRate() const
{
    return true;
}

const char* VGMOPL3::emulatorName()
{
    return "VGM Writer";
}

OPLChipBase::ChipType VGMOPL3::chipType()
{
    return CHIPTYPE_OPL3;
}

bool VGMOPL3::hasFullPanning()
{
    return false;
}

void VGMOPL3::initializeHeader()
{
    vgm_buffer_.clear();
    vgm_buffer_.resize(128, 0);  // VGM header is 128 bytes

    auto *u32 = reinterpret_cast<uint32_t*>(vgm_buffer_.data());

    // Magic: "Vgm " (little-endian: 0x206d6756)
    u32[0x00 / 4] = 0x206d6756;

    // Version 1.51 (required for OPL3 support)
    u32[0x08 / 4] = 0x00000151;

    // VGM data offset (data starts at 0x80, offset relative to 0x34)
    u32[0x34 / 4] = 0x00000080 - 0x34;  // = 0x4C

    // YMF262 (OPL3) clock: 14.318180 MHz
    u32[0x5C / 4] = 14318180;
}

void VGMOPL3::initializeOPL3()
{
    // Standard OPL3 initialization sequence
    writeReg(0x004, 96);   // Timer mask
    writeReg(0x004, 128);  // IRQ reset
    writeReg(0x105, 0x0);  // OPL3 mode disable
    writeReg(0x105, 0x1);  // OPL3 mode enable
    writeReg(0x105, 0x0);  // OPL3 mode disable again (reset)
    writeReg(0x001, 32);   // Waveform select enable
    writeReg(0x105, 0x1);  // OPL3 mode enable (final)
}

void VGMOPL3::flushDelay()
{
    if (pending_samples_ == 0)
        return;

    total_samples_ += pending_samples_;

    while (pending_samples_ > 0)
    {
        uint16_t samples = (pending_samples_ > 65535) ? 65535 : static_cast<uint16_t>(pending_samples_);

        // VGM wait command: 0x61 [low byte] [high byte]
        vgm_buffer_.push_back(0x61);
        vgm_buffer_.push_back(samples & 0xFF);
        vgm_buffer_.push_back(samples >> 8);

        pending_samples_ -= samples;
    }
}

void VGMOPL3::writeReg(uint16_t addr, uint8_t data)
{
    // Forward to real OPL3 chip for PCM generation
    if (real_chip_)
        real_chip_->writeReg(addr, data);

    // Check if this is a redundant write (optimization for VGM size)
    if (reg_state_[addr & 0x1FF] == data)
        return;

    reg_state_[addr & 0x1FF] = data;

    // Flush any pending delays before writing register
    flushDelay();

    // Encode VGM command
    // 0x5E = YMF262 port 0 write (addresses 0x000-0x0FF)
    // 0x5F = YMF262 port 1 write (addresses 0x100-0x1FF)
    uint8_t opcode = (addr & 0x100) ? 0x5F : 0x5E;

    vgm_buffer_.push_back(opcode);
    vgm_buffer_.push_back(addr & 0xFF);
    vgm_buffer_.push_back(data);
}

void VGMOPL3::accumulateDelay(uint32_t samples)
{
    pending_samples_ += samples;
}

void VGMOPL3::nativePreGenerate()
{
    if (real_chip_)
        real_chip_->nativePreGenerate();
}

void VGMOPL3::nativePostGenerate()
{
    if (real_chip_)
        real_chip_->nativePostGenerate();
}

void VGMOPL3::nativeGenerate(int16_t *frame)
{
    // Generate audio from real OPL3 emulator
    if (real_chip_)
    {
        real_chip_->nativeGenerate(frame);

        // Store in PCM buffer for WAV output
        pcm_buffer_.push_back(frame[0]);  // Left channel
        pcm_buffer_.push_back(frame[1]);  // Right channel
    }
    else
    {
        // No emulator - output silence
        frame[0] = 0;
        frame[1] = 0;
    }
}

void VGMOPL3::finalize()
{
    // Flush any remaining delays
    flushDelay();

    // Write end-of-data marker
    vgm_buffer_.push_back(0x66);

    // Write GD3 tag if provided
    if (gd3_tag_)
    {
        auto *u32 = reinterpret_cast<uint32_t*>(vgm_buffer_.data());
        u32[0x14 / 4] = vgm_buffer_.size() - 0x14;  // GD3 offset
        writeGD3Tag();
    }

    // Update header with final values
    updateHeader();
}

void VGMOPL3::updateHeader()
{
    auto *u32 = reinterpret_cast<uint32_t*>(vgm_buffer_.data());

    // EoF offset (file size - 4)
    u32[0x04 / 4] = vgm_buffer_.size() - 0x04;

    // Total # samples (for playback duration)
    u32[0x18 / 4] = total_samples_;
}

void VGMOPL3::writeGD3Tag()
{
    if (!gd3_tag_)
        return;

    std::vector<uint8_t> gd3_data = gd3_tag_->serialize();
    vgm_buffer_.insert(vgm_buffer_.end(), gd3_data.begin(), gd3_data.end());
}

bool VGMOPL3::savePCMtoWAV(const char* filepath)
{
    FILE *f = fopen(filepath, "wb");
    if (!f)
        return false;

    // Calculate sizes
    uint32_t num_samples = pcm_buffer_.size() / 2;  // Stereo: 2 samples per frame
    uint32_t data_size = pcm_buffer_.size() * 2;    // 16-bit samples
    uint32_t file_size = 36 + data_size;

    // WAV header
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;      // PCM
    uint16_t num_channels = 2;      // Stereo
    uint32_t sample_rate = 44100;
    uint32_t byte_rate = sample_rate * num_channels * 2;  // 44100 * 2 * 16/8
    uint16_t block_align = num_channels * 2;
    uint16_t bits_per_sample = 16;

    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(pcm_buffer_.data(), 2, pcm_buffer_.size(), f);

    fclose(f);
    return true;
}
