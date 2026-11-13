/* Shared pitched-source contract. Frequencies are Hz, never snapped or clamped. */
#ifndef SB_AUDIO_PITCH_H
#define SB_AUDIO_PITCH_H
#include <stdbool.h>
#include <math.h>
#define SB_AUDIO_PITCH_MIN 27.5
/* Accept exact equal-tempered C8 and C8 rounded to two decimal places. */
#define SB_AUDIO_PITCH_MAX 4186.01
static inline bool sb_audio_pitch_valid(double hz) {
    return isfinite(hz) && hz >= SB_AUDIO_PITCH_MIN && hz <= SB_AUDIO_PITCH_MAX;
}
#endif
