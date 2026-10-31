/* Standalone fixed-width PolyBLEP pulse. No envelope, gain, or DC removal. */
#ifndef SB_AUDIO_PULSE_H
#define SB_AUDIO_PULSE_H
#include <stdbool.h>
#include <stddef.h>
#define SB_AUDIO_PULSE_RATE 48000u
#define SB_AUDIO_PULSE_DEFAULT_WIDTH 0.5

typedef struct {
    double phase, frequency, width;
} sb_audio_pulse_t;

/* Reset at the rising edge (phase 0); first corrected sample is zero.
 * Pitch: 220..660 Hz. Width: high fraction 0.05..0.95, default macro above.
 * Width is fixed until reset; no live width setter. Zero-initialized storage
 * is unconfigured. Reset is not an audible fade. Invalid requests fail unchanged. */
bool sb_audio_pulse_reset(sb_audio_pulse_t *pulse, double frequency, double width);
/* Change pitch without resetting phase or width; configured state required. */
bool sb_audio_pulse_pitch(sb_audio_pulse_t *pulse, double frequency);
/* Replace mono output, then advance once per sample. Values stay within ±1
 * without clipping. Expected cycle mean is 2*width-1: DC is intentionally kept.
 * PolyBLEP suppresses aliasing, not an alias-free guarantee. Valid configured
 * state required even for zero samples (which permits NULL output).
 * Invalid arguments/state or overflowing counts leave state/output unchanged.
 * Caller owns synchronization and buffer capacity; output cannot overlap state. */
bool sb_audio_pulse_render(sb_audio_pulse_t *pulse, float *output, size_t samples);
#endif
