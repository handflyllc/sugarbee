#include "sb_audio_pitch.h"
#include "sb_music_sequence.h"
#include <math.h>
#include <string.h>

void sb_music_sequence_reset(sb_music_sequence_t *s) {
    if (!s) return;
    if (!s->shared) { memset(s, 0, sizeof(*s)); return; }
    /* Music cleanup cannot reset an unrelated effect, even if it is active. */
    sb_audio_bank_t empty={0};
    for (unsigned v=1;v<=SB_AUDIO_BANK_VOICES;++v)
        if (s->reserved & (1u<<(v-1)))
            (void)sb_audio_bank_copy_voice(&s->bank,v,&empty,v);
    sb_audio_bank_t retained=s->bank;
    memset(s,0,sizeof(*s));
    s->bank=retained;
    s->shared=true;
}

static bool configure(sb_audio_bank_t *bank, unsigned track,
                      const sb_music_instrument_t *i) {
    return sb_audio_bank_waveform(bank, track, i->waveform) &&
           sb_audio_bank_configure(bank, track, &i->envelope) &&
           sb_audio_bank_volume(bank, track, i->volume) &&
           sb_audio_bank_pan(bank, track, i->pan) &&
           (i->waveform != SB_AUDIO_VOICE_PULSE ||
            sb_audio_bank_pulse_width(bank, track, i->pulse_width)) &&
           sb_audio_bank_filter_cutoff(bank, track, i->filter_cutoff) &&
           sb_audio_bank_filter_bypass(bank, track, i->filter_bypass);
}

static bool events_valid(const sb_music_song_t *song) {
    bool held[SB_AUDIO_BANK_VOICES] = {false};
    uint64_t started[SB_AUDIO_BANK_VOICES] = {0};
    for (size_t n = 0; n < song->event_count; ++n) {
        const sb_music_event_t *e = &song->events[n];
        if (e->track < 1 || e->track > song->track_count ||
            e->frame > song->end_frame ||
            (e->kind != SB_MUSIC_NOTE_OFF && e->kind != SB_MUSIC_NOTE_ON)) return false;
        if (n) {
            const sb_music_event_t *p = &song->events[n-1];
            if (e->frame < p->frame || (e->frame == p->frame &&
                (e->kind < p->kind || (e->kind == p->kind && e->track <= p->track))))
                return false;
        }
        unsigned slot = e->track - 1;
        if (e->kind == SB_MUSIC_NOTE_OFF) {
            if (e->hz != 0 || !held[slot] || e->frame <= started[slot]) return false;
            held[slot] = false;
        } else {
            if (held[slot]) return false;
            if (song->instruments[slot].waveform == SB_AUDIO_VOICE_NOISE) {
                if (e->hz != 0) return false;
            } else if (!sb_audio_pitch_valid(e->hz)) return false;
            held[slot] = true;
            started[slot] = e->frame;
        }
    }
    for (unsigned v = 0; v < song->track_count; ++v) if (held[v]) return false;
    return true;
}

bool sb_music_sequence_start(sb_music_sequence_t *s, const sb_music_song_t *song) {
    if (!s || s->shared || s->state != SB_MUSIC_IDLE || !song ||
        song->timeline_rate != SB_MUSIC_SEQUENCE_RATE || !song->end_frame ||
        !song->track_count || song->track_count > SB_AUDIO_BANK_VOICES ||
        !song->instruments || song->event_count > SB_MUSIC_SEQUENCE_MAX_EVENTS ||
        (song->event_count && !song->events)) return false;
    /* Build privately so even a late configuration/event error is atomic. */
    sb_music_sequence_t next = {0};
    for (unsigned v = 1; v <= song->track_count; ++v)
        if (!configure(&next.bank, v, &song->instruments[v-1])) return false;
    if (!events_valid(song)) return false;
    next.gain = next.gain_from = next.gain_target = 1.0;
    next.song = song;
    next.state = SB_MUSIC_PLAYING;
    *s = next;
    return true;
}

/* Valid paired events and fixed per-track instruments are prerequisites.
 * Only the last release survives on a track: a subsequent on retriggers the
 * existing envelope, replacing any earlier tail. No sample rendering needed. */
static bool valid_song_loop_eligible(const sb_music_song_t *song) {
    uint64_t last_off[SB_AUDIO_BANK_VOICES] = {0};
    bool used[SB_AUDIO_BANK_VOICES] = {false};
    for (size_t n = 0; n < song->event_count; ++n) {
        const sb_music_event_t *e = &song->events[n];
        if (e->kind == SB_MUSIC_NOTE_OFF) {
            last_off[e->track - 1] = e->frame;
            used[e->track - 1] = true;
        }
    }
    for (unsigned n = 0; n < song->track_count; ++n) if (used[n]) {
        sb_envelope_t envelope = {0};
        /* Reuse the synth's validated conversion, including positive minimum
         * one frame and half-up rounding, without duplicating its arithmetic. */
        if (!sb_envelope_start(&envelope, &song->instruments[n].envelope)) return false;
        if (envelope.release_frames > song->end_frame - last_off[n]) return false;
    }
    return true;
}
/* Both operations validate outside rendering; no caller-provided eligibility
 * flag can bypass the core checks. One-shot start retains its existing API. */
bool sb_music_sequence_loop_eligible(const sb_music_song_t *song) {
    sb_music_sequence_t probe = {0};
    return sb_music_sequence_start(&probe, song) && valid_song_loop_eligible(song);
}
bool sb_music_sequence_start_loop(sb_music_sequence_t *s, const sb_music_song_t *song) {
    if (!s || s->shared || s->state != SB_MUSIC_IDLE) return false;
    sb_music_sequence_t next = {0};
    if (!sb_music_sequence_start(&next, song) || !valid_song_loop_eligible(song)) return false;
    next.loop = true;
    next.initial_bank = next.bank;
    *s = next;
    return true;
}

bool sb_music_sequence_start_mapped(sb_music_sequence_t *s,
    const sb_music_song_t *song, const unsigned *voices, size_t count, bool loop) {
    if (!s || !s->shared || s->state!=SB_MUSIC_IDLE || !song || !voices ||
        count!=song->track_count || count>SB_AUDIO_BANK_VOICES) return false;
    unsigned mask=0;
    for (size_t n=0;n<count;++n) {
        unsigned v=voices[n];
        if (v<1 || v>SB_AUDIO_BANK_VOICES || (mask & (1u<<(v-1))) ||
            sb_audio_bank_active(&s->bank,v)) return false;
        mask |= 1u<<(v-1);
    }
    /* Reuse complete song/loop validation before touching the live bank. */
    sb_music_sequence_t next={0};
    if (!(loop ? sb_music_sequence_start_loop(&next,song) :
                 sb_music_sequence_start(&next,song))) return false;
    sb_audio_bank_t prepared=next.bank;
    next.bank=s->bank;
    next.initial_bank=(sb_audio_bank_t){0};
    for (size_t n=0;n<count;++n) {
        unsigned v=voices[n];
        (void)sb_audio_bank_copy_voice(&next.bank,v,&prepared,(unsigned)n+1);
        (void)sb_audio_bank_copy_voice(&next.initial_bank,v,&next.bank,v);
        next.voice_map[n]=v;
    }
    next.shared=true;
    next.reserved=mask;
    *s=next;
    return true;
}
static unsigned physical_voice(const sb_music_sequence_t *s, unsigned track) {
    return s->shared ? s->voice_map[track-1] : track;
}

static void dispatch(sb_music_sequence_t *s) {
    while (s->next_event < s->song->event_count &&
           s->song->events[s->next_event].frame == s->frame) {
        const sb_music_event_t *e = &s->song->events[s->next_event++];
        unsigned voice=physical_voice(s,e->track);
        /* Setup validation and reserved ownership guarantee dispatch. */
        if (e->kind == SB_MUSIC_NOTE_OFF) (void)sb_audio_bank_note_off(&s->bank, voice);
        else if (s->song->instruments[e->track-1].waveform == SB_AUDIO_VOICE_NOISE)
            (void)sb_audio_bank_start_selected_noise(&s->bank, voice);
        else (void)sb_audio_bank_start_selected(&s->bank, voice, e->hz);
    }
}

static void finish_if_silent(sb_music_sequence_t *s) {
    if (s->state != SB_MUSIC_FINISHING) return;
    for (unsigned v = 1; v <= s->song->track_count; ++v)
        if (sb_audio_bank_active(&s->bank, physical_voice(s,v))) return;
    sb_music_sequence_reset(s);
}

bool sb_music_sequence_stop(sb_music_sequence_t *s) {
    if (!s) return false;
    if (s->state == SB_MUSIC_IDLE) return true;
    /* Stop event dispatch and boundary restart before applying voice fades.
     * Bank stop is idempotent: repeated calls do not restart a running fade. */
    s->loop = false;
    s->state = SB_MUSIC_FINISHING;
    for (unsigned v = 1; v <= s->song->track_count; ++v)
        (void)sb_audio_bank_stop(&s->bank, physical_voice(s,v));
    finish_if_silent(s);
    return true;
}

bool sb_music_sequence_set_gain(sb_music_sequence_t *s, double gain) {
    if (!s || s->state == SB_MUSIC_IDLE || !isfinite(gain) || gain < 0 || gain > 1)
        return false;
    if (gain != s->gain_target) {
        s->gain_from = s->gain;
        s->gain_target = gain;
        s->gain_left = SB_MUSIC_GAIN_FRAMES;
    }
    return true;
}

bool sb_music_sequence_render(sb_music_sequence_t *s, float *output, size_t frames) {
    if (!s || (frames && !output) || frames > SIZE_MAX / (2 * sizeof(float))) return false;
    for (size_t n = 0; n < frames; ++n) {
        if (s->state == SB_MUSIC_PLAYING) dispatch(s);
        if (s->shared)
            (void)sb_audio_bank_render_music(&s->bank,s->reserved,s->gain,output+2*n,1);
        else {
            (void)sb_audio_bank_render(&s->bank, output + 2*n, 1);
            output[2*n] = (float)(output[2*n] * s->gain);
            output[2*n+1] = (float)(output[2*n+1] * s->gain);
        }
        if (s->gain_left) {
            --s->gain_left;
            s->gain = s->gain_left ? s->gain_from + (s->gain_target - s->gain_from) *
                (SB_MUSIC_GAIN_FRAMES - s->gain_left) / SB_MUSIC_GAIN_FRAMES : s->gain_target;
        }
        if (s->state == SB_MUSIC_PLAYING) {
            /* Never increment beyond end_frame, even for UINT64_MAX ends. */
            ++s->frame;
            if (s->frame == s->song->end_frame) {
                dispatch(s);
                if (s->loop) {
                    /* Fixed-size snapshot copy only: no setup, allocation or
                     * event scan. Frame-zero events precede the next sample. */
                    if (s->shared) {
                        for (unsigned v=1;v<=SB_AUDIO_BANK_VOICES;++v)
                            if (s->reserved & (1u<<(v-1)))
                                (void)sb_audio_bank_copy_voice(&s->bank,v,&s->initial_bank,v);
                    } else s->bank = s->initial_bank;
                    s->frame = 0;
                    s->next_event = 0;
                } else s->state = SB_MUSIC_FINISHING;
            }
        }
        finish_if_silent(s);
    }
    return true;
}
