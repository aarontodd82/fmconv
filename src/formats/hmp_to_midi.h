/*
 * HMP to Standard MIDI File Converter
 * Converts HMP format to Standard MIDI File (SMF) format
 *
 * This allows HMP files to be processed by libADLMIDI via adl_openData()
 */

#ifndef HMP_TO_MIDI_H
#define HMP_TO_MIDI_H

#include <stdint.h>
#include <vector>
#include <string>

/**
 * Convert HMP file to Standard MIDI File format
 *
 * @param hmp_data Input HMP file data
 * @param hmp_size Size of HMP data
 * @param midi_out Output buffer for MIDI data
 * @param error_msg Error message if conversion fails
 * @return true on success, false on error
 */
bool convertHMPtoMIDI(const uint8_t *hmp_data, size_t hmp_size,
                      std::vector<uint8_t> &midi_out,
                      std::string &error_msg);

/**
 * Load HMP file and convert to MIDI
 *
 * @param filepath Path to HMP file
 * @param midi_out Output buffer for MIDI data
 * @param error_msg Error message if conversion fails
 * @return true on success, false on error
 */
bool loadHMPasMIDI(const char *filepath,
                   std::vector<uint8_t> &midi_out,
                   std::string &error_msg);

#endif // HMP_TO_MIDI_H
