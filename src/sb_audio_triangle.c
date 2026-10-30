#include "sb_audio_triangle.h"
#include "sb_audio_tone.h"
#include <math.h>

#define STOP_FRAMES 480u
static const sb_envelope_settings_t defaults = {0.01, 0.10, 0.7, 0.15};
#define CONTROL_FRAMES 480u
static void initialize_controls(sb_audio_triangle_t *v) {
    if (v->controls_initialized) return;
    v->volume = v->volume_target = 0.5;
    v->pan = v->pan_target = 0.0;
    v->controls_initialized = true;
}
static void advance_control(double *value, double from, double target, unsigned *left) {
    if (!*left) return;
    --*left;
    *value = *left ? from + (target-from)*(CONTROL_FRAMES-*left)/CONTROL_FRAMES : target;
}
static bool set_control(sb_audio_triangle_t *v, double target, bool pan) {
    if (!v || !isfinite(target) || target < (pan ? -1.0 : 0.0) || target > 1.0)
        return false;
    initialize_controls(v);
    double *value = pan ? &v->pan : &v->volume;
    double *from = pan ? &v->pan_from : &v->volume_from;
    double *destination = pan ? &v->pan_target : &v->volume_target;
    unsigned *left = pan ? &v->pan_left : &v->volume_left;
    if (!v->active) { *value = *destination = target; *left = 0; }
    else if (*destination != target) {
        *from = *value;
        *destination = target;
        *left = CONTROL_FRAMES;
    }
    return true;
}
bool sb_audio_triangle_volume(sb_audio_triangle_t *v, double volume) {
    return set_control(v, volume, false);
}
bool sb_audio_triangle_pan(sb_audio_triangle_t *v, double pan) {
    return set_control(v, pan, true);
}
static void pan_gains(double pan, double *left, double *right) {
    if (pan == -1) { *left = 1; *right = 0; }
    else if (pan == 1) { *left = 0; *right = 1; }
    else if (pan == 0) { *left = *right = 0.7071067811865475244; }
    else {
        double angle = (pan+1.0) * 0.7853981633974483096;
        *left = cos(angle); *right = sin(angle);
    }
}
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
    initialize_controls(v);
    if (!v->active) {
        v->phase = 0.25;
        /* A completed note has no audible ramp left to preserve. */
        v->volume = v->volume_target; v->volume_left = 0;
        v->pan = v->pan_target; v->pan_left = 0;
    }
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
    initialize_controls(v);
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
        double left, right;
        pan_gains(v->pan, &left, &right);
        output[2*i] = (float)(sample * v->volume * left);
        output[2*i+1] = (float)(sample * v->volume * right);
        advance_control(&v->volume, v->volume_from, v->volume_target, &v->volume_left);
        advance_control(&v->pan, v->pan_from, v->pan_target, &v->pan_left);
    }
}
