/* Standalone PolyBLEP pulse with smoothed width. Explicit raw or DC-conditioned output; no envelope. */
#ifndef SB_AUDIO_PULSE_H
#define SB_AUDIO_PULSE_H
#include <stdbool.h>
#include <stddef.h>
#define SB_AUDIO_PULSE_RATE 48000u
#define SB_AUDIO_PULSE_DEFAULT_WIDTH 0.5
#define SB_AUDIO_PULSE_WIDTH_FRAMES 480u

typedef struct {
    double phase, frequency, width;
    double width_from, width_target;
    unsigned width_left;
    double previous_step, previous_falling_step;
} sb_audio_pulse_t;

/* Reset at the rising edge (phase 0); first corrected sample is zero.
 * Pitch: 220..660 Hz. Width: high fraction 0.05..0.95, default macro above.
 * Reset sets width immediately and cancels any ramp. Zero-initialized storage
 * is unconfigured. Reset is not an audible fade. Invalid requests fail unchanged. */
bool sb_audio_pulse_reset(sb_audio_pulse_t *pulse, double frequency, double width);
/* Change pitch without resetting phase or width; configured state required. */
bool sb_audio_pulse_pitch(sb_audio_pulse_t *pulse, double frequency);
/* Set a width target over exactly 480 rendered samples (10 ms). A different
 * target starts from current width; repeating the target keeps its deadline.
 * Does not reset phase or affect pitch. No idle/note state exists here; later
 * bank integration will decide when idle width edits should apply immediately. */
bool sb_audio_pulse_width(sb_audio_pulse_t *pulse, double width);
/* Replace mono output, then advance once per sample. Values stay within ±1
 * without clipping. Settled cycle mean is 2*width-1; raw DC is intentionally kept.
 * PolyBLEP suppresses aliasing, not an alias-free guarantee. Valid configured
 * state required even for zero samples (which permits NULL output).
 * Invalid arguments/state or overflowing counts leave state/output unchanged.
 * Caller owns synchronization and buffer capacity; output cannot overlap state. */
bool sb_audio_pulse_render(sb_audio_pulse_t *pulse, float *output, size_t samples);
/* Same rendering/state contract as raw output, with explicit conditioning:
 * 0.5 * (raw - (2*current_width - 1)), using this sample's smoothed width.
 * Removes the settled expected DC; short transition windows need not average
 * zero. Fixed gain halves square amplitude; no loudness normalization or clamp.
 * Current width bounds give output within +/-0.95. Both render variants advance
 * the same state; use copies to compare the same samples, not consecutive calls.
 * Reset's first conditioned sample is 0.5-width, not necessarily zero. */
bool sb_audio_pulse_render_conditioned(sb_audio_pulse_t *pulse, float *output, size_t samples);
#endif
