/*
 * GD3 Tag - VGM metadata encoder
 * Encodes song information as UTF-16LE strings
 */

#ifndef GD3_TAG_H
#define GD3_TAG_H

#include <string>
#include <vector>
#include <stdint.h>

struct GD3Tag
{
    std::string title_en;      // Track title (English)
    std::string title;         // Track title (native language)
    std::string album_en;      // Album/game name (English)
    std::string album;         // Album/game name (native)
    std::string system_en;     // System name (English)
    std::string system;        // System name (native)
    std::string author_en;     // Composer (English)
    std::string author;        // Composer (native)
    std::string date;          // Release date
    std::string converted_by;  // Converter info
    std::string notes;         // Additional notes

    // Serialize to VGM GD3 format
    std::vector<uint8_t> serialize() const;

    // Parse GD3 tag from raw data (starting at "Gd3 " magic)
    // Returns true on success
    bool parse(const uint8_t* data, size_t size);

private:
    // Convert UTF-8 to UTF-16LE
    static std::vector<uint16_t> utf8_to_utf16(const std::string &utf8);

    // Convert UTF-16LE to UTF-8
    static std::string utf16_to_utf8(const uint16_t* data, size_t len);
};

#endif // GD3_TAG_H
