/* Internal platform-independent shared music/direct-synth owner. */
#ifndef SB_MUSIC_SHARED_H
#define SB_MUSIC_SHARED_H
#include "sb_music_transport.h"
typedef struct {
    sb_music_sequence_t sequence;
    sb_music_transport_t transport;
} sb_music_shared_t;
/* Initialize fresh storage; clears the entire bank. Never use init to stop music
 * in a running shared bank. Reset below clears only music-owned slots. */
void sb_music_shared_init(sb_music_shared_t *shared);
/* Music-only reset preserves effects and host pause/fade state. */
void sb_music_shared_reset(sb_music_shared_t *shared);
/* Host-wide 480-frame fade/freeze/resume applies to music and direct effects.
 * Same lifecycle implementation as the private host adapter. */
bool sb_music_shared_pause(sb_music_shared_t *shared, bool paused);
/* Exactly one distinct physical voice (1..9) per track, including unused tracks.
 * All requested voices must be idle. Map is copied. Refusal changes nothing.
 * Song and arrays remain alive/immutable until idle/reset. Reservation lasts
 * through rests and finishing; no stealing or restoration of prior settings. */
bool sb_music_shared_start(sb_music_shared_t *shared, const sb_music_song_t *song,
                           const unsigned *voices, size_t count, bool loop);
bool sb_music_shared_stop(sb_music_shared_t *shared);
bool sb_music_shared_set_gain(sb_music_shared_t *shared, double gain);
bool sb_music_shared_render(sb_music_shared_t *shared, float *output, size_t frames);
bool sb_music_shared_reserved(const sb_music_shared_t *shared, unsigned voice);
bool sb_music_shared_voice_active(const sb_music_shared_t *shared, unsigned voice);
/* All calls are serialized by the owner. Use these direct controls, not raw bank
 * calls or writes to embedded state: C struct access is not a security boundary.
 * Reserved voices refuse every mutation, even harmless/repeated commands.
 * Rendering advances music and effects once each; do not also render the bank.
 * This owner includes host pause: do not wrap it in the private host adapter.
 * The SDL backend owns one of these; Lua music integration is deferred. */
bool sb_music_shared_voice_configure(sb_music_shared_t *s, unsigned voice, const sb_envelope_settings_t *settings);
bool sb_music_shared_voice_note_on(sb_music_shared_t *s, unsigned voice, double frequency);
bool sb_music_shared_voice_noise_on(sb_music_shared_t *s, unsigned voice);
bool sb_music_shared_voice_saw_on(sb_music_shared_t *s, unsigned voice, double frequency);
bool sb_music_shared_voice_pulse_on(sb_music_shared_t *s, unsigned voice, double frequency);
bool sb_music_shared_voice_waveform(sb_music_shared_t *s, unsigned voice, sb_audio_voice_kind_t kind);
bool sb_music_shared_voice_start_selected(sb_music_shared_t *s, unsigned voice, double frequency);
bool sb_music_shared_voice_start_selected_noise(sb_music_shared_t *s, unsigned voice);
bool sb_music_shared_voice_pitch(sb_music_shared_t *s, unsigned voice, double frequency);
bool sb_music_shared_voice_volume(sb_music_shared_t *s, unsigned voice, double volume);
bool sb_music_shared_voice_pan(sb_music_shared_t *s, unsigned voice, double pan);
bool sb_music_shared_voice_pulse_width(sb_music_shared_t *s, unsigned voice, double width);
bool sb_music_shared_voice_seed(sb_music_shared_t *s, unsigned voice, uint32_t seed);
bool sb_music_shared_voice_filter_cutoff(sb_music_shared_t *s, unsigned voice, double cutoff);
bool sb_music_shared_voice_filter_bypass(sb_music_shared_t *s, unsigned voice, bool bypass);
bool sb_music_shared_voice_note_off(sb_music_shared_t *s, unsigned voice);
bool sb_music_shared_voice_stop(sb_music_shared_t *s, unsigned voice);
#endif
