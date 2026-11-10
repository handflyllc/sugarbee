#include "sb_audio_bank.h"
#include <stdint.h>
#include <math.h>

static sb_audio_triangle_t *voice_at(sb_audio_bank_t *bank, unsigned voice) {
    return bank && voice >= 1 && voice <= SB_AUDIO_BANK_VOICES ? &bank->voices[voice-1] : NULL;
}
void sb_audio_bank_reset(sb_audio_bank_t *bank) {
    if (bank) *bank = (sb_audio_bank_t){0};
}
bool sb_audio_bank_configure(sb_audio_bank_t *bank, unsigned voice, const sb_envelope_settings_t *settings) {
    return sb_audio_triangle_configure(voice_at(bank, voice), settings);
}
bool sb_audio_bank_note_on(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || bank->switch_left[voice-1] || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_TRIANGLE)) return false;
    bool idle = !v->active;
    if (!sb_audio_triangle_start(v, frequency)) return false;
    if (idle) sb_audio_filter_start(&bank->filters[voice-1]);
    bank->kind[voice-1] = SB_AUDIO_VOICE_TRIANGLE;
    bank->retained_pitch[voice-1] = frequency;
    return true;
}
/* Fixed, distinct nonzero defaults; no clock or process-global random state. */
static const uint32_t default_seeds[SB_AUDIO_BANK_VOICES] = {
    UINT32_C(0x243f6a88), UINT32_C(0x85a308d3), UINT32_C(0x13198a2e),
    UINT32_C(0x03707344), UINT32_C(0xa4093822), UINT32_C(0x299f31d0),
    UINT32_C(0x082efa98), UINT32_C(0xec4e6c89), UINT32_C(0x452821e6)
};
static void fresh_noise(sb_audio_bank_t *bank, unsigned slot) {
    uint32_t seed = bank->seeds[slot];
    sb_audio_noise_reset(&bank->noise[slot], seed ? seed : default_seeds[slot]);
}
bool sb_audio_bank_seed(sb_audio_bank_t *bank, unsigned voice, uint32_t seed) {
    if (!voice_at(bank, voice)) return false;
    bank->seeds[voice-1] = seed;
    return true;
}
bool sb_audio_bank_noise_on(sb_audio_bank_t *bank, unsigned voice) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || bank->switch_left[voice-1] || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_NOISE)) return false;
    bool idle = !v->active;
    if (!sb_audio_voice_start(v)) return false;
    if (idle) {
        fresh_noise(bank, voice-1);
        sb_audio_filter_start(&bank->filters[voice-1]);
    }
    bank->kind[voice-1] = SB_AUDIO_VOICE_NOISE;
    return true;
}
bool sb_audio_bank_saw_on(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || bank->switch_left[voice-1] || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_SAW)) return false;
    /* Validate the prospective oscillator before touching the live envelope. */
    sb_audio_saw_t next = bank->saw[voice-1];
    bool ok = v->active ? sb_audio_saw_pitch(&next, frequency) :
                          sb_audio_saw_reset(&next, frequency);
    bool idle = !v->active;
    if (!ok || !sb_audio_voice_start(v)) return false;
    if (idle) sb_audio_filter_start(&bank->filters[voice-1]);
    bank->saw[voice-1] = next;
    bank->kind[voice-1] = SB_AUDIO_VOICE_SAW;
    bank->retained_pitch[voice-1] = frequency;
    return true;
}
bool sb_audio_bank_pulse_on(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || bank->switch_left[voice-1] || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_PULSE)) return false;
    sb_audio_pulse_t next = bank->pulse[voice-1];
    double width = bank->widths[voice-1];
    bool ok = v->active ? sb_audio_pulse_pitch(&next, frequency) :
        sb_audio_pulse_reset(&next, frequency, width ? width : SB_AUDIO_PULSE_DEFAULT_WIDTH);
    bool idle = !v->active;
    if (!ok || !sb_audio_voice_start(v)) return false;
    if (idle) sb_audio_filter_start(&bank->filters[voice-1]);
    bank->pulse[voice-1] = next;
    bank->kind[voice-1] = SB_AUDIO_VOICE_PULSE;
    bank->retained_pitch[voice-1] = frequency;
    return true;
}
bool sb_audio_bank_pulse_width(sb_audio_bank_t *bank, unsigned voice, double width) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || !isfinite(width) || width < .05 || width > .95) return false;
    if (v->active && (bank->kind[voice-1] == SB_AUDIO_VOICE_PULSE ||
        (bank->switch_left[voice-1] && bank->destination[voice-1] == SB_AUDIO_VOICE_PULSE)) &&
        !sb_audio_pulse_width(&bank->pulse[voice-1], width)) return false;
    bank->widths[voice-1] = width;
    return true;
}
static bool pitched(sb_audio_voice_kind_t kind) {
    return kind == SB_AUDIO_VOICE_TRIANGLE || kind == SB_AUDIO_VOICE_SAW || kind == SB_AUDIO_VOICE_PULSE;
}
static double source_phase(const sb_audio_bank_t *bank, unsigned slot) {
    if (bank->kind[slot] == SB_AUDIO_VOICE_SAW) return bank->saw[slot].phase;
    if (bank->kind[slot] == SB_AUDIO_VOICE_PULSE) return bank->pulse[slot].phase;
    return bank->voices[slot].phase;
}
static double source_pitch(const sb_audio_bank_t *bank, unsigned slot) {
    if (bank->kind[slot] == SB_AUDIO_VOICE_NOISE)
        return bank->retained_pitch[slot] ? bank->retained_pitch[slot] : 440;
    if (bank->kind[slot] == SB_AUDIO_VOICE_SAW) return bank->saw[slot].frequency;
    if (bank->kind[slot] == SB_AUDIO_VOICE_PULSE) return bank->pulse[slot].frequency;
    return bank->voices[slot].frequency;
}
bool sb_audio_bank_waveform(sb_audio_bank_t *bank, unsigned voice, sb_audio_voice_kind_t kind) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || (!pitched(kind) && kind != SB_AUDIO_VOICE_NOISE)) return false;
    unsigned slot = voice-1;
    if (bank->switch_left[slot]) return kind == bank->destination[slot];
    if (!v->active) { bank->kind[slot] = kind; return true; }
    if (kind == bank->kind[slot]) return true;
    bool from_noise = bank->kind[slot] == SB_AUDIO_VOICE_NOISE;
    double phase = source_phase(bank, slot), frequency = source_pitch(bank, slot);
    if (kind == SB_AUDIO_VOICE_NOISE) {
        fresh_noise(bank, slot);
    } else if (kind == SB_AUDIO_VOICE_SAW) {
        sb_audio_saw_t next;
        if (!sb_audio_saw_reset(&next, frequency)) return false;
        if (!from_noise) next.phase = phase;
        bank->saw[slot] = next;
    } else if (kind == SB_AUDIO_VOICE_PULSE) {
        sb_audio_pulse_t next;
        double width = bank->widths[slot];
        if (!sb_audio_pulse_reset(&next, frequency, width ? width : .5)) return false;
        if (!from_noise) next.phase = phase;
        bank->pulse[slot] = next;
    } else {
        v->phase = from_noise ? .25 : phase;
        v->frequency = frequency;
    }
    bank->destination[slot] = kind;
    bank->switch_left[slot] = SB_AUDIO_SWITCH_FRAMES;
    return true;
}
static void set_source_pitch(sb_audio_bank_t *bank, unsigned slot, sb_audio_voice_kind_t kind, double frequency) {
    if (kind == SB_AUDIO_VOICE_SAW) (void)sb_audio_saw_pitch(&bank->saw[slot], frequency);
    else if (kind == SB_AUDIO_VOICE_PULSE) (void)sb_audio_pulse_pitch(&bank->pulse[slot], frequency);
    else if (kind == SB_AUDIO_VOICE_TRIANGLE) bank->voices[slot].frequency = frequency;
}
bool sb_audio_bank_pitch(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || !v->active ||
        !isfinite(frequency) || frequency < 110 || frequency > 880) return false;
    unsigned slot = voice-1;
    bank->retained_pitch[slot] = frequency;
    set_source_pitch(bank, slot, bank->kind[slot], frequency);
    if (bank->switch_left[slot]) set_source_pitch(bank, slot, bank->destination[slot], frequency);
    return true;
}
bool sb_audio_bank_start_selected(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v) return false;
    sb_audio_voice_kind_t selected = bank->switch_left[voice-1] ?
        bank->destination[voice-1] : bank->kind[voice-1];
    if (!pitched(selected)) return false;
    if (bank->switch_left[voice-1]) {
        if (!isfinite(frequency) || frequency < 110 || frequency > 880) return false;
        if (!sb_audio_voice_start(v)) return false;
        return sb_audio_bank_pitch(bank, voice, frequency);
    }
    if (bank->kind[voice-1] == SB_AUDIO_VOICE_SAW) return sb_audio_bank_saw_on(bank, voice, frequency);
    if (bank->kind[voice-1] == SB_AUDIO_VOICE_PULSE) return sb_audio_bank_pulse_on(bank, voice, frequency);
    return sb_audio_bank_note_on(bank, voice, frequency);
}
bool sb_audio_bank_start_selected_noise(sb_audio_bank_t *bank, unsigned voice) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v) return false;
    sb_audio_voice_kind_t selected = bank->switch_left[voice-1] ?
        bank->destination[voice-1] : bank->kind[voice-1];
    if (selected != SB_AUDIO_VOICE_NOISE) return false;
    if (bank->switch_left[voice-1]) return sb_audio_voice_start(v);
    return sb_audio_bank_noise_on(bank, voice);
}
bool sb_audio_bank_volume(sb_audio_bank_t *bank, unsigned voice, double volume) {
    return sb_audio_triangle_volume(voice_at(bank, voice), volume);
}
bool sb_audio_bank_pan(sb_audio_bank_t *bank, unsigned voice, double pan) {
    return sb_audio_triangle_pan(voice_at(bank, voice), pan);
}
bool sb_audio_bank_filter_cutoff(sb_audio_bank_t *bank, unsigned voice, double cutoff) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    return v && sb_audio_filter_cutoff(&bank->filters[voice-1], cutoff, v->active);
}
bool sb_audio_bank_filter_bypass(sb_audio_bank_t *bank, unsigned voice, bool bypass) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    return v && sb_audio_filter_bypass(&bank->filters[voice-1], bypass, v->active);
}
static void complete_switch(sb_audio_bank_t *bank, unsigned slot) {
    if (bank->switch_left[slot]) {
        bank->kind[slot] = bank->destination[slot];
        bank->switch_left[slot] = 0;
    }
}
bool sb_audio_bank_note_off(sb_audio_bank_t *bank, unsigned voice) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v) return false;
    sb_audio_triangle_note_off(v);
    if (!v->active) complete_switch(bank, voice-1);
    return true;
}
bool sb_audio_bank_stop(sb_audio_bank_t *bank, unsigned voice) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v) return false;
    sb_audio_triangle_stop(v);
    return true;
}
bool sb_audio_bank_active(const sb_audio_bank_t *bank, unsigned voice) {
    return bank && voice >= 1 && voice <= SB_AUDIO_BANK_VOICES && bank->voices[voice-1].active;
}
static double render_source(sb_audio_bank_t *bank, unsigned slot, sb_audio_voice_kind_t kind) {
    float sample = 0;
    if (kind == SB_AUDIO_VOICE_NOISE) (void)sb_audio_noise_render(&bank->noise[slot], &sample, 1);
    else if (kind == SB_AUDIO_VOICE_SAW) (void)sb_audio_saw_render(&bank->saw[slot], &sample, 1);
    else if (kind == SB_AUDIO_VOICE_PULSE) (void)sb_audio_pulse_render_conditioned(&bank->pulse[slot], &sample, 1);
    else {
        sb_audio_triangle_t *v = &bank->voices[slot];
        double triangle = 1.0 - 4.0*fabs(v->phase-.5);
        v->phase += v->frequency/48000.0;
        if (v->phase >= 1) v->phase -= 1;
        return triangle;
    }
    return sample;
}
static bool render_bank(sb_audio_bank_t *bank, unsigned mask, double gain, unsigned skip,
                        float *output, size_t frames) {
    if (skip >= (1u << SB_AUDIO_BANK_VOICES) || mask >= (1u << SB_AUDIO_BANK_VOICES) || !isfinite(gain) || gain < 0 || gain > 1) return false;
    if (!bank || frames > SIZE_MAX / (2 * sizeof(float)) || (!output && frames)) return false;
    for (size_t frame = 0; frame < frames; ++frame) {
        double left = 0, right = 0, music_left = 0, music_right = 0;
        for (unsigned slot = 0; slot < SB_AUDIO_BANK_VOICES; ++slot) {
            if (skip & (1u<<slot)) continue;
            sb_audio_triangle_t *v = &bank->voices[slot];
            double sample = 0;
            if (v->active) {
                sample = render_source(bank, slot, bank->kind[slot]);
                if (bank->switch_left[slot]) {
                    double incoming = render_source(bank, slot, bank->destination[slot]);
                    double mix = (SB_AUDIO_SWITCH_FRAMES-bank->switch_left[slot])/(double)SB_AUDIO_SWITCH_FRAMES;
                    sample = (1-mix)*sample + mix*incoming;
                    if (bank->switch_left[slot] == 1) complete_switch(bank, slot);
                    else --bank->switch_left[slot];
                }
            }
            if (v->active) sample = sb_audio_filter_sample(&bank->filters[slot], sample);
            float stereo[2];
            sb_audio_voice_apply(v, sample, stereo);
            if (!v->active) complete_switch(bank, slot);
            if (mask & (1u << slot)) {
                music_left += stereo[0]; music_right += stereo[1];
            } else { left += stereo[0]; right += stereo[1]; }
        }
        output[2*frame] = (float)((left + music_left*gain) * SB_AUDIO_SYNTH_GAIN);
        output[2*frame+1] = (float)((right + music_right*gain) * SB_AUDIO_SYNTH_GAIN);
    }
    return true;
}

bool sb_audio_bank_render(sb_audio_bank_t *bank, float *output, size_t frames) {
    return sb_audio_bank_render_music(bank, 0, 1, output, frames);
}
bool sb_audio_bank_copy_voice(sb_audio_bank_t *dst, unsigned to,
                              const sb_audio_bank_t *src, unsigned from) {
    if (!dst || !src || to<1 || to>SB_AUDIO_BANK_VOICES ||
        from<1 || from>SB_AUDIO_BANK_VOICES) return false;
    unsigned d=to-1, s=from-1;
    dst->voices[d] = src->voices[s];
    dst->noise[d] = src->noise[s];
    dst->seeds[d] = src->seeds[s];
    dst->saw[d] = src->saw[s];
    dst->pulse[d] = src->pulse[s];
    dst->filters[d] = src->filters[s];
    dst->retained_pitch[d] = src->retained_pitch[s];
    dst->widths[d] = src->widths[s];
    dst->kind[d] = src->kind[s];
    dst->destination[d] = src->destination[s];
    dst->switch_left[d] = src->switch_left[s];
    return true;
}

bool sb_audio_bank_render_music(sb_audio_bank_t *bank,unsigned mask,double gain,
                                float *output,size_t frames) {
    return render_bank(bank,mask,gain,0,output,frames);
}
bool sb_audio_bank_render_unreserved(sb_audio_bank_t *bank,unsigned reserved,
                                     float *output,size_t frames) {
    return render_bank(bank,0,1,reserved,output,frames);
}
