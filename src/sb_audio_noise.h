/* Standalone internal white-noise source. No SDL, Lua, or global random state. */
#ifndef SB_AUDIO_NOISE_H
#define SB_AUDIO_NOISE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SB_AUDIO_NOISE_DEFAULT_SEED UINT32_C(1)
typedef struct {
    uint32_t state;
} sb_audio_noise_t;

/* Restart the sequence; seed zero selects DEFAULT_SEED. Zero initialization
 * also selects that sequence on first render. No audible fade or idle state.
 * NULL returns false. State is caller-owned, including synchronization. */
bool sb_audio_noise_reset(sb_audio_noise_t *noise, uint32_t seed);
/* Replace samples mono floats. Advance once per sample; no sample-rate/pitch
 * parameter. Same seed and total sample count give the same output/state
 * regardless of buffer partitioning. Values lie strictly between -1 and +1.
 * Caller provides samples floats, not overlapping noise state. Zero samples
 * permits NULL output and leaves state unchanged. Invalid arguments return
 * false without changing state/output. No allocation or diagnostic gain. */
bool sb_audio_noise_render(sb_audio_noise_t *noise, float *output, size_t samples);
#endif
