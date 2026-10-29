#include "sb_audio_triangle.h"
#include "sb_audio_tone.h"
#include <math.h>

#define STOP_FRAMES 480u
static const sb_envelope_settings_t defaults = {0.01, 0.10, 0.7, 0.15};
static bool valid_pitch(double frequency) {
    return isfinite(frequency) && frequency >= 220.0 && frequency <= 660.0;
}
bool sb_audio_triangle_configure(sb_audio_triangle_t *v, const sb_envelope_settings_t *s) {
    if (!v || !sb_envelope_settings_valid(s)) return false;
    v->settings = *s;
    v->configured = true;
    return true;
}
bool sb_audio_triangle_start(sb_audio_triangle_t *v, double frequency) {
    if (!v || !valid_pitch(frequency)) return false;
    const sb_envelope_settings_t *s = v->configured ? &v->settings : &defaults;
    if (!sb_envelope_retrigger(&v->envelope, s)) return false;
    if (!v->active) v->phase = 0.25;
    v->frequency = frequency;
    v->active = true;
    v->stopping = false; /* A new note also replaces an in-progress short stop. */
    v->stop_left = 0;
    return true;
}
bool sb_audio_triangle_pitch(sb_audio_triangle_t *v, double frequency) {
    if (!v || !valid_pitch(frequency) || !v->active) return false;
    v->frequency = frequency;
    return true;
}
void sb_audio_triangle_note_off(sb_audio_triangle_t *v) {
    if (!v || !v->active || v->stopping) return;
    sb_envelope_note_off(&v->envelope);
    v->active = v->envelope.stage != SB_ENVELOPE_IDLE;
}
void sb_audio_triangle_stop(sb_audio_triangle_t *v) {
    if (!v || !v->active || v->stopping) return;
    v->stopping = true;
    v->stop_start = v->envelope.level;
    v->stop_left = STOP_FRAMES;
}
void sb_audio_triangle_render(sb_audio_triangle_t *v, float *output, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        float sample = 0;
        if (v->active) {
            double triangle = 1.0 - 4.0 * fabs(v->phase - 0.5);
            /* Use the interval's starting level, then advance for the next
               sample. Controls see exactly that next-sample boundary level. */
            sample = (float)(0.05 * v->envelope.level * triangle);
            v->phase += v->frequency / SB_AUDIO_RATE;
            if (v->phase >= 1.0) v->phase -= 1.0;
            if (v->stopping) {
                --v->stop_left;
                v->envelope.level = v->stop_start * v->stop_left / STOP_FRAMES;
                if (v->stop_left == 0) {
                    sb_envelope_reset(&v->envelope);
                    v->active = v->stopping = false;
                }
            } else {
                sb_envelope_advance(&v->envelope);
                v->active = v->envelope.stage != SB_ENVELOPE_IDLE;
            }
        }
        output[2*i] = output[2*i+1] = sample;
    }
}
