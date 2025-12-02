/*
 * OpenMPT Export - OPL register capture and sample audio export
 *
 * This file is compiled as part of libopenmpt to access internal APIs.
 * Provides a simple C-style interface for fmconv to call.
 */

#include "common/stdafx.h"

#include "soundlib/Sndfile.h"
#include "soundlib/OPL.h"
#include "soundlib/mod_specifications.h"
#include "soundlib/AudioReadTarget.h"
#include "common/FileReader.h"
#include "common/mptPathString.h"
#include "common/mptString.h"

#include "mpt/io_read/filecursor_memory.hpp"
#include "mpt/string_transcode/transcode.hpp"

#include "common/mptRandom.h"

#include <vector>
#include <map>
#include <fstream>
#include <cstring>

// Include our header (outside OpenMPT namespace)
#include "openmpt_export.h"

OPENMPT_NAMESPACE_BEGIN

//============================================================================
// OPL Register Logger - captures OPL writes for VGM export
//============================================================================

class OPLCaptureLogger : public OPL::IRegisterLogger {
public:
    struct RegisterWrite {
        uint64 sample_offset;
        uint8 reg_lo;
        uint8 reg_hi;  // 0 = low bank, 1 = high bank
        uint8 value;
    };

    OPLCaptureLogger() = default;

    void Port(CHANNELINDEX /*c*/, OPL::Register reg, OPL::Value value) override {
        // Only log if value changed (optimization)
        if (auto it = prev_values_.find(reg); it != prev_values_.end()) {
            if (it->second == value) return;
        }
        prev_values_[reg] = value;

        RegisterWrite write;
        write.sample_offset = total_samples_;
        write.reg_lo = static_cast<uint8>(reg & 0xFF);
        write.reg_hi = static_cast<uint8>(reg >> 8);
        write.value = value;
        register_writes_.push_back(write);
    }

    void MoveChannel(CHANNELINDEX /*from*/, CHANNELINDEX /*to*/) override {
        // Not needed for VGM export
    }

    void AddSamples(uint64 count) {
        total_samples_ += count;
    }

    void Reset() {
        register_writes_.clear();
        prev_values_.clear();
        total_samples_ = 0;
    }

    const std::vector<RegisterWrite>& GetWrites() const {
        return register_writes_;
    }

    uint64 GetTotalSamples() const { return total_samples_; }

    bool HasData() const { return !register_writes_.empty(); }

private:
    std::vector<RegisterWrite> register_writes_;
    std::map<OPL::Register, OPL::Value> prev_values_;
    uint64 total_samples_ = 0;
};

//============================================================================
// VGM Writer - converts captured OPL writes to VGM format
//============================================================================

static void WriteVGMFromCapture(std::vector<uint8>& buffer,
                                const std::vector<OPLCaptureLogger::RegisterWrite>& writes,
                                uint64 total_samples) {
    // VGM 1.51 header (256 bytes)
    buffer.resize(256, 0);

    // Magic: "Vgm "
    buffer[0] = 'V'; buffer[1] = 'g';
    buffer[2] = 'm'; buffer[3] = ' ';

    // Version 1.51
    buffer[0x08] = 0x51; buffer[0x09] = 0x01;

    // YMF262 (OPL3) clock = 14318180
    uint32 clock = 14318180;
    buffer[0x5C] = (clock >> 0) & 0xFF;
    buffer[0x5D] = (clock >> 8) & 0xFF;
    buffer[0x5E] = (clock >> 16) & 0xFF;
    buffer[0x5F] = (clock >> 24) & 0xFF;

    // VGM data offset (relative to 0x34) = 256 - 0x34 = 204
    uint32 data_offset = 256 - 0x34;
    buffer[0x34] = (data_offset >> 0) & 0xFF;
    buffer[0x35] = (data_offset >> 8) & 0xFF;
    buffer[0x36] = (data_offset >> 16) & 0xFF;
    buffer[0x37] = (data_offset >> 24) & 0xFF;

    // Write register data
    uint64 prev_offset = 0;

    for (const auto& write : writes) {
        // Write delay if needed
        uint64 delay = write.sample_offset - prev_offset;
        while (delay > 0) {
            if (delay <= 16) {
                buffer.push_back(0x6F + static_cast<uint8>(delay));
                delay = 0;
            } else if (delay == 735) {
                buffer.push_back(0x62);  // 1/60th second
                delay = 0;
            } else if (delay == 882) {
                buffer.push_back(0x63);  // 1/50th second
                delay = 0;
            } else if (delay <= 65535) {
                buffer.push_back(0x61);
                buffer.push_back(delay & 0xFF);
                buffer.push_back((delay >> 8) & 0xFF);
                delay = 0;
            } else {
                buffer.push_back(0x61);
                buffer.push_back(0xFF);
                buffer.push_back(0xFF);
                delay -= 65535;
            }
        }
        prev_offset = write.sample_offset;

        // Write OPL3 register command
        // 0x5E = OPL3 port 0, 0x5F = OPL3 port 1
        buffer.push_back(0x5E + write.reg_hi);
        buffer.push_back(write.reg_lo);
        buffer.push_back(write.value);
    }

    // Final delay to end of track
    uint64 final_delay = total_samples - prev_offset;
    while (final_delay > 0) {
        if (final_delay <= 65535) {
            if (final_delay > 0) {
                buffer.push_back(0x61);
                buffer.push_back(final_delay & 0xFF);
                buffer.push_back((final_delay >> 8) & 0xFF);
            }
            final_delay = 0;
        } else {
            buffer.push_back(0x61);
            buffer.push_back(0xFF);
            buffer.push_back(0xFF);
            final_delay -= 65535;
        }
    }

    // End of sound data
    buffer.push_back(0x66);

    // Update header - EOF offset (relative to 0x04)
    uint32 eof_offset = static_cast<uint32>(buffer.size()) - 4;
    buffer[0x04] = (eof_offset >> 0) & 0xFF;
    buffer[0x05] = (eof_offset >> 8) & 0xFF;
    buffer[0x06] = (eof_offset >> 16) & 0xFF;
    buffer[0x07] = (eof_offset >> 24) & 0xFF;

    // Total samples
    uint32 samples32 = static_cast<uint32>(total_samples);
    buffer[0x18] = (samples32 >> 0) & 0xFF;
    buffer[0x19] = (samples32 >> 8) & 0xFF;
    buffer[0x1A] = (samples32 >> 16) & 0xFF;
    buffer[0x1B] = (samples32 >> 24) & 0xFF;
}

OPENMPT_NAMESPACE_END

//============================================================================
// External C API (outside namespace)
//============================================================================

extern "C" {

struct OpenmptExportContext {
    std::unique_ptr<OpenMPT::CSoundFile> sndFile;
    std::vector<char> fileData;
    std::string error;

    // Results
    std::vector<uint8_t> vgm_data;
    std::vector<int16_t> pcm_data;
    uint32_t sample_rate;

    // Module info
    std::string title;
    std::string artist;
    std::string message;
    std::string tracker;
    std::string format_type;
    std::string format_name;
    bool has_opl;
    bool has_samples;
};

openmpt_export_context* openmpt_export_create(void) {
    return new OpenmptExportContext();
}

void openmpt_export_destroy(openmpt_export_context* ctx) {
    if (ctx) {
        if (ctx->sndFile) {
            ctx->sndFile->Destroy();
        }
        delete ctx;
    }
}

int openmpt_export_load(openmpt_export_context* ctx, const char* filepath) {
    if (!ctx) return 0;

    try {
        // Read file
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            ctx->error = "Failed to open file";
            return 0;
        }

        ctx->fileData = std::vector<char>(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        // Create CSoundFile
        ctx->sndFile = std::make_unique<OpenMPT::CSoundFile>();

        OpenMPT::FileReader fileReader(
            mpt::const_byte_span(
                reinterpret_cast<const std::byte*>(ctx->fileData.data()),
                ctx->fileData.size()));

        if (!ctx->sndFile->Create(fileReader, OpenMPT::CSoundFile::loadCompleteModule)) {
            ctx->error = "Failed to load module";
            return 0;
        }

        // Extract info
        // GetTitle() returns std::string which is already UTF8-compatible
        ctx->title = ctx->sndFile->GetTitle();

        // Artist (mpt::ustring needs conversion to UTF8)
        ctx->artist = mpt::transcode<std::string>(mpt::common_encoding::utf8, ctx->sndFile->m_songArtist);

        // Song message/comments
        ctx->message = ctx->sndFile->m_songMessage.GetFormatted(OpenMPT::SongMessage::leLF);

        // What tracker made it
        ctx->tracker = mpt::transcode<std::string>(mpt::common_encoding::utf8,
            ctx->sndFile->m_modFormat.madeWithTracker);

        // Format info
        ctx->format_type = ctx->sndFile->GetModSpecifications().fileExtension;
        ctx->format_name = mpt::transcode<std::string>(mpt::common_encoding::utf8,
            ctx->sndFile->m_modFormat.formatName);

        // Check for OPL and sample instruments
        ctx->has_opl = false;
        ctx->has_samples = false;

        for (OpenMPT::SAMPLEINDEX smp = 1; smp <= ctx->sndFile->GetNumSamples(); ++smp) {
            const auto& sample = ctx->sndFile->GetSample(smp);
            if (sample.uFlags[OpenMPT::CHN_ADLIB]) {
                ctx->has_opl = true;
            } else if (sample.HasSampleData()) {
                ctx->has_samples = true;
            }
        }

        return 1;

    } catch (const std::exception& e) {
        ctx->error = std::string("Exception: ") + e.what();
        return 0;
    } catch (...) {
        ctx->error = "Unknown exception";
        return 0;
    }
}

int openmpt_export_has_opl(openmpt_export_context* ctx) {
    return ctx && ctx->has_opl ? 1 : 0;
}

int openmpt_export_has_samples(openmpt_export_context* ctx) {
    return ctx && ctx->has_samples ? 1 : 0;
}

const char* openmpt_export_get_title(openmpt_export_context* ctx) {
    return ctx ? ctx->title.c_str() : "";
}

const char* openmpt_export_get_artist(openmpt_export_context* ctx) {
    return ctx ? ctx->artist.c_str() : "";
}

const char* openmpt_export_get_message(openmpt_export_context* ctx) {
    return ctx ? ctx->message.c_str() : "";
}

const char* openmpt_export_get_tracker(openmpt_export_context* ctx) {
    return ctx ? ctx->tracker.c_str() : "";
}

const char* openmpt_export_get_format(openmpt_export_context* ctx) {
    return ctx ? ctx->format_type.c_str() : "";
}

const char* openmpt_export_get_format_name(openmpt_export_context* ctx) {
    return ctx ? ctx->format_name.c_str() : "";
}

const char* openmpt_export_get_error(openmpt_export_context* ctx) {
    return ctx ? ctx->error.c_str() : "";
}

int openmpt_export_render_opl(openmpt_export_context* ctx,
                               uint32_t sample_rate,
                               int max_seconds) {
    if (!ctx || !ctx->sndFile || !ctx->has_opl) return 0;

    try {
        // Configure mixer
        OpenMPT::MixerSettings mixerSettings = ctx->sndFile->m_MixerSettings;
        mixerSettings.gdwMixingFreq = sample_rate;
        mixerSettings.gnChannels = 2;
        ctx->sndFile->SetMixerSettings(mixerSettings);
        ctx->sndFile->SetRepeatCount(0);
        ctx->sndFile->m_bIsRendering = true;

        // Create OPL logger
        OpenMPT::OPLCaptureLogger oplLogger;

        // Replace OPL with our logger
        ctx->sndFile->m_opl = std::make_unique<OpenMPT::OPL>(oplLogger);

        // Reset playback
        ctx->sndFile->ResetPlayPos();
        ctx->sndFile->InitPlayer(true);

        // Render to capture OPL writes
        uint64_t max_samples = static_cast<uint64_t>(max_seconds) * sample_rate;
        uint64_t total_rendered = 0;

        while (total_rendered < max_samples) {
            auto count = ctx->sndFile->ReadOneTick();
            if (count == 0) break;

            oplLogger.AddSamples(count);
            total_rendered += count;
        }

        // Generate VGM from captured registers
        if (oplLogger.HasData()) {
            std::vector<OpenMPT::uint8> vgm_buffer;
            OpenMPT::WriteVGMFromCapture(vgm_buffer, oplLogger.GetWrites(),
                                         oplLogger.GetTotalSamples());

            ctx->vgm_data.assign(vgm_buffer.begin(), vgm_buffer.end());
            ctx->sample_rate = sample_rate;
            return 1;
        }

        return 0;

    } catch (const std::exception& e) {
        ctx->error = std::string("OPL render exception: ") + e.what();
        return 0;
    }
}

int openmpt_export_render_samples(openmpt_export_context* ctx,
                                   uint32_t sample_rate,
                                   int max_seconds) {
    if (!ctx || !ctx->sndFile) return 0;

    try {
        // Configure mixer
        OpenMPT::MixerSettings mixerSettings = ctx->sndFile->m_MixerSettings;
        mixerSettings.gdwMixingFreq = sample_rate;
        mixerSettings.gnChannels = 2;
        ctx->sndFile->SetMixerSettings(mixerSettings);
        ctx->sndFile->SetRepeatCount(0);
        ctx->sndFile->m_bIsRendering = true;

        // Disable OPL for sample-only render
        ctx->sndFile->m_opl.reset();

        // Reset playback
        ctx->sndFile->ResetPlayPos();
        ctx->sndFile->InitPlayer(true);

        // Render audio
        ctx->sample_rate = sample_rate;
        ctx->pcm_data.clear();

        uint64_t max_samples = static_cast<uint64_t>(max_seconds) * sample_rate;

        // Pre-allocate buffer for efficiency
        ctx->pcm_data.reserve(max_samples * 2);

        constexpr size_t CHUNK_SIZE = 4096;
        std::vector<int16_t> buffer(CHUNK_SIZE * 2);  // Stereo interleaved

        // Create dither instance (no dithering for 16-bit output from 32-bit internal)
        OpenMPT::DithersWrapperOpenMPT dithers(OpenMPT::mpt::global_prng(), 0, 2);  // mode 0 = no dither, 2 channels

        uint64_t total_rendered = 0;
        while (total_rendered < max_samples) {
            size_t frames_to_render = std::min(CHUNK_SIZE, static_cast<size_t>(max_samples - total_rendered));

            // Create audio target buffer
            OpenMPT::AudioTargetBufferWithGain<mpt::audio_span_interleaved<int16_t>> target(
                mpt::audio_span_interleaved<int16_t>(buffer.data(), 2, frames_to_render),
                dithers, 1.0f);

            size_t count = ctx->sndFile->Read(
                static_cast<OpenMPT::samplecount_t>(frames_to_render),
                target);

            if (count == 0) break;

            // Append to output
            ctx->pcm_data.insert(ctx->pcm_data.end(),
                                 buffer.begin(), buffer.begin() + count * 2);

            total_rendered += count;
        }

        return ctx->pcm_data.empty() ? 0 : 1;

    } catch (const std::exception& e) {
        ctx->error = std::string("Sample render exception: ") + e.what();
        return 0;
    }
}

const uint8_t* openmpt_export_get_vgm_data(openmpt_export_context* ctx, size_t* size) {
    if (!ctx || ctx->vgm_data.empty()) {
        if (size) *size = 0;
        return nullptr;
    }
    if (size) *size = ctx->vgm_data.size();
    return ctx->vgm_data.data();
}

const int16_t* openmpt_export_get_pcm_data(openmpt_export_context* ctx, size_t* size) {
    if (!ctx || ctx->pcm_data.empty()) {
        if (size) *size = 0;
        return nullptr;
    }
    if (size) *size = ctx->pcm_data.size();
    return ctx->pcm_data.data();
}

uint32_t openmpt_export_get_sample_rate(openmpt_export_context* ctx) {
    return ctx ? ctx->sample_rate : 0;
}

} // extern "C"
