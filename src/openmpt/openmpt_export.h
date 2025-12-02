/*
 * OpenMPT Export - C API for OPL register capture and sample audio export
 *
 * This header provides a simple C interface to the OpenMPT-based
 * tracker file converter. It can:
 * - Load S3M/MOD/XM/IT and other tracker formats
 * - Detect if file has OPL instruments and/or sample instruments
 * - Export OPL register writes to VGM format
 * - Export sample audio to PCM (with OPL muted)
 */

#ifndef OPENMPT_EXPORT_H
#define OPENMPT_EXPORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context handle
typedef struct OpenmptExportContext openmpt_export_context;

// Create/destroy context
openmpt_export_context* openmpt_export_create(void);
void openmpt_export_destroy(openmpt_export_context* ctx);

// Load a tracker file (S3M, MOD, XM, IT, etc.)
// Returns 1 on success, 0 on failure
int openmpt_export_load(openmpt_export_context* ctx, const char* filepath);

// Check what the file contains
int openmpt_export_has_opl(openmpt_export_context* ctx);
int openmpt_export_has_samples(openmpt_export_context* ctx);

// Get module info
const char* openmpt_export_get_title(openmpt_export_context* ctx);
const char* openmpt_export_get_format(openmpt_export_context* ctx);
const char* openmpt_export_get_error(openmpt_export_context* ctx);

// Render OPL instruments to VGM
// This captures OPL register writes during playback
// Returns 1 on success, 0 on failure
int openmpt_export_render_opl(openmpt_export_context* ctx,
                               uint32_t sample_rate,
                               int max_seconds);

// Render sample-based instruments to PCM
// This renders audio with OPL muted
// Returns 1 on success, 0 on failure
int openmpt_export_render_samples(openmpt_export_context* ctx,
                                   uint32_t sample_rate,
                                   int max_seconds);

// Get rendered VGM data
// Returns pointer to data and sets size, or NULL if no data
const uint8_t* openmpt_export_get_vgm_data(openmpt_export_context* ctx, size_t* size);

// Get rendered PCM data (stereo interleaved: L, R, L, R, ...)
// Returns pointer to data and sets size (number of samples, not bytes), or NULL if no data
const int16_t* openmpt_export_get_pcm_data(openmpt_export_context* ctx, size_t* size);

// Get sample rate used for rendering
uint32_t openmpt_export_get_sample_rate(openmpt_export_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // OPENMPT_EXPORT_H
