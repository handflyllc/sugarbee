#ifndef SB_WAV_H
#define SB_WAV_H
#include <stddef.h>
#include <stdint.h>
#define SB_WAV_SOURCE_LIMIT (16u*1024u*1024u)
#define SB_WAV_ASSET_LIMIT (4u*1024u*1024u)
#define SB_WAV_TOTAL_LIMIT (32u*1024u*1024u)
#define SB_WAV_ASSETS 64u
#define SB_WAV_PLAYBACKS 8u
#define SB_WAV_RATE 48000u
/* IDs are context-local, nonzero, never recycled during a device's lifetime. */
typedef uint64_t sb_wav_handle_t;
typedef enum {
    SB_WAV_OK, SB_WAV_INVALID, SB_WAV_UNSUPPORTED, SB_WAV_TOO_LARGE,
    SB_WAV_NO_MEMORY, SB_WAV_BUDGET, SB_WAV_ASSETS_FULL, SB_WAV_VOICES_FULL,
    SB_WAV_BAD_HANDLE, SB_WAV_IN_USE, SB_WAV_UNAVAILABLE, SB_WAV_REJECTED,
    SB_WAV_IDS_EXHAUSTED
} sb_wav_result_t;
const char *sb_wav_result_string(sb_wav_result_t result);
/* Standalone decoder, usable without an audio device. Input is borrowed.
 * Output must be empty; on failure remains empty. Caller frees samples via
 * sb_wav_free. 48 kHz interleaved floats, preserving mono/stereo channels. */
typedef struct { float *samples; size_t frames; unsigned channels; } sb_wav_data_t;
sb_wav_result_t sb_wav_decode(const void *bytes, size_t length, sb_wav_data_t *out);
void sb_wav_free(sb_wav_data_t *data);
#endif
