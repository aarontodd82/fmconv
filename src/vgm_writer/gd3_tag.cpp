/*
 * GD3 Tag - VGM metadata encoder
 * Implementation
 */

#include "gd3_tag.h"
#include <codecvt>
#include <locale>
#include <cstring>

std::vector<uint16_t> GD3Tag::utf8_to_utf16(const std::string &utf8)
{
    std::vector<uint16_t> result;

    if (utf8.empty())
        return result;

    try
    {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
        std::u16string u16str = converter.from_bytes(utf8);

        result.reserve(u16str.size());
        for (char16_t c : u16str)
        {
            result.push_back(static_cast<uint16_t>(c));
        }
    }
    catch (...)
    {
        // If conversion fails, return empty vector
        result.clear();
    }

    return result;
}

std::vector<uint8_t> GD3Tag::serialize() const
{
    std::vector<uint8_t> buffer;

    // GD3 header magic: "Gd3 " (0x20336447 in little-endian)
    const char magic[] = "Gd3 ";
    buffer.insert(buffer.end(), magic, magic + 4);

    // Version: 1.00 (0x00000100)
    const uint8_t version[] = {0x00, 0x01, 0x00, 0x00};
    buffer.insert(buffer.end(), version, version + 4);

    // Length placeholder (will be filled later)
    size_t length_offset = buffer.size();
    buffer.insert(buffer.end(), 4, 0);

    // Encode all strings in order
    const std::string *strings[] = {
        &title_en, &title,
        &album_en, &album,
        &system_en, &system,
        &author_en, &author,
        &date,
        &converted_by,
        &notes
    };

    for (const std::string *str : strings)
    {
        if (str && !str->empty())
        {
            std::vector<uint16_t> u16 = utf8_to_utf16(*str);

            // Write UTF-16LE characters
            for (uint16_t c : u16)
            {
                buffer.push_back(c & 0xFF);         // Low byte
                buffer.push_back((c >> 8) & 0xFF);  // High byte
            }
        }

        // Null terminator (2 bytes for UTF-16)
        buffer.push_back(0x00);
        buffer.push_back(0x00);
    }

    // Update length field (data size after length field)
    uint32_t data_length = buffer.size() - (length_offset + 4);
    buffer[length_offset + 0] = (data_length >> 0) & 0xFF;
    buffer[length_offset + 1] = (data_length >> 8) & 0xFF;
    buffer[length_offset + 2] = (data_length >> 16) & 0xFF;
    buffer[length_offset + 3] = (data_length >> 24) & 0xFF;

    return buffer;
}
