/* Private one-pole low-pass for the native bank, fixed 48 kHz sample rate. */
#ifndef SB_AUDIO_FILTER_H
#define SB_AUDIO_FILTER_H
#include <stdbool.h>
#define SB_AUDIO_FILTER_FRAMES 480u
typedef struct {
    bool initialized;
    double cutoff, state;
    double coefficient, coefficient_from, coefficient_target;
    double wet, wet_from, wet_target;
    unsigned coefficient_left, wet_left;
} sb_audio_filter_t;
/* Zero storage means default cutoff 2000 Hz, bypassed. Finite 20..20000 Hz.
 * Idle edits apply immediately; live coefficient/bypass ramps last 480 samples.
 * Same targets keep deadlines; retargets start at current values. */
bool sb_audio_filter_cutoff(sb_audio_filter_t *filter, double cutoff, bool active);
bool sb_audio_filter_bypass(sb_audio_filter_t *filter, bool bypass, bool active);
/* Fresh note only: clear history, settle controls at retained targets. */
void sb_audio_filter_start(sb_audio_filter_t *filter);
/* Initialized state and finite normalized input required. Updates history even
 * when bypassed; exact dry output at wet=0. No allocation or transcendental work. */
double sb_audio_filter_sample(sb_audio_filter_t *filter, double input);
#endif
