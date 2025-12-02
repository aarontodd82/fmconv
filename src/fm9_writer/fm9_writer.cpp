/*
 * FM9 Writer implementation
 */

#include "fm9_writer.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <climits>

// stb_image for loading PNG, JPEG, GIF
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_NO_FAILURE_STRINGS
#include "../stb_image.h"

#ifdef HAVE_MINIZ
#include "../miniz.h"
#elif defined(HAVE_ZLIB)
#include <zlib.h>
#endif

// Helper to get lowercase extension
static std::string getExtensionLower(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// Gzip compression (same as unified_converter.cpp)
static std::vector<uint8_t> gzipCompress(const std::vector<uint8_t>& data) {
#if defined(HAVE_MINIZ) || defined(HAVE_ZLIB)
    if (data.empty()) return {};

    mz_ulong crc = mz_crc32(MZ_CRC32_INIT, data.data(), data.size());

    mz_ulong compressed_bound = mz_compressBound(static_cast<mz_ulong>(data.size()));
    std::vector<uint8_t> deflate_data(compressed_bound);

    mz_stream strm = {};
    strm.next_in = data.data();
    strm.avail_in = static_cast<unsigned int>(data.size());
    strm.next_out = deflate_data.data();
    strm.avail_out = static_cast<unsigned int>(deflate_data.size());

    int ret = mz_deflateInit2(&strm, MZ_DEFAULT_COMPRESSION, MZ_DEFLATED,
                               -MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY);
    if (ret != MZ_OK) return {};

    ret = mz_deflate(&strm, MZ_FINISH);
    mz_ulong deflate_size = strm.total_out;
    mz_deflateEnd(&strm);

    if (ret != MZ_STREAM_END) return {};

    std::vector<uint8_t> gzip_data;
    gzip_data.reserve(10 + deflate_size + 8);

    // Gzip header
    gzip_data.push_back(0x1f);
    gzip_data.push_back(0x8b);
    gzip_data.push_back(0x08);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0x00);
    gzip_data.push_back(0xff);

    // Deflate data
    gzip_data.insert(gzip_data.end(), deflate_data.begin(),
                     deflate_data.begin() + deflate_size);

    // Gzip trailer
    gzip_data.push_back(static_cast<uint8_t>(crc & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((crc >> 8) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((crc >> 16) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((crc >> 24) & 0xff));

    uint32_t orig_size = static_cast<uint32_t>(data.size());
    gzip_data.push_back(static_cast<uint8_t>(orig_size & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((orig_size >> 8) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((orig_size >> 16) & 0xff));
    gzip_data.push_back(static_cast<uint8_t>((orig_size >> 24) & 0xff));

    return gzip_data;
#else
    (void)data;
    return {};
#endif
}

FM9Writer::FM9Writer() = default;
FM9Writer::~FM9Writer() = default;

void FM9Writer::setVGMData(const std::vector<uint8_t>& vgm_data) {
    vgm_data_ = vgm_data;
}

bool FM9Writer::loadFile(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_ = "Failed to open file: " + path;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        error_ = "Failed to read file: " + path;
        return false;
    }

    return true;
}

uint8_t FM9Writer::detectAudioFormat(const std::string& path) {
    std::string ext = getExtensionLower(path);

    if (ext == "wav" || ext == "wave") {
        return FM9_AUDIO_WAV;
    }
    if (ext == "mp3") {
        return FM9_AUDIO_MP3;
    }

    // Try magic bytes
    std::ifstream file(path, std::ios::binary);
    if (!file) return FM9_AUDIO_NONE;

    uint8_t magic[4];
    file.read(reinterpret_cast<char*>(magic), 4);

    // WAV: "RIFF"
    if (magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F') {
        return FM9_AUDIO_WAV;
    }

    // MP3: ID3 tag or frame sync
    if ((magic[0] == 'I' && magic[1] == 'D' && magic[2] == '3') ||
        (magic[0] == 0xFF && (magic[1] & 0xE0) == 0xE0)) {
        return FM9_AUDIO_MP3;
    }

    return FM9_AUDIO_NONE;
}

void FM9Writer::setSourceFormat(const std::string& extension) {
    source_format_ = extensionToSourceFormat(extension.c_str());
}

bool FM9Writer::setAudioFile(const std::string& path) {
    audio_format_ = detectAudioFormat(path);
    if (audio_format_ == FM9_AUDIO_NONE) {
        error_ = "Unsupported audio format (use WAV or MP3): " + path;
        return false;
    }

    if (!loadFile(path, audio_data_)) {
        audio_format_ = FM9_AUDIO_NONE;
        return false;
    }

    return true;
}

void FM9Writer::setAudioData(const std::vector<uint8_t>& data, uint8_t format) {
    audio_data_ = data;
    audio_format_ = format;
}

bool FM9Writer::setFXFile(const std::string& path) {
    if (!loadFile(path, fx_data_)) {
        return false;
    }

    // Basic JSON validation - check for opening brace
    bool found_brace = false;
    for (uint8_t c : fx_data_) {
        if (c == '{') {
            found_brace = true;
            break;
        }
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
    }

    if (!found_brace) {
        error_ = "FX file does not appear to be valid JSON: " + path;
        fx_data_.clear();
        return false;
    }

    return true;
}

bool FM9Writer::setImageFile(const std::string& path, bool dither) {
    // Check file size first (reject >10MB)
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_ = "Failed to open image file: " + path;
        return false;
    }

    std::streamsize file_size = file.tellg();
    file.close();

    if (file_size > 10 * 1024 * 1024) {
        error_ = "Image file too large (max 10MB): " + path;
        return false;
    }

    // Load image with stb_image
    int width, height, channels;
    uint8_t* pixels = stbi_load(path.c_str(), &width, &height, &channels, 3);  // Force RGB

    if (!pixels) {
        error_ = "Failed to load image (unsupported format or corrupt file): " + path;
        return false;
    }

    // Check dimensions (reject >4096x4096)
    if (width > 4096 || height > 4096) {
        stbi_image_free(pixels);
        error_ = "Image too large (max 4096x4096): " + path;
        return false;
    }

    // Process the image
    bool success = processImage(pixels, width, height, dither);
    stbi_image_free(pixels);

    return success;
}

uint16_t FM9Writer::rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void FM9Writer::scaleImage(const uint8_t* src, int src_w, int src_h,
                           uint8_t* dst, int dst_w, int dst_h, int dst_x, int dst_y) {
    // Bilinear interpolation scaling
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            // Map destination pixel to source coordinates
            float src_xf = (x + 0.5f) * src_w / dst_w - 0.5f;
            float src_yf = (y + 0.5f) * src_h / dst_h - 0.5f;

            int x0 = static_cast<int>(std::floor(src_xf));
            int y0 = static_cast<int>(std::floor(src_yf));
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            // Clamp to source bounds
            x0 = std::max(0, std::min(x0, src_w - 1));
            x1 = std::max(0, std::min(x1, src_w - 1));
            y0 = std::max(0, std::min(y0, src_h - 1));
            y1 = std::max(0, std::min(y1, src_h - 1));

            float fx = src_xf - std::floor(src_xf);
            float fy = src_yf - std::floor(src_yf);

            // Bilinear interpolation for each channel
            for (int c = 0; c < 3; c++) {
                float v00 = src[(y0 * src_w + x0) * 3 + c];
                float v10 = src[(y0 * src_w + x1) * 3 + c];
                float v01 = src[(y1 * src_w + x0) * 3 + c];
                float v11 = src[(y1 * src_w + x1) * 3 + c];

                float v0 = v00 * (1 - fx) + v10 * fx;
                float v1 = v01 * (1 - fx) + v11 * fx;
                float v = v0 * (1 - fy) + v1 * fy;

                int dst_idx = ((dst_y + y) * FM9_IMAGE_WIDTH + (dst_x + x)) * 3 + c;
                dst[dst_idx] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v)));
            }
        }
    }
}

// Bayer 4x4 ordered dithering matrix (values 0-15, normalized to threshold later)
static const int bayer4x4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

// Color structure for median cut
struct Color {
    uint8_t r, g, b;

    bool operator<(const Color& other) const {
        if (r != other.r) return r < other.r;
        if (g != other.g) return g < other.g;
        return b < other.b;
    }

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

// Box for median cut algorithm
struct ColorBox {
    std::vector<Color> colors;
    int r_min, r_max, g_min, g_max, b_min, b_max;

    void computeBounds() {
        r_min = g_min = b_min = 255;
        r_max = g_max = b_max = 0;
        for (const auto& c : colors) {
            r_min = std::min(r_min, (int)c.r);
            r_max = std::max(r_max, (int)c.r);
            g_min = std::min(g_min, (int)c.g);
            g_max = std::max(g_max, (int)c.g);
            b_min = std::min(b_min, (int)c.b);
            b_max = std::max(b_max, (int)c.b);
        }
    }

    int longestAxis() const {
        int r_range = r_max - r_min;
        int g_range = g_max - g_min;
        int b_range = b_max - b_min;
        if (r_range >= g_range && r_range >= b_range) return 0;
        if (g_range >= r_range && g_range >= b_range) return 1;
        return 2;
    }

    Color averageColor() const {
        if (colors.empty()) return {0, 0, 0};
        long r_sum = 0, g_sum = 0, b_sum = 0;
        for (const auto& c : colors) {
            r_sum += c.r;
            g_sum += c.g;
            b_sum += c.b;
        }
        return {
            static_cast<uint8_t>(r_sum / colors.size()),
            static_cast<uint8_t>(g_sum / colors.size()),
            static_cast<uint8_t>(b_sum / colors.size())
        };
    }
};

// Find nearest color in palette
static int findNearestColor(uint8_t r, uint8_t g, uint8_t b, const std::vector<Color>& palette) {
    int best_idx = 0;
    int best_dist = INT_MAX;
    for (size_t i = 0; i < palette.size(); i++) {
        int dr = (int)r - palette[i].r;
        int dg = (int)g - palette[i].g;
        int db = (int)b - palette[i].b;
        // Weighted distance (green is more perceptually important)
        int dist = dr * dr * 2 + dg * dg * 4 + db * db * 3;
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = static_cast<int>(i);
        }
    }
    return best_idx;
}

// Median cut algorithm to generate optimal 16-color palette
static std::vector<Color> medianCut(const uint8_t* pixels, int width, int height, int num_colors) {
    // Collect all unique colors (or sample if too many)
    std::vector<Color> all_colors;
    all_colors.reserve(width * height);

    for (int i = 0; i < width * height; i++) {
        Color c = {pixels[i * 3], pixels[i * 3 + 1], pixels[i * 3 + 2]};
        // Skip black pixels (letterbox background)
        if (c.r == 0 && c.g == 0 && c.b == 0) continue;
        all_colors.push_back(c);
    }

    if (all_colors.empty()) {
        // All black image - return basic palette
        std::vector<Color> palette(num_colors, {0, 0, 0});
        return palette;
    }

    // Start with one box containing all colors
    std::vector<ColorBox> boxes;
    boxes.push_back({});
    boxes[0].colors = std::move(all_colors);
    boxes[0].computeBounds();

    // Split boxes until we have num_colors
    while (boxes.size() < static_cast<size_t>(num_colors)) {
        // Find box with most colors to split
        int best_box = -1;
        size_t best_size = 1;  // Need at least 2 colors to split
        for (size_t i = 0; i < boxes.size(); i++) {
            if (boxes[i].colors.size() > best_size) {
                best_size = boxes[i].colors.size();
                best_box = static_cast<int>(i);
            }
        }

        if (best_box < 0) break;  // Can't split anymore

        ColorBox& box = boxes[best_box];
        int axis = box.longestAxis();

        // Sort by the longest axis
        if (axis == 0) {
            std::sort(box.colors.begin(), box.colors.end(),
                      [](const Color& a, const Color& b) { return a.r < b.r; });
        } else if (axis == 1) {
            std::sort(box.colors.begin(), box.colors.end(),
                      [](const Color& a, const Color& b) { return a.g < b.g; });
        } else {
            std::sort(box.colors.begin(), box.colors.end(),
                      [](const Color& a, const Color& b) { return a.b < b.b; });
        }

        // Split at median
        size_t median = box.colors.size() / 2;
        ColorBox new_box;
        new_box.colors.assign(box.colors.begin() + median, box.colors.end());
        box.colors.resize(median);

        box.computeBounds();
        new_box.computeBounds();
        boxes.push_back(std::move(new_box));
    }

    // Extract palette (average color of each box)
    std::vector<Color> palette;
    palette.reserve(num_colors);
    for (const auto& box : boxes) {
        palette.push_back(box.averageColor());
    }

    // Always include black for letterboxing
    bool has_black = false;
    for (const auto& c : palette) {
        if (c.r == 0 && c.g == 0 && c.b == 0) {
            has_black = true;
            break;
        }
    }
    if (!has_black && !palette.empty()) {
        palette[palette.size() - 1] = {0, 0, 0};
    }

    // Pad palette if needed
    while (palette.size() < static_cast<size_t>(num_colors)) {
        palette.push_back({0, 0, 0});
    }

    return palette;
}

void FM9Writer::applyDithering(uint8_t* pixels, int width, int height) {
    // Generate optimal 16-color palette using median cut
    std::vector<Color> palette = medianCut(pixels, width, height, 16);

    // Apply ordered dithering with the palette
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;

            // Get original color
            int r = pixels[idx + 0];
            int g = pixels[idx + 1];
            int b = pixels[idx + 2];

            // Get Bayer threshold, scaled to affect color matching
            // Range: -32 to +30 (subtle enough to not cause major color shifts)
            int threshold = (bayer4x4[y % 4][x % 4] - 8) * 4;

            // Apply threshold to bias color matching
            int r_biased = std::max(0, std::min(255, r + threshold));
            int g_biased = std::max(0, std::min(255, g + threshold));
            int b_biased = std::max(0, std::min(255, b + threshold));

            // Find nearest palette color
            int nearest = findNearestColor(r_biased, g_biased, b_biased, palette);

            // Write palette color
            pixels[idx + 0] = palette[nearest].r;
            pixels[idx + 1] = palette[nearest].g;
            pixels[idx + 2] = palette[nearest].b;
        }
    }
}

bool FM9Writer::processImage(const uint8_t* pixels, int width, int height, bool dither) {
    // Calculate scale factor to fit within 100x100 preserving aspect ratio
    float scale = std::min(100.0f / width, 100.0f / height);
    int scaled_width = static_cast<int>(width * scale);
    int scaled_height = static_cast<int>(height * scale);

    // Center on 100x100 canvas
    int offset_x = (FM9_IMAGE_WIDTH - scaled_width) / 2;
    int offset_y = (FM9_IMAGE_HEIGHT - scaled_height) / 2;

    // Allocate RGB888 canvas (black background)
    std::vector<uint8_t> canvas(FM9_IMAGE_WIDTH * FM9_IMAGE_HEIGHT * 3, 0);

    // Scale and place image
    scaleImage(pixels, width, height,
               canvas.data(), scaled_width, scaled_height, offset_x, offset_y);

    // Apply dithering if requested
    if (dither) {
        applyDithering(canvas.data(), FM9_IMAGE_WIDTH, FM9_IMAGE_HEIGHT);
    }

    // Convert to RGB565
    image_data_.resize(FM9_IMAGE_SIZE);
    uint16_t* rgb565 = reinterpret_cast<uint16_t*>(image_data_.data());

    for (int i = 0; i < FM9_IMAGE_WIDTH * FM9_IMAGE_HEIGHT; i++) {
        uint8_t r = canvas[i * 3 + 0];
        uint8_t g = canvas[i * 3 + 1];
        uint8_t b = canvas[i * 3 + 2];
        rgb565[i] = rgb888ToRgb565(r, g, b);
    }

    return true;
}

FM9Header FM9Writer::buildHeader() const {
    FM9Header header = {};

    header.magic[0] = 'F';
    header.magic[1] = 'M';
    header.magic[2] = '9';
    header.magic[3] = '0';

    header.version = 1;

    header.flags = 0;
    if (!audio_data_.empty()) header.flags |= FM9_FLAG_HAS_AUDIO;
    if (!fx_data_.empty()) header.flags |= FM9_FLAG_HAS_FX;
    if (!image_data_.empty()) header.flags |= FM9_FLAG_HAS_IMAGE;

    header.audio_format = audio_format_;
    header.source_format = static_cast<uint8_t>(source_format_);

    // Offsets are from start of FM9 header (within decompressed data)
    // FX data is in the compressed section, right after the header
    // Audio is OUTSIDE the compressed section (appended raw after gzip)
    header.fx_offset = sizeof(FM9Header);
    header.fx_size = static_cast<uint32_t>(fx_data_.size());

    // Audio offset is special: it indicates audio exists but actual data
    // is after the gzip section. We store the size here so Teensy knows
    // how much to copy. The actual file offset = gzip_size (determined at runtime)
    header.audio_offset = 0;  // Not used for seeking (audio is after gzip)
    header.audio_size = static_cast<uint32_t>(audio_data_.size());

    return header;
}

size_t FM9Writer::write(const std::string& output_path) {
    if (vgm_data_.empty()) {
        error_ = "No VGM data set";
        return 0;
    }

    // Build the COMPRESSED portion:
    // [VGM data] + [FM9 Header] + [FX data]
    // Audio is appended UNCOMPRESSED after the gzip data
    std::vector<uint8_t> compressed_portion;
    compressed_portion.reserve(vgm_data_.size() + sizeof(FM9Header) + fx_data_.size());

    // VGM data first
    compressed_portion.insert(compressed_portion.end(), vgm_data_.begin(), vgm_data_.end());

    // FM9 header
    FM9Header header = buildHeader();
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
    compressed_portion.insert(compressed_portion.end(), header_bytes, header_bytes + sizeof(FM9Header));

    // FX data (if any) - goes in compressed section
    if (!fx_data_.empty()) {
        compressed_portion.insert(compressed_portion.end(), fx_data_.begin(), fx_data_.end());
    }

    // Gzip compress the VGM + header + FX
    std::vector<uint8_t> compressed = gzipCompress(compressed_portion);
    if (compressed.empty()) {
        error_ = "Gzip compression failed";
        return 0;
    }

    // Write to file
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        error_ = "Failed to open output file: " + output_path;
        return 0;
    }

    // Write compressed portion (VGM + FM9 header + FX)
    out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());

    // Append raw audio data AFTER the gzip section (uncompressed)
    if (!audio_data_.empty()) {
        out.write(reinterpret_cast<const char*>(audio_data_.data()), audio_data_.size());
    }

    // Append raw image data AFTER audio (uncompressed)
    // Image offset = gzipEndOffset + audio_size (Teensy calculates this)
    if (!image_data_.empty()) {
        out.write(reinterpret_cast<const char*>(image_data_.data()), image_data_.size());
    }

    out.close();

    return compressed.size() + audio_data_.size() + image_data_.size();
}
