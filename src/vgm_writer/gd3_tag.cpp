/*
 * GD3 Tag - VGM metadata encoder
 * Implementation
 */

#include "gd3_tag.h"
#include <codecvt>
#include <locale>
#include <cstring>

std::string GD3Tag::utf16_to_utf8(const uint16_t* data, size_t len)
{
    if (!data || len == 0)
        return "";

    try
    {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
        std::u16string u16str;
        u16str.reserve(len);
        for (size_t i = 0; i < len; i++)
        {
            u16str.push_back(static_cast<char16_t>(data[i]));
        }
        return converter.to_bytes(u16str);
    }
    catch (...)
    {
        return "";
    }
}

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

bool GD3Tag::parse(const uint8_t* data, size_t size)
{
    // Need at least: magic (4) + version (4) + length (4) = 12 bytes
    if (!data || size < 12)
        return false;

    // Check magic "Gd3 "
    if (data[0] != 'G' || data[1] != 'd' || data[2] != '3' || data[3] != ' ')
        return false;

    // Read data length (after the 12-byte header)
    uint32_t data_length = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);

    if (size < 12 + data_length)
        return false;

    // Parse the UTF-16LE strings
    const uint8_t* str_data = data + 12;
    size_t remaining = data_length;

    std::string* fields[] = {
        &title_en, &title,
        &album_en, &album,
        &system_en, &system,
        &author_en, &author,
        &date,
        &converted_by,
        &notes
    };

    for (std::string* field : fields)
    {
        if (remaining < 2)
            break;

        // Find null terminator (two zero bytes)
        std::vector<uint16_t> chars;
        while (remaining >= 2)
        {
            uint16_t ch = str_data[0] | (str_data[1] << 8);
            str_data += 2;
            remaining -= 2;

            if (ch == 0)
                break;

            chars.push_back(ch);
        }

        if (!chars.empty())
        {
            *field = utf16_to_utf8(chars.data(), chars.size());
        }
        else
        {
            field->clear();
        }
    }

    return true;
}
