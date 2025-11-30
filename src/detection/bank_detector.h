/*
 * Bank Detector - Intelligent bank selection with confidence scoring
 * Automatically detects OPL3 bank from filename, format, and file content
 */

#ifndef BANK_DETECTOR_H
#define BANK_DETECTOR_H

#include <string>
#include <stdint.h>

struct BankDetection
{
    int bank_id;        // Detected bank (0-78)
    float confidence;   // Confidence level (0.0 = unknown, 1.0 = certain)
    std::string reason; // Why this bank was chosen

    BankDetection(int id = 58, float conf = 0.3f, const std::string &r = "Unknown")
        : bank_id(id), confidence(conf), reason(r) {}
};

class BankDetector
{
public:
    // Detect bank from filename and file extension
    static BankDetection detect(const char *filepath);

private:
    // Helper functions
    static std::string to_lowercase(const std::string &str);
    static std::string get_extension(const std::string &path);
    static std::string get_filename(const std::string &path);

    // Detection strategies
    static BankDetection detect_from_hmp_timb(const char *filepath);
    static BankDetection detect_from_filename(const std::string &filename);
    static BankDetection detect_from_extension(const std::string &ext);
};

#endif // BANK_DETECTOR_H
