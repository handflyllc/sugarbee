/* Standalone internal ADSR at the engine's 48 kHz audio rate; no SDL/Lua. */
#ifndef SB_AUDIO_ENVELOPE_H
#define SB_AUDIO_ENVELOPE_H
#include <stdbool.h>
#include <stdint.h>
#define SB_ENVELOPE_SAMPLE_RATE 48000u

typedef struct {
    double attack, decay, sustain, release;
} sb_envelope_settings_t;

typedef enum {
    SB_ENVELOPE_IDLE = 0, SB_ENVELOPE_ATTACK, SB_ENVELOPE_DECAY,
    SB_ENVELOPE_SUSTAIN, SB_ENVELOPE_RELEASE
} sb_envelope_stage_t;

typedef struct {
    sb_envelope_settings_t settings; /* Captured by value at start. */
    uint32_t attack_frames, decay_frames, release_frames, elapsed;
    double level, release_start;
    sb_envelope_stage_t stage;
} sb_envelope_t;

/* Zero-initialization is an idle envelope. Reset also cancels immediately;
 * it is initialization/cleanup, not the future audible stop fade. */
void sb_envelope_reset(sb_envelope_t *envelope);
/* Start only from idle. Active starts (retrigger), invalid settings, or NULL
 * return false without changing state. Times: finite 0..10 seconds; sustain:
 * finite 0..1. Positive times round to nearest frame (half up), minimum one.
 * Zero-duration stages transition immediately, including at start/note-off. */
bool sb_envelope_start(sb_envelope_t *envelope, const sb_envelope_settings_t *settings);
/* Release from current level using captured release time. Repeated calls
 * during release and calls while idle do nothing. */
void sb_envelope_note_off(sb_envelope_t *envelope);
/* Advance exactly one audio-frame interval and return its ending level.
 * Initial level is available in envelope->level before the first advance.
 * On a boundary, zero-duration following stages settle immediately too.
 * No allocation/IO/locking: caller owns synchronization when integrated. */
double sb_envelope_advance(sb_envelope_t *envelope);
#endif
