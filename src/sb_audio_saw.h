/* Standalone internal PolyBLEP saw; no envelope, SDL, Lua, or diagnostic gain. */
#ifndef SB_AUDIO_SAW_H
#define SB_AUDIO_SAW_H
#include <stdbool.h>
#include <stddef.h>
#define SB_AUDIO_SAW_RATE 48000u

typedef struct {
    double phase, frequency;
} sb_audio_saw_t;

/* Reset to rising zero crossing (phase 0.5), with pitch 27.5..4186.01 Hz inclusive.
 * Pitch edits preserve phase. Invalid/nonfinite requests return false unchanged.
 * Zero-initialized storage is unconfigured; reset is required before rendering.
 * Reset is not a fade; audible note lifecycle belongs to later integration. */
bool sb_audio_saw_reset(sb_audio_saw_t *saw, double frequency);
bool sb_audio_saw_pitch(sb_audio_saw_t *saw, double frequency);
/* Replace mono output with bounded normalized samples; render current phase,
 * then advance. No clamp. PolyBLEP reduces aliasing, not an alias-free promise.
 * Zero samples permits NULL output, but still requires a configured oscillator.
 * Invalid pointers/count/state fail without changing output or oscillator.
 * Caller owns synchronization, buffer capacity, and non-overlapping storage. */
bool sb_audio_saw_render(sb_audio_saw_t *saw, float *output, size_t samples);
#endif
