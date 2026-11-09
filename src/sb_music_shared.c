#include "sb_music_shared.h"
#include <string.h>
void sb_music_shared_init(sb_music_shared_t *s) {
    if (!s) return;
    memset(s,0,sizeof(*s));
    s->sequence.shared=true;
}
void sb_music_shared_reset(sb_music_shared_t *s) {
    if (s) sb_music_sequence_reset(&s->sequence);
}
bool sb_music_shared_start(sb_music_shared_t *s, const sb_music_song_t *song,
                           const unsigned *voices, size_t count, bool loop) {
    return s && sb_music_sequence_start_mapped(&s->sequence,song,voices,count,loop);
}
bool sb_music_shared_stop(sb_music_shared_t *s) {
    return s && sb_music_sequence_stop(&s->sequence);
}
bool sb_music_shared_set_gain(sb_music_shared_t *s, double gain) {
    return s && sb_music_sequence_set_gain(&s->sequence,gain);
}
bool sb_music_shared_render(sb_music_shared_t *s, float *output, size_t frames) {
    return s && sb_music_sequence_render(&s->sequence,output,frames);
}
bool sb_music_shared_reserved(const sb_music_shared_t *s, unsigned voice) {
    return s && voice>=1 && voice<=SB_AUDIO_BANK_VOICES &&
        (s->sequence.reserved & (1u<<(voice-1)))!=0;
}
bool sb_music_shared_voice_active(const sb_music_shared_t *s, unsigned voice) {
    return s && sb_audio_bank_active(&s->sequence.bank,voice);
}
static bool available(const sb_music_shared_t *s, unsigned voice) {
    return s && s->sequence.shared && voice>=1 && voice<=SB_AUDIO_BANK_VOICES &&
        !sb_music_shared_reserved(s,voice);
}
bool sb_music_shared_voice_configure(sb_music_shared_t *s, unsigned voice, const sb_envelope_settings_t *settings) {
    return available(s,voice) && sb_audio_bank_configure(&s->sequence.bank,voice, settings);
}
bool sb_music_shared_voice_note_on(sb_music_shared_t *s, unsigned voice, double frequency) {
    return available(s,voice) && sb_audio_bank_note_on(&s->sequence.bank,voice, frequency);
}
bool sb_music_shared_voice_noise_on(sb_music_shared_t *s, unsigned voice) {
    return available(s,voice) && sb_audio_bank_noise_on(&s->sequence.bank,voice);
}
bool sb_music_shared_voice_saw_on(sb_music_shared_t *s, unsigned voice, double frequency) {
    return available(s,voice) && sb_audio_bank_saw_on(&s->sequence.bank,voice, frequency);
}
bool sb_music_shared_voice_pulse_on(sb_music_shared_t *s, unsigned voice, double frequency) {
    return available(s,voice) && sb_audio_bank_pulse_on(&s->sequence.bank,voice, frequency);
}
bool sb_music_shared_voice_waveform(sb_music_shared_t *s, unsigned voice, sb_audio_voice_kind_t kind) {
    return available(s,voice) && sb_audio_bank_waveform(&s->sequence.bank,voice, kind);
}
bool sb_music_shared_voice_start_selected(sb_music_shared_t *s, unsigned voice, double frequency) {
    return available(s,voice) && sb_audio_bank_start_selected(&s->sequence.bank,voice, frequency);
}
bool sb_music_shared_voice_start_selected_noise(sb_music_shared_t *s, unsigned voice) {
    return available(s,voice) && sb_audio_bank_start_selected_noise(&s->sequence.bank,voice);
}
bool sb_music_shared_voice_pitch(sb_music_shared_t *s, unsigned voice, double frequency) {
    return available(s,voice) && sb_audio_bank_pitch(&s->sequence.bank,voice, frequency);
}
bool sb_music_shared_voice_volume(sb_music_shared_t *s, unsigned voice, double volume) {
    return available(s,voice) && sb_audio_bank_volume(&s->sequence.bank,voice, volume);
}
bool sb_music_shared_voice_pan(sb_music_shared_t *s, unsigned voice, double pan) {
    return available(s,voice) && sb_audio_bank_pan(&s->sequence.bank,voice, pan);
}
bool sb_music_shared_voice_pulse_width(sb_music_shared_t *s, unsigned voice, double width) {
    return available(s,voice) && sb_audio_bank_pulse_width(&s->sequence.bank,voice, width);
}
bool sb_music_shared_voice_seed(sb_music_shared_t *s, unsigned voice, uint32_t seed) {
    return available(s,voice) && sb_audio_bank_seed(&s->sequence.bank,voice, seed);
}
bool sb_music_shared_voice_filter_cutoff(sb_music_shared_t *s, unsigned voice, double cutoff) {
    return available(s,voice) && sb_audio_bank_filter_cutoff(&s->sequence.bank,voice, cutoff);
}
bool sb_music_shared_voice_filter_bypass(sb_music_shared_t *s, unsigned voice, bool bypass) {
    return available(s,voice) && sb_audio_bank_filter_bypass(&s->sequence.bank,voice, bypass);
}
bool sb_music_shared_voice_note_off(sb_music_shared_t *s, unsigned voice) {
    return available(s,voice) && sb_audio_bank_note_off(&s->sequence.bank,voice);
}
bool sb_music_shared_voice_stop(sb_music_shared_t *s, unsigned voice) {
    return available(s,voice) && sb_audio_bank_stop(&s->sequence.bank,voice);
}
