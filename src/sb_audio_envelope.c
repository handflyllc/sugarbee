#include "sb_audio_envelope.h"
#include <math.h>

static bool in_range(double value, double maximum) {
    return isfinite(value) && value >= 0.0 && value <= maximum;
}
static uint32_t duration_frames(double seconds) {
    if (seconds == 0.0) return 0;
    uint32_t frames = (uint32_t)(seconds * SB_ENVELOPE_SAMPLE_RATE + 0.5);
    return frames ? frames : 1;
}
/* At most attack -> decay -> sustain; no unbounded transition loop. */
static void settle_zero_stages(sb_envelope_t *e) {
    if (e->stage == SB_ENVELOPE_ATTACK && e->attack_frames == 0) {
        e->level = 1.0;
        e->stage = SB_ENVELOPE_DECAY;
    }
    if (e->stage == SB_ENVELOPE_DECAY && e->decay_frames == 0) {
        e->level = e->settings.sustain;
        e->stage = SB_ENVELOPE_SUSTAIN;
    }
}
void sb_envelope_reset(sb_envelope_t *e) {
    if (e) *e = (sb_envelope_t){0};
}
bool sb_envelope_settings_valid(const sb_envelope_settings_t *s) {
    return s && in_range(s->attack, 10.0) && in_range(s->decay, 10.0) &&
           in_range(s->release, 10.0) && in_range(s->sustain, 1.0);
}
bool sb_envelope_start(sb_envelope_t *e, const sb_envelope_settings_t *s) {
    if (!e || e->stage != SB_ENVELOPE_IDLE) return false;
    return sb_envelope_retrigger(e, s);
}
bool sb_envelope_retrigger(sb_envelope_t *e, const sb_envelope_settings_t *s) {
    if (!e || !sb_envelope_settings_valid(s)) return false;
    /* Build before assignment, so even settings stored in e can be captured. */
    sb_envelope_t next = {0};
    next.settings = *s;
    next.level = next.attack_start = e->stage == SB_ENVELOPE_IDLE ? 0.0 : e->level;
    next.attack_frames = duration_frames(s->attack);
    next.decay_frames = duration_frames(s->decay);
    next.release_frames = duration_frames(s->release);
    next.stage = SB_ENVELOPE_ATTACK;
    settle_zero_stages(&next);
    *e = next;
    return true;
}
void sb_envelope_note_off(sb_envelope_t *e) {
    if (!e || e->stage == SB_ENVELOPE_IDLE || e->stage == SB_ENVELOPE_RELEASE) return;
    e->release_start = e->level;
    e->elapsed = 0;
    if (e->release_frames == 0) {
        e->level = 0.0;
        e->stage = SB_ENVELOPE_IDLE;
    } else {
        e->stage = SB_ENVELOPE_RELEASE;
    }
}
double sb_envelope_advance(sb_envelope_t *e) {
    if (!e) return 0.0;
    switch (e->stage) {
        case SB_ENVELOPE_ATTACK:
            ++e->elapsed;
            e->level = e->attack_start + (1.0 - e->attack_start) * e->elapsed / e->attack_frames;
            if (e->elapsed == e->attack_frames) {
                e->level = 1.0;
                e->elapsed = 0;
                e->stage = SB_ENVELOPE_DECAY;
                settle_zero_stages(e);
            }
            break;
        case SB_ENVELOPE_DECAY:
            ++e->elapsed;
            e->level = 1.0 + (e->settings.sustain - 1.0) * e->elapsed / e->decay_frames;
            if (e->elapsed == e->decay_frames) {
                e->level = e->settings.sustain;
                e->elapsed = 0;
                e->stage = SB_ENVELOPE_SUSTAIN;
            }
            break;
        case SB_ENVELOPE_RELEASE:
            ++e->elapsed;
            e->level = e->release_start * (1.0 - (double)e->elapsed / e->release_frames);
            if (e->elapsed == e->release_frames) {
                e->level = 0.0;
                e->elapsed = 0;
                e->stage = SB_ENVELOPE_IDLE;
            }
            break;
        case SB_ENVELOPE_SUSTAIN:
        case SB_ENVELOPE_IDLE:
            break;
    }
    return e->level;
}
