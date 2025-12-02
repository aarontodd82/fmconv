/*
 * Source Format - Identifies the original file format before conversion
 *
 * This enum is stored in the FM9 header to indicate what format
 * the file was converted from. Useful for display and categorization.
 *
 * Format ranges:
 *   0x00       = Unknown
 *   0x01-0x0F  = Pass-through (VGM, VGZ, FM9)
 *   0x10-0x1F  = MIDI-style formats (libADLMIDI)
 *   0x20-0x5F  = Native OPL formats (AdPlug)
 *   0x60-0x9F  = Tracker formats (OpenMPT)
 *   0xA0-0xFF  = Reserved for future use
 */

#ifndef SOURCE_FORMAT_H
#define SOURCE_FORMAT_H

#include <cstdint>
#include <cstring>

enum SourceFormat : uint8_t {
    // ========================================================================
    // Unknown (0x00)
    // ========================================================================
    SRC_UNKNOWN         = 0x00,

    // ========================================================================
    // Pass-through / Container formats (0x01-0x0F)
    // ========================================================================
    SRC_VGM             = 0x01,  // Video Game Music (uncompressed)
    SRC_VGZ             = 0x02,  // Video Game Music (gzip compressed)
    SRC_FM9             = 0x03,  // FM9 (re-encode)

    // ========================================================================
    // MIDI-style formats (0x10-0x1F) - libADLMIDI
    // These require FM instrument bank selection
    // ========================================================================
    SRC_MID             = 0x10,  // Standard MIDI File (.mid, .midi, .smf)
    SRC_KAR             = 0x11,  // Karaoke MIDI (.kar)
    SRC_RMI             = 0x12,  // RIFF MIDI (.rmi)
    SRC_XMI             = 0x13,  // Extended MIDI - Miles Sound System (.xmi)
    SRC_MUS             = 0x14,  // DMX Music - DOOM, Heretic, Hexen (.mus)
    SRC_HMP             = 0x15,  // HMI MIDI - Descent, etc. (.hmp)
    SRC_HMI             = 0x16,  // HMI MIDI variant (.hmi)
    SRC_KLM             = 0x17,  // Wacky Wheels Music (.klm)

    // ========================================================================
    // Native OPL formats (0x20-0x5F) - AdPlug
    // These have embedded OPL instruments
    // ========================================================================

    // Reality AdLib Tracker
    SRC_RAD             = 0x20,  // Reality AdLib Tracker (.rad)

    // id Software / Apogee
    SRC_IMF             = 0x21,  // id Software Music - Wolf3D, Keen (.imf, .wlf)
    SRC_WLF             = 0x21,  // Alias for IMF (.wlf)
    SRC_ADLIB           = 0x22,  // id Software Music variant (.adlib)

    // DOSBox capture
    SRC_DRO             = 0x23,  // DOSBox Raw OPL (.dro)

    // Creative / Sound Blaster
    SRC_CMF             = 0x24,  // Creative Music File (.cmf)

    // Adlib Tracker 2
    SRC_A2M             = 0x25,  // Adlib Tracker 2 (.a2m)
    SRC_A2T             = 0x26,  // Adlib Tracker 2 variant (.a2t)

    // Other trackers
    SRC_AMD             = 0x27,  // AMUSIC Adlib Tracker (.amd)
    SRC_XMS             = 0x28,  // XMS-Tracker (.xms)
    SRC_BAM             = 0x29,  // Bob's Adlib Music (.bam)
    SRC_CFF             = 0x2A,  // Boomtracker 4.0 (.cff)
    SRC_D00             = 0x2B,  // Packed EdLib (.d00)
    SRC_DFM             = 0x2C,  // Digital-FM (.dfm)
    SRC_HSC             = 0x2D,  // HSC-Tracker (.hsc)
    SRC_HSP             = 0x2E,  // HSC Packed (.hsp)
    SRC_KSM             = 0x2F,  // Ken Silverman Music - Build engine (.ksm)
    SRC_MAD             = 0x30,  // Mlat Adlib Tracker (.mad)
    SRC_MKJ             = 0x31,  // MKJamz (.mkj)
    SRC_DTM_OPL         = 0x32,  // DeFy Adlib Tracker (.dtm) - AdPlug version
    SRC_MTK             = 0x33,  // MPU-401 Trakker (.mtk)
    SRC_MTR             = 0x34,  // Master Tracker (.mtr)
    SRC_SA2             = 0x35,  // Surprise! Adlib Tracker 2 (.sa2)
    SRC_SAT             = 0x36,  // Surprise! Adlib Tracker (.sat)
    SRC_XAD             = 0x37,  // Various - FLASH, BMF, Hybrid, etc. (.xad)
    SRC_BMF             = 0x38,  // BMF Adlib Tracker (.bmf)
    SRC_LDS             = 0x39,  // LOUDNESS Sound System (.lds)
    SRC_PLX             = 0x3A,  // PALLADIX Sound System (.plx)
    SRC_XSM             = 0x3B,  // eXtra Simple Music (.xsm)
    SRC_PIS             = 0x3C,  // Beni Tracker PIS Player (.pis)
    SRC_MSC             = 0x3D,  // AdLib MSC Player (.msc)
    SRC_SNG             = 0x3E,  // Various - SNGPlay, Faust, etc. (.sng)
    SRC_JBM             = 0x3F,  // JBM Adlib Music (.jbm)
    SRC_GOT             = 0x40,  // God of Thunder Music (.got)
    SRC_SOP             = 0x41,  // Note Sequencer by sopepos (.sop)
    SRC_ROL             = 0x42,  // AdLib Visual Composer (.rol)
    SRC_RAW             = 0x43,  // Raw AdLib Capture (.raw)
    SRC_RAC             = 0x44,  // Raw AdLib Capture variant (.rac)

    // LucasArts / Sierra
    SRC_LAA             = 0x45,  // LucasArts AdLib Audio (.laa)
    SRC_SCI             = 0x46,  // Sierra SCI (.sci)

    // AdLib MIDI variants
    SRC_MDI             = 0x47,  // AdLib MIDIPlay File (.mdi)
    SRC_MDY             = 0x48,  // AdLib MIDI variant (.mdy)
    SRC_IMS             = 0x49,  // AdLib IMS variant (.ims)

    // Westwood
    SRC_ADL             = 0x4A,  // Westwood ADL (.adl)

    // Coktel Vision (also uses .adl but different format)
    SRC_COKTEL          = 0x4B,  // Coktel Vision Adlib Music (.adl)

    // Twin TrackPlayer
    SRC_DMO             = 0x4C,  // TwinTeam (.dmo)

    // Softstar RIX
    SRC_RIX             = 0x4D,  // Softstar RIX OPL Music (.rix)
    SRC_MKF             = 0x4E,  // Softstar RIX variant (.mkf)

    // Ultima
    SRC_U6M             = 0x4F,  // Ultima 6 Music (.m)

    // Herbulot AdLib System
    SRC_HSQ             = 0x50,  // Herbulot AdLib System (.hsq)
    SRC_SQX             = 0x51,  // Herbulot variant (.sqx)
    SRC_SDB             = 0x52,  // Herbulot variant (.sdb)
    SRC_AGD             = 0x53,  // Herbulot variant (.agd)
    SRC_HA2             = 0x54,  // Herbulot variant (.ha2)

    // ========================================================================
    // Tracker formats (0x60-0x9F) - OpenMPT
    // These may have OPL instruments, samples, or both
    // ========================================================================

    // Major tracker formats
    SRC_MOD             = 0x60,  // ProTracker (.mod)
    SRC_S3M             = 0x61,  // Scream Tracker 3 (.s3m)
    SRC_XM              = 0x62,  // FastTracker 2 (.xm)
    SRC_IT              = 0x63,  // Impulse Tracker (.it)
    SRC_MPTM            = 0x64,  // OpenMPT Module (.mptm)

    // Scream Tracker variants
    SRC_STM             = 0x65,  // Scream Tracker 2 (.stm)
    SRC_STX             = 0x66,  // Scream Tracker Extended (.stx)
    SRC_STP             = 0x67,  // Scream Tracker Project (.stp)

    // Composer 669
    SRC_669             = 0x68,  // Composer 669 (.669)
    SRC_667             = 0x69,  // Composer 667 (.667)
    SRC_C67             = 0x6A,  // Composer 667 variant (.c67)

    // MultiTracker
    SRC_MTM             = 0x6B,  // MultiTracker (.mtm)

    // OctaMED
    SRC_MED             = 0x6C,  // OctaMED (.med)

    // Oktalyzer
    SRC_OKT             = 0x6D,  // Oktalyzer (.okt)

    // Farandole
    SRC_FAR             = 0x6E,  // Farandole Composer (.far)
    SRC_FMT             = 0x6F,  // Farandole Module variant (.fmt)

    // Digitrakker
    SRC_MDL             = 0x70,  // Digitrakker (.mdl)

    // Extreme's Tracker / Velvet Studio
    SRC_AMS             = 0x71,  // Extreme's Tracker / Velvet Studio (.ams)

    // DigiBooster
    SRC_DBM             = 0x72,  // DigiBooster Pro (.dbm)
    SRC_DIGI            = 0x73,  // DigiBooster (.digi)

    // X-Tracker
    SRC_DMF             = 0x74,  // X-Tracker (.dmf)

    // DSIK
    SRC_DSM             = 0x75,  // DSIK Format (.dsm)
    SRC_DSYM            = 0x76,  // DSIK Symbol variant (.dsym)

    // DeFy Adlib Tracker (OpenMPT version)
    SRC_DTM             = 0x77,  // DeFy Adlib Tracker (.dtm) - OpenMPT version

    // ASYLUM
    SRC_AMF             = 0x78,  // ASYLUM Music Format (.amf)

    // Epic Megagames
    SRC_PSM             = 0x79,  // Epic Megagames MASI (.psm)

    // MadTracker
    SRC_MT2             = 0x7A,  // MadTracker 2 (.mt2)

    // Unreal
    SRC_UMX             = 0x7B,  // Unreal Music (.umx)

    // Jazz Jackrabbit
    SRC_J2B             = 0x7C,  // Jazz Jackrabbit 2 (.j2b)

    // PolyTracker
    SRC_PTM             = 0x7D,  // PolyTracker (.ptm)
    SRC_PPM             = 0x7E,  // Packed PolyTracker (.ppm)
    SRC_PLM             = 0x7F,  // Plastic Music (.plm)

    // Startracker
    SRC_SFX             = 0x80,  // Startracker (.sfx)
    SRC_SFX2            = 0x81,  // Startracker 2 (.sfx2)

    // NEST
    SRC_NST             = 0x82,  // NEST Sound Tracker (.nst)

    // Grave Composer
    SRC_WOW             = 0x83,  // Grave Composer (.wow)

    // UltraTracker
    SRC_ULT             = 0x84,  // UltraTracker (.ult)

    // GEMINI
    SRC_GDM             = 0x85,  // GEMINI Sound Format (.gdm)

    // MO3 Compressed
    SRC_MO3             = 0x86,  // MO3 Compressed (.mo3)

    // OXM
    SRC_OXM             = 0x87,  // OXM Format (.oxm)

    // Real Tracker
    SRC_RTM             = 0x88,  // Real Tracker (.rtm)

    // ProTracker variants
    SRC_PT36            = 0x89,  // ProTracker 3.6 (.pt36)
    SRC_M15             = 0x8A,  // 15-instrument MOD variant (.m15)
    SRC_STK             = 0x8B,  // Soundtracker (.stk)
    SRC_ST26            = 0x8C,  // SoundTracker 2.6 (.st26)
    SRC_UNIC            = 0x8D,  // UNIC Format (.unic)

    // Compressed containers
    SRC_ICE             = 0x8E,  // ICE Tracker Compressed (.ice)
    SRC_MMCMP           = 0x8F,  // MultiMedia Compact (.mmcmp)
    SRC_XPK             = 0x90,  // XPK Packed (.xpk)
    SRC_MMS             = 0x91,  // MultiMedia Sound (.mms)

    // CBA
    SRC_CBA             = 0x92,  // CBA Module (.cba)

    // EMU Tracker
    SRC_ETX             = 0x93,  // EMU Tracker 2.0 (.etx)

    // Future Composer
    SRC_FC              = 0x94,  // Future Composer (.fc)
    SRC_FC13            = 0x95,  // Future Composer 1.3 (.fc13)
    SRC_FC14            = 0x96,  // Future Composer 1.4 (.fc14)

    // Future Sound Tracker
    SRC_FST             = 0x97,  // Future Sound Tracker (.fst)

    // FamiTracker
    SRC_FTM             = 0x98,  // FamiTracker Module (.ftm)

    // Game Music Creator
    SRC_GMC             = 0x99,  // Game Music Creator (.gmc)

    // Graoumf Tracker
    SRC_GTK             = 0x9A,  // Graoumf Tracker (.gtk)
    SRC_GT2             = 0x9B,  // Graoumf Tracker 2 (.gt2)

    // PumaTracker
    SRC_PUMA            = 0x9C,  // PumaTracker (.puma)

    // Other variants
    SRC_SMOD            = 0x9D,  // Scream Tracker Module variant (.smod)
    SRC_SYMMOD          = 0x9E,  // Symbolic Module variant (.symmod)
    SRC_TCB             = 0x9F,  // TFC Tracker (.tcb)
    SRC_XMF             = 0xA0,  // XM File variant (.xmf)

    // ========================================================================
    // Reserved (0xA1-0xFF)
    // ========================================================================
};

// Get human-readable format name from source format code
inline const char* getSourceFormatName(SourceFormat fmt) {
    switch (fmt) {
        // Pass-through
        case SRC_VGM:       return "VGM";
        case SRC_VGZ:       return "VGZ";
        case SRC_FM9:       return "FM9";

        // MIDI-style
        case SRC_MID:       return "MIDI";
        case SRC_KAR:       return "Karaoke MIDI";
        case SRC_RMI:       return "RIFF MIDI";
        case SRC_XMI:       return "Miles XMI";
        case SRC_MUS:       return "DMX MUS";
        case SRC_HMP:       return "HMI HMP";
        case SRC_HMI:       return "HMI";
        case SRC_KLM:       return "Wacky Wheels";

        // AdPlug Native OPL
        case SRC_RAD:       return "Reality AdLib Tracker";
        case SRC_IMF:       return "id Software IMF";
        case SRC_ADLIB:     return "id Software ADLIB";
        case SRC_DRO:       return "DOSBox Raw OPL";
        case SRC_CMF:       return "Creative Music File";
        case SRC_A2M:       return "Adlib Tracker 2";
        case SRC_A2T:       return "Adlib Tracker 2";
        case SRC_AMD:       return "AMUSIC";
        case SRC_XMS:       return "XMS-Tracker";
        case SRC_BAM:       return "Bob's Adlib Music";
        case SRC_CFF:       return "Boomtracker";
        case SRC_D00:       return "EdLib";
        case SRC_DFM:       return "Digital-FM";
        case SRC_HSC:       return "HSC-Tracker";
        case SRC_HSP:       return "HSC Packed";
        case SRC_KSM:       return "Ken Silverman Music";
        case SRC_MAD:       return "Mlat Adlib Tracker";
        case SRC_MKJ:       return "MKJamz";
        case SRC_DTM_OPL:   return "DeFy Adlib Tracker";
        case SRC_MTK:       return "MPU-401 Trakker";
        case SRC_MTR:       return "Master Tracker";
        case SRC_SA2:       return "Surprise! Adlib Tracker 2";
        case SRC_SAT:       return "Surprise! Adlib Tracker";
        case SRC_XAD:       return "XAD";
        case SRC_BMF:       return "BMF Adlib Tracker";
        case SRC_LDS:       return "LOUDNESS";
        case SRC_PLX:       return "PALLADIX";
        case SRC_XSM:       return "eXtra Simple Music";
        case SRC_PIS:       return "Beni Tracker";
        case SRC_MSC:       return "AdLib MSC";
        case SRC_SNG:       return "SNGPlay";
        case SRC_JBM:       return "JBM Adlib Music";
        case SRC_GOT:       return "God of Thunder";
        case SRC_SOP:       return "sopepos Sequencer";
        case SRC_ROL:       return "AdLib Visual Composer";
        case SRC_RAW:       return "Raw AdLib";
        case SRC_RAC:       return "Raw AdLib";
        case SRC_LAA:       return "LucasArts AdLib";
        case SRC_SCI:       return "Sierra SCI";
        case SRC_MDI:       return "AdLib MIDIPlay";
        case SRC_MDY:       return "AdLib MDY";
        case SRC_IMS:       return "AdLib IMS";
        case SRC_ADL:       return "Westwood ADL";
        case SRC_COKTEL:    return "Coktel Vision";
        case SRC_DMO:       return "TwinTeam";
        case SRC_RIX:       return "Softstar RIX";
        case SRC_MKF:       return "Softstar RIX";
        case SRC_U6M:       return "Ultima 6";
        case SRC_HSQ:       return "Herbulot AdLib";
        case SRC_SQX:       return "Herbulot AdLib";
        case SRC_SDB:       return "Herbulot AdLib";
        case SRC_AGD:       return "Herbulot AdLib";
        case SRC_HA2:       return "Herbulot AdLib";

        // OpenMPT Tracker
        case SRC_MOD:       return "ProTracker";
        case SRC_S3M:       return "Scream Tracker 3";
        case SRC_XM:        return "FastTracker 2";
        case SRC_IT:        return "Impulse Tracker";
        case SRC_MPTM:      return "OpenMPT";
        case SRC_STM:       return "Scream Tracker 2";
        case SRC_STX:       return "Scream Tracker Ext";
        case SRC_STP:       return "Scream Tracker Project";
        case SRC_669:       return "Composer 669";
        case SRC_667:       return "Composer 667";
        case SRC_C67:       return "Composer 667";
        case SRC_MTM:       return "MultiTracker";
        case SRC_MED:       return "OctaMED";
        case SRC_OKT:       return "Oktalyzer";
        case SRC_FAR:       return "Farandole";
        case SRC_FMT:       return "Farandole";
        case SRC_MDL:       return "Digitrakker";
        case SRC_AMS:       return "Velvet Studio";
        case SRC_DBM:       return "DigiBooster Pro";
        case SRC_DIGI:      return "DigiBooster";
        case SRC_DMF:       return "X-Tracker";
        case SRC_DSM:       return "DSIK";
        case SRC_DSYM:      return "DSIK Symbol";
        case SRC_DTM:       return "DeFy Adlib Tracker";
        case SRC_AMF:       return "ASYLUM";
        case SRC_PSM:       return "Epic MASI";
        case SRC_MT2:       return "MadTracker 2";
        case SRC_UMX:       return "Unreal Music";
        case SRC_J2B:       return "Jazz Jackrabbit 2";
        case SRC_PTM:       return "PolyTracker";
        case SRC_PPM:       return "Packed PolyTracker";
        case SRC_PLM:       return "Plastic Music";
        case SRC_SFX:       return "Startracker";
        case SRC_SFX2:      return "Startracker 2";
        case SRC_NST:       return "NoiseTracker";
        case SRC_WOW:       return "Grave Composer";
        case SRC_ULT:       return "UltraTracker";
        case SRC_GDM:       return "GEMINI";
        case SRC_MO3:       return "MO3";
        case SRC_OXM:       return "OXM";
        case SRC_RTM:       return "Real Tracker";
        case SRC_PT36:      return "ProTracker 3.6";
        case SRC_M15:       return "15-instrument MOD";
        case SRC_STK:       return "Soundtracker";
        case SRC_ST26:      return "SoundTracker 2.6";
        case SRC_UNIC:      return "UNIC Tracker";
        case SRC_ICE:       return "ICE Tracker";
        case SRC_MMCMP:     return "MMCMP";
        case SRC_XPK:       return "XPK";
        case SRC_MMS:       return "MMS";
        case SRC_CBA:       return "CBA";
        case SRC_ETX:       return "EMU Tracker";
        case SRC_FC:        return "Future Composer";
        case SRC_FC13:      return "Future Composer 1.3";
        case SRC_FC14:      return "Future Composer 1.4";
        case SRC_FST:       return "Future Sound Tracker";
        case SRC_FTM:       return "FamiTracker";
        case SRC_GMC:       return "Game Music Creator";
        case SRC_GTK:       return "Graoumf Tracker";
        case SRC_GT2:       return "Graoumf Tracker 2";
        case SRC_PUMA:      return "PumaTracker";
        case SRC_SMOD:      return "SMOD";
        case SRC_SYMMOD:    return "Symbolic";
        case SRC_TCB:       return "TCB Tracker";
        case SRC_XMF:       return "XMF";

        default:            return "Unknown";
    }
}

// Get short extension-style name (e.g., "MOD", "S3M", "RAD")
inline const char* getSourceFormatShortName(SourceFormat fmt) {
    switch (fmt) {
        // Pass-through
        case SRC_VGM:       return "VGM";
        case SRC_VGZ:       return "VGZ";
        case SRC_FM9:       return "FM9";

        // MIDI-style
        case SRC_MID:       return "MID";
        case SRC_KAR:       return "KAR";
        case SRC_RMI:       return "RMI";
        case SRC_XMI:       return "XMI";
        case SRC_MUS:       return "MUS";
        case SRC_HMP:       return "HMP";
        case SRC_HMI:       return "HMI";
        case SRC_KLM:       return "KLM";

        // AdPlug Native OPL
        case SRC_RAD:       return "RAD";
        case SRC_IMF:       return "IMF";
        case SRC_ADLIB:     return "ADLIB";
        case SRC_DRO:       return "DRO";
        case SRC_CMF:       return "CMF";
        case SRC_A2M:       return "A2M";
        case SRC_A2T:       return "A2T";
        case SRC_AMD:       return "AMD";
        case SRC_XMS:       return "XMS";
        case SRC_BAM:       return "BAM";
        case SRC_CFF:       return "CFF";
        case SRC_D00:       return "D00";
        case SRC_DFM:       return "DFM";
        case SRC_HSC:       return "HSC";
        case SRC_HSP:       return "HSP";
        case SRC_KSM:       return "KSM";
        case SRC_MAD:       return "MAD";
        case SRC_MKJ:       return "MKJ";
        case SRC_DTM_OPL:   return "DTM";
        case SRC_MTK:       return "MTK";
        case SRC_MTR:       return "MTR";
        case SRC_SA2:       return "SA2";
        case SRC_SAT:       return "SAT";
        case SRC_XAD:       return "XAD";
        case SRC_BMF:       return "BMF";
        case SRC_LDS:       return "LDS";
        case SRC_PLX:       return "PLX";
        case SRC_XSM:       return "XSM";
        case SRC_PIS:       return "PIS";
        case SRC_MSC:       return "MSC";
        case SRC_SNG:       return "SNG";
        case SRC_JBM:       return "JBM";
        case SRC_GOT:       return "GOT";
        case SRC_SOP:       return "SOP";
        case SRC_ROL:       return "ROL";
        case SRC_RAW:       return "RAW";
        case SRC_RAC:       return "RAC";
        case SRC_LAA:       return "LAA";
        case SRC_SCI:       return "SCI";
        case SRC_MDI:       return "MDI";
        case SRC_MDY:       return "MDY";
        case SRC_IMS:       return "IMS";
        case SRC_ADL:       return "ADL";
        case SRC_COKTEL:    return "ADL";
        case SRC_DMO:       return "DMO";
        case SRC_RIX:       return "RIX";
        case SRC_MKF:       return "MKF";
        case SRC_U6M:       return "M";
        case SRC_HSQ:       return "HSQ";
        case SRC_SQX:       return "SQX";
        case SRC_SDB:       return "SDB";
        case SRC_AGD:       return "AGD";
        case SRC_HA2:       return "HA2";

        // OpenMPT Tracker
        case SRC_MOD:       return "MOD";
        case SRC_S3M:       return "S3M";
        case SRC_XM:        return "XM";
        case SRC_IT:        return "IT";
        case SRC_MPTM:      return "MPTM";
        case SRC_STM:       return "STM";
        case SRC_STX:       return "STX";
        case SRC_STP:       return "STP";
        case SRC_669:       return "669";
        case SRC_667:       return "667";
        case SRC_C67:       return "C67";
        case SRC_MTM:       return "MTM";
        case SRC_MED:       return "MED";
        case SRC_OKT:       return "OKT";
        case SRC_FAR:       return "FAR";
        case SRC_FMT:       return "FMT";
        case SRC_MDL:       return "MDL";
        case SRC_AMS:       return "AMS";
        case SRC_DBM:       return "DBM";
        case SRC_DIGI:      return "DIGI";
        case SRC_DMF:       return "DMF";
        case SRC_DSM:       return "DSM";
        case SRC_DSYM:      return "DSYM";
        case SRC_DTM:       return "DTM";
        case SRC_AMF:       return "AMF";
        case SRC_PSM:       return "PSM";
        case SRC_MT2:       return "MT2";
        case SRC_UMX:       return "UMX";
        case SRC_J2B:       return "J2B";
        case SRC_PTM:       return "PTM";
        case SRC_PPM:       return "PPM";
        case SRC_PLM:       return "PLM";
        case SRC_SFX:       return "SFX";
        case SRC_SFX2:      return "SFX2";
        case SRC_NST:       return "NST";
        case SRC_WOW:       return "WOW";
        case SRC_ULT:       return "ULT";
        case SRC_GDM:       return "GDM";
        case SRC_MO3:       return "MO3";
        case SRC_OXM:       return "OXM";
        case SRC_RTM:       return "RTM";
        case SRC_PT36:      return "PT36";
        case SRC_M15:       return "M15";
        case SRC_STK:       return "STK";
        case SRC_ST26:      return "ST26";
        case SRC_UNIC:      return "UNIC";
        case SRC_ICE:       return "ICE";
        case SRC_MMCMP:     return "MMCMP";
        case SRC_XPK:       return "XPK";
        case SRC_MMS:       return "MMS";
        case SRC_CBA:       return "CBA";
        case SRC_ETX:       return "ETX";
        case SRC_FC:        return "FC";
        case SRC_FC13:      return "FC13";
        case SRC_FC14:      return "FC14";
        case SRC_FST:       return "FST";
        case SRC_FTM:       return "FTM";
        case SRC_GMC:       return "GMC";
        case SRC_GTK:       return "GTK";
        case SRC_GT2:       return "GT2";
        case SRC_PUMA:      return "PUMA";
        case SRC_SMOD:      return "SMOD";
        case SRC_SYMMOD:    return "SYMMOD";
        case SRC_TCB:       return "TCB";
        case SRC_XMF:       return "XMF";

        default:            return "???";
    }
}

// Get format category (for grouping/icons)
enum SourceFormatCategory : uint8_t {
    CAT_UNKNOWN     = 0,
    CAT_PASSTHROUGH = 1,  // VGM/VGZ/FM9
    CAT_MIDI        = 2,  // MIDI-style formats
    CAT_OPL         = 3,  // Native OPL (AdPlug)
    CAT_TRACKER     = 4,  // Tracker (OpenMPT)
};

inline SourceFormatCategory getSourceFormatCategory(SourceFormat fmt) {
    uint8_t code = static_cast<uint8_t>(fmt);
    if (code == 0x00) return CAT_UNKNOWN;
    if (code <= 0x0F) return CAT_PASSTHROUGH;
    if (code <= 0x1F) return CAT_MIDI;
    if (code <= 0x5F) return CAT_OPL;
    if (code <= 0xA0) return CAT_TRACKER;
    return CAT_UNKNOWN;
}

// Map file extension to source format
inline SourceFormat extensionToSourceFormat(const char* ext) {
    // Skip leading dot if present
    if (ext && ext[0] == '.') ext++;
    if (!ext || !ext[0]) return SRC_UNKNOWN;

    // Convert to lowercase for comparison
    char lower[16] = {0};
    for (int i = 0; i < 15 && ext[i]; i++) {
        lower[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    }

    // Pass-through
    if (!strcmp(lower, "vgm")) return SRC_VGM;
    if (!strcmp(lower, "vgz")) return SRC_VGZ;
    if (!strcmp(lower, "fm9")) return SRC_FM9;

    // MIDI-style
    if (!strcmp(lower, "mid") || !strcmp(lower, "midi") || !strcmp(lower, "smf")) return SRC_MID;
    if (!strcmp(lower, "kar")) return SRC_KAR;
    if (!strcmp(lower, "rmi")) return SRC_RMI;
    if (!strcmp(lower, "xmi")) return SRC_XMI;
    if (!strcmp(lower, "mus")) return SRC_MUS;
    if (!strcmp(lower, "hmp")) return SRC_HMP;
    if (!strcmp(lower, "hmi")) return SRC_HMI;
    if (!strcmp(lower, "klm")) return SRC_KLM;

    // AdPlug Native OPL
    if (!strcmp(lower, "rad")) return SRC_RAD;
    if (!strcmp(lower, "imf")) return SRC_IMF;
    if (!strcmp(lower, "wlf")) return SRC_IMF;
    if (!strcmp(lower, "adlib")) return SRC_ADLIB;
    if (!strcmp(lower, "dro")) return SRC_DRO;
    if (!strcmp(lower, "cmf")) return SRC_CMF;
    if (!strcmp(lower, "a2m")) return SRC_A2M;
    if (!strcmp(lower, "a2t")) return SRC_A2T;
    if (!strcmp(lower, "amd")) return SRC_AMD;
    if (!strcmp(lower, "xms")) return SRC_XMS;
    if (!strcmp(lower, "bam")) return SRC_BAM;
    if (!strcmp(lower, "cff")) return SRC_CFF;
    if (!strcmp(lower, "d00")) return SRC_D00;
    if (!strcmp(lower, "dfm")) return SRC_DFM;
    if (!strcmp(lower, "hsc")) return SRC_HSC;
    if (!strcmp(lower, "hsp")) return SRC_HSP;
    if (!strcmp(lower, "ksm")) return SRC_KSM;
    if (!strcmp(lower, "mad")) return SRC_MAD;
    if (!strcmp(lower, "mkj")) return SRC_MKJ;
    if (!strcmp(lower, "mtk")) return SRC_MTK;
    if (!strcmp(lower, "mtr")) return SRC_MTR;
    if (!strcmp(lower, "sa2")) return SRC_SA2;
    if (!strcmp(lower, "sat")) return SRC_SAT;
    if (!strcmp(lower, "xad")) return SRC_XAD;
    if (!strcmp(lower, "bmf")) return SRC_BMF;
    if (!strcmp(lower, "lds")) return SRC_LDS;
    if (!strcmp(lower, "plx")) return SRC_PLX;
    if (!strcmp(lower, "xsm")) return SRC_XSM;
    if (!strcmp(lower, "pis")) return SRC_PIS;
    if (!strcmp(lower, "msc")) return SRC_MSC;
    if (!strcmp(lower, "sng")) return SRC_SNG;
    if (!strcmp(lower, "jbm")) return SRC_JBM;
    if (!strcmp(lower, "got")) return SRC_GOT;
    if (!strcmp(lower, "sop")) return SRC_SOP;
    if (!strcmp(lower, "rol")) return SRC_ROL;
    if (!strcmp(lower, "raw")) return SRC_RAW;
    if (!strcmp(lower, "rac")) return SRC_RAC;
    if (!strcmp(lower, "laa")) return SRC_LAA;
    if (!strcmp(lower, "sci")) return SRC_SCI;
    if (!strcmp(lower, "mdi")) return SRC_MDI;
    if (!strcmp(lower, "mdy")) return SRC_MDY;
    if (!strcmp(lower, "ims")) return SRC_IMS;
    if (!strcmp(lower, "adl")) return SRC_ADL;
    if (!strcmp(lower, "dmo")) return SRC_DMO;
    if (!strcmp(lower, "rix")) return SRC_RIX;
    if (!strcmp(lower, "mkf")) return SRC_MKF;
    if (!strcmp(lower, "m")) return SRC_U6M;
    if (!strcmp(lower, "hsq")) return SRC_HSQ;
    if (!strcmp(lower, "sqx")) return SRC_SQX;
    if (!strcmp(lower, "sdb")) return SRC_SDB;
    if (!strcmp(lower, "agd")) return SRC_AGD;
    if (!strcmp(lower, "ha2")) return SRC_HA2;

    // OpenMPT Tracker
    if (!strcmp(lower, "mod")) return SRC_MOD;
    if (!strcmp(lower, "s3m")) return SRC_S3M;
    if (!strcmp(lower, "xm")) return SRC_XM;
    if (!strcmp(lower, "it")) return SRC_IT;
    if (!strcmp(lower, "mptm")) return SRC_MPTM;
    if (!strcmp(lower, "stm")) return SRC_STM;
    if (!strcmp(lower, "stx")) return SRC_STX;
    if (!strcmp(lower, "stp")) return SRC_STP;
    if (!strcmp(lower, "669")) return SRC_669;
    if (!strcmp(lower, "667")) return SRC_667;
    if (!strcmp(lower, "c67")) return SRC_C67;
    if (!strcmp(lower, "mtm")) return SRC_MTM;
    if (!strcmp(lower, "med")) return SRC_MED;
    if (!strcmp(lower, "okt")) return SRC_OKT;
    if (!strcmp(lower, "far")) return SRC_FAR;
    if (!strcmp(lower, "fmt")) return SRC_FMT;
    if (!strcmp(lower, "mdl")) return SRC_MDL;
    if (!strcmp(lower, "ams")) return SRC_AMS;
    if (!strcmp(lower, "dbm")) return SRC_DBM;
    if (!strcmp(lower, "digi")) return SRC_DIGI;
    if (!strcmp(lower, "dmf")) return SRC_DMF;
    if (!strcmp(lower, "dsm")) return SRC_DSM;
    if (!strcmp(lower, "dsym")) return SRC_DSYM;
    if (!strcmp(lower, "dtm")) return SRC_DTM;
    if (!strcmp(lower, "amf")) return SRC_AMF;
    if (!strcmp(lower, "psm")) return SRC_PSM;
    if (!strcmp(lower, "mt2")) return SRC_MT2;
    if (!strcmp(lower, "umx")) return SRC_UMX;
    if (!strcmp(lower, "j2b")) return SRC_J2B;
    if (!strcmp(lower, "ptm")) return SRC_PTM;
    if (!strcmp(lower, "ppm")) return SRC_PPM;
    if (!strcmp(lower, "plm")) return SRC_PLM;
    if (!strcmp(lower, "sfx")) return SRC_SFX;
    if (!strcmp(lower, "sfx2")) return SRC_SFX2;
    if (!strcmp(lower, "nst")) return SRC_NST;
    if (!strcmp(lower, "wow")) return SRC_WOW;
    if (!strcmp(lower, "ult")) return SRC_ULT;
    if (!strcmp(lower, "gdm")) return SRC_GDM;
    if (!strcmp(lower, "mo3")) return SRC_MO3;
    if (!strcmp(lower, "oxm")) return SRC_OXM;
    if (!strcmp(lower, "rtm")) return SRC_RTM;
    if (!strcmp(lower, "pt36")) return SRC_PT36;
    if (!strcmp(lower, "m15")) return SRC_M15;
    if (!strcmp(lower, "stk")) return SRC_STK;
    if (!strcmp(lower, "st26")) return SRC_ST26;
    if (!strcmp(lower, "unic")) return SRC_UNIC;
    if (!strcmp(lower, "ice")) return SRC_ICE;
    if (!strcmp(lower, "mmcmp")) return SRC_MMCMP;
    if (!strcmp(lower, "xpk")) return SRC_XPK;
    if (!strcmp(lower, "mms")) return SRC_MMS;
    if (!strcmp(lower, "cba")) return SRC_CBA;
    if (!strcmp(lower, "etx")) return SRC_ETX;
    if (!strcmp(lower, "fc")) return SRC_FC;
    if (!strcmp(lower, "fc13")) return SRC_FC13;
    if (!strcmp(lower, "fc14")) return SRC_FC14;
    if (!strcmp(lower, "fst")) return SRC_FST;
    if (!strcmp(lower, "ftm")) return SRC_FTM;
    if (!strcmp(lower, "gmc")) return SRC_GMC;
    if (!strcmp(lower, "gtk")) return SRC_GTK;
    if (!strcmp(lower, "gt2")) return SRC_GT2;
    if (!strcmp(lower, "puma")) return SRC_PUMA;
    if (!strcmp(lower, "smod")) return SRC_SMOD;
    if (!strcmp(lower, "symmod")) return SRC_SYMMOD;
    if (!strcmp(lower, "tcb")) return SRC_TCB;
    if (!strcmp(lower, "xmf")) return SRC_XMF;

    return SRC_UNKNOWN;
}

#endif // SOURCE_FORMAT_H
