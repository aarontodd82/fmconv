/*
 * Bank Detector - Intelligent bank selection with confidence scoring
 * Implementation
 */

#include "bank_detector.h"
#include <algorithm>
#include <cctype>
#include <fstream>

std::string BankDetector::to_lowercase(const std::string &str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

std::string BankDetector::get_extension(const std::string &path)
{
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos)
        return "";

    return to_lowercase(path.substr(dot_pos));
}

std::string BankDetector::get_filename(const std::string &path)
{
    size_t slash_pos = path.find_last_of("/\\");
    if (slash_pos == std::string::npos)
        return path;

    return path.substr(slash_pos + 1);
}

BankDetection BankDetector::detect_from_hmp_timb(const char *filepath)
{
    // TODO: Parse HMP TIMB chunk to extract bank ID
    // For now, return unknown
    (void)filepath;
    return BankDetection(-1, 0.0f, "");
}

BankDetection BankDetector::detect_from_filename(const std::string &filename)
{
    std::string lower = to_lowercase(filename);

    // ========== BANK 0-1: AIL/Bisqwit ==========
    if (lower.find("starcontrol") != std::string::npos || lower.find("sc3") != std::string::npos)
        return BankDetection(0, 0.85f, "Star Control 3 (AIL)");
    if (lower.find("albion") != std::string::npos)
        return BankDetection(0, 0.85f, "Albion (AIL)");
    if (lower.find("empire2") != std::string::npos)
        return BankDetection(0, 0.85f, "Empire 2 (AIL)");
    if (lower.find("settlers2") != std::string::npos || lower.find("s2") != std::string::npos)
        return BankDetection(0, 0.85f, "Settlers 2 (AIL)");
    if (lower.find("simcity") != std::string::npos || lower.find("sc2000") != std::string::npos)
        return BankDetection(0, 0.85f, "SimCity 2000 (AIL)");

    // ========== BANK 2-13: HMI Family ==========
    // Descent family (2-6)
    if (lower.find("descent2") != std::string::npos || lower.find("d2") != std::string::npos)
        return BankDetection(6, 0.85f, "Descent 2 (HMI)");
    if (lower.find("descent") != std::string::npos)
    {
        if (lower.find("int") != std::string::npos)
            return BankDetection(3, 0.90f, "Descent Int (HMI)");
        if (lower.find("ham") != std::string::npos)
            return BankDetection(4, 0.90f, "Descent Ham (HMI)");
        if (lower.find("rick") != std::string::npos)
            return BankDetection(5, 0.90f, "Descent Rick (HMI)");
        return BankDetection(2, 0.85f, "Descent (HMI)");
    }
    if (lower.find("asterix") != std::string::npos)
        return BankDetection(2, 0.85f, "Asterix (HMI)");
    if (lower.find("normality") != std::string::npos)
        return BankDetection(7, 0.85f, "Normality (HMI)");
    if (lower.find("shattered") != std::string::npos && lower.find("steel") != std::string::npos)
        return BankDetection(8, 0.85f, "Shattered Steel (HMI)");
    if (lower.find("themepark") != std::string::npos || lower.find("theme_park") != std::string::npos)
        return BankDetection(9, 0.85f, "Theme Park (HMI)");
    if (lower.find("3dtable") != std::string::npos || lower.find("toshinden") != std::string::npos)
        return BankDetection(10, 0.85f, "3D Table Sports/Toshinden (HMI)");
    if (lower.find("aces") != std::string::npos && lower.find("deep") != std::string::npos)
        return BankDetection(11, 0.85f, "Aces of the Deep (HMI)");
    if (lower.find("earthsiege") != std::string::npos)
        return BankDetection(12, 0.85f, "Earthsiege (HMI)");
    if (lower.find("anvil") != std::string::npos && lower.find("dawn") != std::string::npos)
        return BankDetection(13, 0.85f, "Anvil of Dawn (HMI)");

    // ========== BANK 14-16: DMX Family ==========
    if (lower.find("doom2") != std::string::npos || lower.find("doom_2") != std::string::npos)
        return BankDetection(14, 0.85f, "Doom 2 (DMX v2)");
    if (lower.find("heretic") != std::string::npos)
        return BankDetection(15, 0.85f, "Heretic (DMX)");
    if (lower.find("hexen") != std::string::npos)
        return BankDetection(15, 0.85f, "Hexen (DMX)");
    if (lower.find("doom") != std::string::npos)
        return BankDetection(16, 0.85f, "DOOM (DMX v1)");

    // ========== BANK 17-54: AIL Family (Extended) ==========
    if (lower.find("discworld") != std::string::npos)
        return BankDetection(17, 0.85f, "Discworld (AIL)");
    if (lower.find("ultima4") != std::string::npos || lower.find("ultima_4") != std::string::npos)
        return BankDetection(17, 0.85f, "Ultima 4 (AIL)");
    if (lower.find("simon") != std::string::npos && lower.find("sorcerer") != std::string::npos)
    {
        if (lower.find("2") != std::string::npos)
            return BankDetection(17, 0.85f, "Simon the Sorcerer 2 (AIL)");
        return BankDetection(57, 0.85f, "Simon the Sorcerer (SB)");
    }
    if (lower.find("warcraft2") != std::string::npos || lower.find("wc2") != std::string::npos)
        return BankDetection(18, 0.85f, "Warcraft 2 (AIL)");
    if (lower.find("warcraft") != std::string::npos || lower.find("wc1") != std::string::npos)
        return BankDetection(43, 0.85f, "Warcraft (AIL)");
    if (lower.find("syndicate") != std::string::npos)
    {
        if (lower.find("wars") != std::string::npos)
            return BankDetection(41, 0.85f, "Syndicate Wars (AIL)");
        return BankDetection(19, 0.85f, "Syndicate (AIL)");
    }
    if (lower.find("guilty") != std::string::npos || lower.find("orion") != std::string::npos || lower.find("terra") != std::string::npos)
    {
        if (lower.find("nova") != std::string::npos)
            return BankDetection(44, 0.85f, "Terra Nova (AIL 4op)");
        return BankDetection(20, 0.85f, "Guilty/Orion/TNSFC (AIL)");
    }
    if (lower.find("magic") != std::string::npos && lower.find("carpet") != std::string::npos)
        return BankDetection(21, 0.85f, "Magic Carpet 2 (AIL)");
    if (lower.find("nemesis") != std::string::npos)
        return BankDetection(22, 0.85f, "Nemesis (AIL)");
    if (lower.find("jagged") != std::string::npos && lower.find("alliance") != std::string::npos)
        return BankDetection(23, 0.85f, "Jagged Alliance (AIL)");
    if (lower.find("when") != std::string::npos && lower.find("worlds") != std::string::npos)
        return BankDetection(24, 0.85f, "When Two Worlds War (AIL)");
    if (lower.find("bards") != std::string::npos && lower.find("tale") != std::string::npos)
        return BankDetection(25, 0.85f, "Bards Tale Construction (AIL)");
    if (lower.find("zork") != std::string::npos)
        return BankDetection(26, 0.85f, "Return to Zork (AIL)");
    if (lower.find("theme") != std::string::npos && lower.find("hospital") != std::string::npos)
        return BankDetection(27, 0.85f, "Theme Hospital (AIL)");
    if (lower.find("nhl") != std::string::npos)
        return BankDetection(28, 0.85f, "NHL PA (AIL)");
    if (lower.find("inherit") != std::string::npos && lower.find("earth") != std::string::npos)
    {
        if (lower.find("2") != std::string::npos || lower.find("file2") != std::string::npos)
            return BankDetection(30, 0.85f, "Inherit The Earth file 2 (AIL)");
        return BankDetection(29, 0.85f, "Inherit The Earth (AIL)");
    }
    if (lower.find("little") != std::string::npos && lower.find("big") != std::string::npos)
        return BankDetection(31, 0.85f, "Little Big Adventure (AIL)");
    if (lower.find("heroes") != std::string::npos && lower.find("might") != std::string::npos)
        return BankDetection(32, 0.85f, "Heroes of Might and Magic II (AIL)");
    if (lower.find("death") != std::string::npos && lower.find("gate") != std::string::npos)
        return BankDetection(33, 0.85f, "Death Gate (AIL)");
    if (lower.find("fifa") != std::string::npos)
        return BankDetection(34, 0.85f, "FIFA International Soccer (AIL)");
    if (lower.find("starship") != std::string::npos && lower.find("invasion") != std::string::npos)
        return BankDetection(35, 0.85f, "Starship Invasion (AIL)");
    if (lower.find("street") != std::string::npos && lower.find("fighter") != std::string::npos)
        return BankDetection(36, 0.85f, "Super Street Fighter 2 (AIL 4op)");
    if (lower.find("lords") != std::string::npos && lower.find("realm") != std::string::npos)
        return BankDetection(37, 0.85f, "Lords of the Realm (AIL)");
    if (lower.find("simfarm") != std::string::npos)
    {
        if (lower.find("settlers") != std::string::npos || lower.find("serf") != std::string::npos)
            return BankDetection(39, 0.85f, "SimFarm/Settlers (AIL)");
        return BankDetection(38, 0.85f, "SimFarm (AIL 4op)");
    }
    if (lower.find("simhealth") != std::string::npos)
        return BankDetection(38, 0.85f, "SimHealth (AIL 4op)");
    if (lower.find("caesar") != std::string::npos)
        return BankDetection(40, 0.85f, "Caesar 2 (AIL)");
    if (lower.find("bubble") != std::string::npos && lower.find("bobble") != std::string::npos)
        return BankDetection(42, 0.85f, "Bubble Bobble (AIL LoudMouth)");
    if (lower.find("system") != std::string::npos && lower.find("shock") != std::string::npos)
        return BankDetection(45, 0.85f, "System Shock (AIL 4op)");
    if (lower.find("advanced") != std::string::npos && lower.find("civilization") != std::string::npos)
        return BankDetection(46, 0.85f, "Advanced Civilization (AIL)");
    if (lower.find("battle") != std::string::npos && lower.find("chess") != std::string::npos)
        return BankDetection(47, 0.85f, "Battle Chess 4000 (AIL 4op)");
    if (lower.find("ultimate") != std::string::npos && lower.find("soccer") != std::string::npos)
        return BankDetection(48, 0.85f, "Ultimate Soccer Manager (AIL 4op)");
    if (lower.find("air") != std::string::npos && lower.find("bucks") != std::string::npos)
        return BankDetection(49, 0.85f, "Air Bucks (AIL)");
    if (lower.find("terminator") != std::string::npos && lower.find("2029") != std::string::npos)
        return BankDetection(49, 0.85f, "Terminator 2029 (AIL)");
    if (lower.find("ultima") != std::string::npos && lower.find("underworld") != std::string::npos)
        return BankDetection(50, 0.85f, "Ultima Underworld 2 (AIL)");
    if (lower.find("putt") != std::string::npos || lower.find("fatty") != std::string::npos || lower.find("kasparov") != std::string::npos)
        return BankDetection(51, 0.85f, "Putt-Putt/Fatty Bear (AIL MT32)");
    if (lower.find("high") != std::string::npos && lower.find("seas") != std::string::npos)
        return BankDetection(52, 0.85f, "High Seas Trader (AIL)");
    if (lower.find("master") != std::string::npos && lower.find("magic") != std::string::npos)
    {
        if (lower.find("orchestral") != std::string::npos || lower.find("drum") != std::string::npos)
            return BankDetection(54, 0.85f, "Master of Magic orchestral (AIL 4op)");
        return BankDetection(53, 0.85f, "Master of Magic (AIL 4op)");
    }
    if (lower.find("lost") != std::string::npos && lower.find("vikings") != std::string::npos)
        return BankDetection(75, 0.85f, "The Lost Vikings (AIL)");
    if (lower.find("monopoly") != std::string::npos)
        return BankDetection(78, 0.85f, "Monopoly Deluxe (AIL)");

    // ========== BANK 55-57: SB Family ==========
    if (lower.find("action") != std::string::npos && lower.find("soccer") != std::string::npos)
        return BankDetection(55, 0.85f, "Action Soccer (SB)");
    if (lower.find("3d") != std::string::npos && lower.find("cyberpuck") != std::string::npos)
        return BankDetection(56, 0.85f, "3D Cyberpuck (SB)");

    // ========== BANK 60-61: OP3 JungleVision/Wallace ==========
    if (lower.find("skunny") != std::string::npos)
        return BankDetection(60, 0.85f, "Skunny (OP3 JungleVision)");
    if (lower.find("nitemare") != std::string::npos)
        return BankDetection(61, 0.85f, "Nitemare 3D (OP3 Wallace)");

    // ========== BANK 62-63, 69-71: TMB Build Engine Family ==========
    if (lower.find("duke") != std::string::npos || lower.find("dn3d") != std::string::npos)
    {
        if (lower.find("1.3") != std::string::npos || lower.find("v1.3") != std::string::npos)
            return BankDetection(71, 0.85f, "Duke Nukem 1.3D (TMB)");
        return BankDetection(62, 0.85f, "Duke Nukem 3D (TMB)");
    }
    if (lower.find("shadow") != std::string::npos && lower.find("warrior") != std::string::npos)
        return BankDetection(63, 0.85f, "Shadow Warrior (TMB)");
    if (lower.find("blood") != std::string::npos)
        return BankDetection(69, 0.85f, "Blood (TMB)");
    if (lower.find("rott") != std::string::npos || (lower.find("rise") != std::string::npos && lower.find("triad") != std::string::npos))
        return BankDetection(70, 0.85f, "Rise of the Triad (TMB)");
    if (lower.find("nam") != std::string::npos)
        return BankDetection(71, 0.85f, "Nam (TMB)");

    // ========== BANK 64: DMX Raptor ==========
    if (lower.find("raptor") != std::string::npos)
        return BankDetection(64, 0.85f, "Raptor (DMX)");

    // ========== BANK 67, 74: Apogee Family ==========
    if (lower.find("wolf3d") != std::string::npos || lower.find("wolfenstein") != std::string::npos)
        return BankDetection(74, 0.85f, "Wolfenstein 3D (WOPL Apogee IMF)");
    if (lower.find("keen") != std::string::npos || lower.find("commander") != std::string::npos)
        return BankDetection(74, 0.85f, "Commander Keen (WOPL Apogee IMF)");
    if (lower.find("blake") != std::string::npos)
        return BankDetection(74, 0.80f, "Blake Stone (WOPL Apogee IMF)");

    // ========== BANK 73: EA Cartooners ==========
    if (lower.find("cartooners") != std::string::npos)
        return BankDetection(73, 0.85f, "Cartooners (EA)");

    // ========== BANK 76: DMX Strife ==========
    if (lower.find("strife") != std::string::npos)
        return BankDetection(76, 0.85f, "Strife (DMX)");

    // No filename match
    return BankDetection(-1, 0.0f, "");
}

BankDetection BankDetector::detect_from_extension(const std::string &ext)
{
    // Extension-based defaults (medium confidence)
    if (ext == ".mus")
        return BankDetection(16, 0.75f, "MUS format (DMX bank)");

    if (ext == ".xmi")
        return BankDetection(0, 0.70f, "XMI format (AIL bank)");

    if (ext == ".imf" || ext == ".wlf")
        return BankDetection(44, 0.75f, "IMF format (Apogee bank)");

    if (ext == ".hmp" || ext == ".hmi")
        return BankDetection(2, 0.65f, "HMP/HMI format (HMI bank default)");

    if (ext == ".mid" || ext == ".midi" || ext == ".rmi")
        return BankDetection(58, 0.50f, "MIDI format (General MIDI)");

    // Unknown extension
    return BankDetection(58, 0.30f, "Unknown format (General MIDI default)");
}

BankDetection BankDetector::detect(const char *filepath)
{
    std::string path_str(filepath);
    std::string filename = get_filename(path_str);
    std::string ext = get_extension(path_str);

    // Strategy 1: HMP TIMB chunk parsing (highest confidence)
    if (ext == ".hmp" || ext == ".hmi")
    {
        BankDetection timb_result = detect_from_hmp_timb(filepath);
        if (timb_result.bank_id >= 0)
            return BankDetection(timb_result.bank_id, 1.0f, "TIMB chunk in HMP file");
    }

    // Strategy 2: Filename-based detection (high confidence)
    BankDetection filename_result = detect_from_filename(filename);
    if (filename_result.bank_id >= 0)
        return filename_result;

    // Strategy 3: Extension-based detection (medium confidence)
    BankDetection ext_result = detect_from_extension(ext);
    return ext_result;
}
