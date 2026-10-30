#include "sb_audio_bank.h"
#include <stdint.h>

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
    return sb_audio_triangle_start(voice_at(bank, voice), frequency);
}
bool sb_audio_bank_pitch(sb_audio_bank_t *bank, unsigned voice, double frequency) {
    return sb_audio_triangle_pitch(voice_at(bank, voice), frequency);
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
            sb_audio_triangle_render(&bank->voices[voice], stereo, 1);
            left += stereo[0];
            right += stereo[1];
        }
        output[2*frame] = (float)(left * SB_AUDIO_SYNTH_GAIN);
        output[2*frame+1] = (float)(right * SB_AUDIO_SYNTH_GAIN);
    }
    return true;
}
