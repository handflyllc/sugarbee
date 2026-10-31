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
    if (!v || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_TRIANGLE)) return false;
    if (!sb_audio_triangle_start(v, frequency)) return false;
    bank->kind[voice-1] = SB_AUDIO_VOICE_TRIANGLE;
    return true;
}
/* Fixed, distinct nonzero defaults; no clock or process-global random state. */
static const uint32_t default_seeds[SB_AUDIO_BANK_VOICES] = {
    UINT32_C(0x243f6a88), UINT32_C(0x85a308d3), UINT32_C(0x13198a2e),
    UINT32_C(0x03707344), UINT32_C(0xa4093822), UINT32_C(0x299f31d0),
    UINT32_C(0x082efa98), UINT32_C(0xec4e6c89), UINT32_C(0x452821e6)
};
bool sb_audio_bank_seed(sb_audio_bank_t *bank, unsigned voice, uint32_t seed) {
    if (!voice_at(bank, voice)) return false;
    bank->seeds[voice-1] = seed;
    return true;
}
bool sb_audio_bank_noise_on(sb_audio_bank_t *bank, unsigned voice) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_NOISE)) return false;
    bool idle = !v->active;
    if (!sb_audio_voice_start(v)) return false;
    if (idle) {
        uint32_t seed = bank->seeds[voice-1];
        sb_audio_noise_reset(&bank->noise[voice-1], seed ? seed : default_seeds[voice-1]);
    }
    bank->kind[voice-1] = SB_AUDIO_VOICE_NOISE;
    return true;
}
bool sb_audio_bank_saw_on(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_SAW)) return false;
    /* Validate the prospective oscillator before touching the live envelope. */
    sb_audio_saw_t next = bank->saw[voice-1];
    bool ok = v->active ? sb_audio_saw_pitch(&next, frequency) :
                          sb_audio_saw_reset(&next, frequency);
    if (!ok || !sb_audio_voice_start(v)) return false;
    bank->saw[voice-1] = next;
    bank->kind[voice-1] = SB_AUDIO_VOICE_SAW;
    return true;
}
bool sb_audio_bank_pulse_on(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || (v->active && bank->kind[voice-1] != SB_AUDIO_VOICE_PULSE)) return false;
    sb_audio_pulse_t next = bank->pulse[voice-1];
    double width = bank->widths[voice-1];
    bool ok = v->active ? sb_audio_pulse_pitch(&next, frequency) :
        sb_audio_pulse_reset(&next, frequency, width ? width : SB_AUDIO_PULSE_DEFAULT_WIDTH);
    if (!ok || !sb_audio_voice_start(v)) return false;
    bank->pulse[voice-1] = next;
    bank->kind[voice-1] = SB_AUDIO_VOICE_PULSE;
    return true;
}
bool sb_audio_bank_pulse_width(sb_audio_bank_t *bank, unsigned voice, double width) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || !isfinite(width) || width < .05 || width > .95) return false;
    if (v->active && bank->kind[voice-1] == SB_AUDIO_VOICE_PULSE &&
        !sb_audio_pulse_width(&bank->pulse[voice-1], width)) return false;
    bank->widths[voice-1] = width;
    return true;
}
bool sb_audio_bank_pitch(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v || !v->active || bank->kind[voice-1] == SB_AUDIO_VOICE_NOISE) return false;
    if (bank->kind[voice-1] == SB_AUDIO_VOICE_PULSE)
        return sb_audio_pulse_pitch(&bank->pulse[voice-1], frequency);
    if (bank->kind[voice-1] == SB_AUDIO_VOICE_SAW)
        return sb_audio_saw_pitch(&bank->saw[voice-1], frequency);
    return sb_audio_triangle_pitch(v, frequency);
}
bool sb_audio_bank_volume(sb_audio_bank_t *bank, unsigned voice, double volume) {
    return sb_audio_triangle_volume(voice_at(bank, voice), volume);
}
bool sb_audio_bank_pan(sb_audio_bank_t *bank, unsigned voice, double pan) {
    return sb_audio_triangle_pan(voice_at(bank, voice), pan);
}
bool sb_audio_bank_note_off(sb_audio_bank_t *bank, unsigned voice) {
    sb_audio_triangle_t *v = voice_at(bank, voice);
    if (!v) return false;
    sb_audio_triangle_note_off(v);
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
bool sb_audio_bank_render(sb_audio_bank_t *bank, float *output, size_t frames) {
    if (!bank || frames > SIZE_MAX / (2 * sizeof(float)) || (!output && frames)) return false;
    for (size_t frame = 0; frame < frames; ++frame) {
        double left = 0, right = 0;
        for (unsigned voice = 0; voice < SB_AUDIO_BANK_VOICES; ++voice) {
            float stereo[2];
            sb_audio_voice_render(&bank->voices[voice],
                                  bank->kind[voice] == SB_AUDIO_VOICE_NOISE ? &bank->noise[voice] : NULL,
                                  bank->kind[voice] == SB_AUDIO_VOICE_SAW ? &bank->saw[voice] : NULL,
                                  bank->kind[voice] == SB_AUDIO_VOICE_PULSE ? &bank->pulse[voice] : NULL,
                                  stereo, 1);
            left += stereo[0];
            right += stereo[1];
        }
        output[2*frame] = (float)(left * SB_AUDIO_SYNTH_GAIN);
        output[2*frame+1] = (float)(right * SB_AUDIO_SYNTH_GAIN);
    }
    return true;
}
