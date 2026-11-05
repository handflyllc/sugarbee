/* SDL2 audio foundation: fixed callback format, host lifetime, opt-in diagnostic tone. */
#include "sb_audio.h"
#include "sb_audio_tone.h"
#include "sb_audio_bank.h"
#include "sb_wav_internal.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fresh devices permit native diagnostics. The host closes this mode at startup. */
typedef enum { AUDIO_DIAGNOSTIC, AUDIO_SESSION, AUDIO_FAILED, AUDIO_CLOSED } audio_mode_t;
#define SESSION_FADE_FRAMES 480u
struct sb_audio {
    SDL_AudioDeviceID device;
    sb_audio_tone_t tone;
    sb_audio_triangle_t triangle;
    sb_audio_bank_t bank;
    sb_wav_pool_t wav;
    audio_mode_t mode;
    bool paused;
    double session_gain, gain_from, gain_target;
    unsigned gain_left;
};

/* Caller holds the device lock, or is the callback itself. */
static bool bank_active(const sb_audio_t *audio) {
    for (unsigned voice = 1; voice <= SB_AUDIO_BANK_VOICES; ++voice)
        if (sb_audio_bank_active(&audio->bank, voice)) return true;
    return false;
}

static bool wav_active(const sb_audio_t *audio) {
    for (unsigned i=0;i<SB_WAV_PLAYBACKS;++i)
        if (audio->wav.playbacks[i].active) return true;
    return false;
}

static const float diagnostic_gain = 0.05f;
static const float bank_diagnostic_gain = 0.790569415f; /* 0.25 * 10^(10/20): +10 dB. */

static bool bank_controls_allowed(const sb_audio_t *audio) {
    return audio->mode == AUDIO_DIAGNOSTIC || audio->mode == AUDIO_SESSION;
}
static void ramp_session(sb_audio_t *audio, double target) {
    audio->gain_from = audio->session_gain;
    audio->gain_target = target;
    audio->gain_left = audio->session_gain == target ? 0 : SESSION_FADE_FRAMES;
}
/* Callback only. Freeze the complete bank at silence; process a fade exactly
 * once per frame regardless of callback size. Failed sessions cannot resume. */
static void render_session(sb_audio_t *audio, float *output, size_t frames) {
    for (size_t i=0; i<frames; ++i) {
        if (!audio->gain_left && (audio->paused || audio->mode == AUDIO_FAILED)) continue;
        float sample[2];
        (void)sb_audio_bank_render(&audio->bank, sample, 1);
        float wav[2];
        sb_wav_render(&audio->wav,wav,1);
        output[2*i] = (float)((sample[0]*bank_diagnostic_gain+wav[0])*audio->session_gain);
        output[2*i+1] = (float)((sample[1]*bank_diagnostic_gain+wav[1])*audio->session_gain);
        if (audio->gain_left) {
            --audio->gain_left;
            audio->session_gain = audio->gain_left ? audio->gain_from +
                (audio->gain_target-audio->gain_from)*
                (SESSION_FADE_FRAMES-audio->gain_left)/SESSION_FADE_FRAMES : audio->gain_target;
            if (!audio->gain_left && audio->mode == AUDIO_FAILED) {
                sb_audio_bank_reset(&audio->bank);
                sb_wav_stop_all(&audio->wav);
            }
        }
    }
}
static void audio_output(void *userdata, Uint8 *stream, int length) {
    sb_audio_t *audio = userdata;
    if (length <= 0) return;
    /* Clear any trailing bytes too. SDL provides aligned float-format buffers. */
    memset(stream, 0, (size_t)length);
    if (audio->mode == AUDIO_SESSION || audio->mode == AUDIO_FAILED) {
        render_session(audio, (float *)stream, (size_t)length / (2*sizeof(float)));
        return;
    }
    if (audio->tone.active)
        sb_audio_tone_render(&audio->tone, (float *)stream,
                             (size_t)length / (2 * sizeof(float)));
    else if (audio->triangle.active || (audio->mode == AUDIO_DIAGNOSTIC && bank_active(audio))) {
        size_t frames = (size_t)length / (2 * sizeof(float));
        float gain = audio->triangle.active ? diagnostic_gain : bank_diagnostic_gain;
        if (audio->triangle.active)
            sb_audio_triangle_render(&audio->triangle, (float *)stream, frames);
        else
            (void)sb_audio_bank_render(&audio->bank, (float *)stream, frames);
        /* Diagnostic loudness is independent of the bank's fixed 1/9 headroom. */
        float *samples = (float *)stream;
        for (size_t i = 0; i < frames * 2; ++i) samples[i] *= gain;
    }
    if (audio->mode == AUDIO_DIAGNOSTIC && wav_active(audio)) {
        float *samples=(float *)stream;
        size_t frames=(size_t)length/(2*sizeof(float));
        for (size_t i=0;i<frames;++i) {
            float wav[2]; sb_wav_render(&audio->wav,wav,1);
            samples[2*i]+=wav[0]; samples[2*i+1]+=wav[1];
        }
    }
}

sb_audio_t *sb_audio_create(void) {
    sb_audio_t *audio = calloc(1, sizeof(*audio));
    if (!audio) {
        fprintf(stderr, "[SugarBee] Audio unavailable: out of memory; continuing silently.\n");
        return NULL;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[SugarBee] Audio unavailable: %s; continuing silently.\n", SDL_GetError());
        free(audio);
        return NULL;
    }
    SDL_AudioSpec desired = {0}, obtained = {0};
    desired.freq = SB_AUDIO_RATE;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = audio_output;
    desired.userdata = audio;
    /* SDL converts to the hardware format if necessary. Only callback buffer
       size may change; float stereo at 48 kHz remains our internal format. */
    audio->device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained,
                                       SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!audio->device) {
        fprintf(stderr, "[SugarBee] Audio unavailable: %s; continuing silently.\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        free(audio);
        return NULL;
    }
    SDL_PauseAudioDevice(audio->device, 0);
    return audio;
}

void sb_audio_destroy(sb_audio_t *audio) {
    if (!audio) return;
    /* SDL waits for any running callback before returning. No application
       drain loop or wait for future sounds is introduced. */
    SDL_CloseAudioDevice(audio->device);
    sb_wav_pool_reset(&audio->wav);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(audio);
}

/* Only the control thread locks: SDL excludes the callback while these short
   accesses run. The callback owns tone advancement and never calls Lua. */
bool sb_audio_start_test_tone(sb_audio_t *audio) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool started = audio->mode != AUDIO_SESSION && audio->mode != AUDIO_FAILED && !audio->tone.active && !audio->triangle.active && !bank_active(audio);
    if (started) sb_audio_tone_start(&audio->tone);
    SDL_UnlockAudioDevice(audio->device);
    return started;
}

bool sb_audio_test_tone_active(sb_audio_t *audio) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool active = audio->tone.active;
    SDL_UnlockAudioDevice(audio->device);
    return active;
}

bool sb_audio_start_triangle(sb_audio_t *audio, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool started = audio->mode != AUDIO_SESSION && audio->mode != AUDIO_FAILED && !audio->tone.active && !bank_active(audio) && sb_audio_triangle_start(&audio->triangle, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return started;
}

bool sb_audio_set_triangle_pitch(sb_audio_t *audio, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool changed = sb_audio_triangle_pitch(&audio->triangle, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return changed;
}

void sb_audio_stop_triangle(sb_audio_t *audio) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    sb_audio_triangle_stop(&audio->triangle);
    SDL_UnlockAudioDevice(audio->device);
}

bool sb_audio_triangle_active(sb_audio_t *audio) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool active = audio->triangle.active;
    SDL_UnlockAudioDevice(audio->device);
    return active;
}

bool sb_audio_configure_triangle(sb_audio_t *audio, double attack, double decay,
                                  double sustain, double release) {
    if (!audio) return false;
    sb_envelope_settings_t settings = {attack, decay, sustain, release};
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_triangle_configure(&audio->triangle, &settings);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
void sb_audio_note_off_triangle(sb_audio_t *audio) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    sb_audio_triangle_note_off(&audio->triangle);
    SDL_UnlockAudioDevice(audio->device);
}

bool sb_audio_set_triangle_volume(sb_audio_t *audio, double volume) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_triangle_volume(&audio->triangle, volume);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_set_triangle_pan(sb_audio_t *audio, double pan) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_triangle_pan(&audio->triangle, pan);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_configure(sb_audio_t *audio, unsigned voice, double attack,
                              double decay, double sustain, double release) {
    if (!audio) return false;
    sb_envelope_settings_t settings = {attack, decay, sustain, release};
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_configure(&audio->bank, voice, &settings);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_note_on(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_note_on(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_pitch(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_pitch(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_volume(sb_audio_t *audio, unsigned voice, double volume) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_volume(&audio->bank, voice, volume);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_pan(sb_audio_t *audio, unsigned voice, double pan) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_pan(&audio->bank, voice, pan);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_note_off(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_note_off(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_stop(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_stop(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_active(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_active(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_noise_on(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_noise_on(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_seed(sb_audio_t *audio, unsigned voice, uint32_t seed) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_seed(&audio->bank, voice, seed);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_saw_on(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_saw_on(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_pulse_on(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_pulse_on(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_pulse_width(sb_audio_t *audio, unsigned voice, double width) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_pulse_width(&audio->bank, voice, width);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_waveform(sb_audio_t *audio, unsigned voice, sb_audio_voice_kind_t kind) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_waveform(&audio->bank, voice, kind);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_start_selected(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_start_selected(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_start_selected_noise(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_start_selected_noise(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_filter_cutoff(sb_audio_t *audio, unsigned voice, double cutoff) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_filter_cutoff(&audio->bank, voice, cutoff);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_filter_bypass(sb_audio_t *audio, unsigned voice, bool bypass) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = bank_controls_allowed(audio) && sb_audio_bank_filter_bypass(&audio->bank, voice, bypass);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

/* Session transitions are host-control-thread operations, never Lua callbacks
 * from the audio thread. End is immediate and does not wait for fade playback. */
void sb_audio_session_begin(sb_audio_t *audio) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    sb_audio_bank_reset(&audio->bank);
    sb_wav_pool_reset(&audio->wav);
    audio->tone = (sb_audio_tone_t){0};
    audio->triangle = (sb_audio_triangle_t){0};
    audio->mode = AUDIO_SESSION;
    audio->paused = false;
    audio->session_gain = audio->gain_from = audio->gain_target = 1;
    audio->gain_left = 0;
    SDL_UnlockAudioDevice(audio->device);
}
void sb_audio_session_pause(sb_audio_t *audio, bool paused) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    if (audio->mode == AUDIO_SESSION && audio->paused != paused) {
        audio->paused = paused;
        ramp_session(audio, paused ? 0 : 1);
    }
    SDL_UnlockAudioDevice(audio->device);
}
void sb_audio_session_fail(sb_audio_t *audio) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    if (audio->mode == AUDIO_SESSION) {
        audio->mode = AUDIO_FAILED;
        ramp_session(audio, 0);
        if (!audio->gain_left) {
            sb_audio_bank_reset(&audio->bank);
            sb_wav_stop_all(&audio->wav);
        }
    }
    SDL_UnlockAudioDevice(audio->device);
}
void sb_audio_session_end(sb_audio_t *audio) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    sb_audio_bank_reset(&audio->bank);
    sb_wav_pool_reset(&audio->wav);
    audio->tone = (sb_audio_tone_t){0};
    audio->triangle = (sb_audio_triangle_t){0};
    audio->mode = AUDIO_CLOSED;
    audio->paused = false;
    audio->session_gain = audio->gain_from = audio->gain_target = 0;
    audio->gain_left = 0;
    SDL_UnlockAudioDevice(audio->device);
}

void sb_audio_session_prepare_shutdown(sb_audio_t *audio) {
    if (!audio) return;
    SDL_LockAudioDevice(audio->device);
    /* Exit from a pause menu must not briefly play shutdown-created notes,
     * even if the pause fade has not yet reached the callback. */
    if (audio->mode == AUDIO_SESSION && audio->paused) {
        audio->session_gain = audio->gain_from = audio->gain_target = 0;
        audio->gain_left = 0;
    }
    SDL_UnlockAudioDevice(audio->device);
}

sb_audio_voice_settings_t sb_audio_voice_defaults(void) {
    return (sb_audio_voice_settings_t){SB_AUDIO_VOICE_TRIANGLE,
        .01, .1, .7, .15, .5, 0, .5, 2000, true};
}
bool sb_audio_voice_apply_settings(sb_audio_t *audio, unsigned voice,
                          const sb_audio_voice_settings_t *settings) {
    if (!audio || !settings) return false;
    SDL_LockAudioDevice(audio->device);
    /* Fixed-size scratch copy: validate every operation before publishing it.
     * No Lua call, allocation, or error transfer occurs while locked. */
    sb_audio_bank_t next = audio->bank;
    sb_envelope_settings_t envelope = {settings->attack, settings->decay,
                                       settings->sustain, settings->release};
    bool ok = bank_controls_allowed(audio) &&
        sb_audio_bank_configure(&next,voice,&envelope) &&
        sb_audio_bank_volume(&next,voice,settings->volume) &&
        sb_audio_bank_pan(&next,voice,settings->pan) &&
        sb_audio_bank_pulse_width(&next,voice,settings->pulse_width) &&
        sb_audio_bank_filter_cutoff(&next,voice,settings->cutoff) &&
        sb_audio_bank_filter_bypass(&next,voice,settings->bypass) &&
        sb_audio_bank_waveform(&next,voice,settings->waveform);
    if (ok) audio->bank = next;
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

sb_wav_result_t sb_audio_wav_load(sb_audio_t *audio,const void *bytes,size_t length,sb_wav_handle_t *asset) {
    if (!asset) return SB_WAV_INVALID;
    *asset=0;
    if (!audio) return SB_WAV_UNAVAILABLE;
    SDL_LockAudioDevice(audio->device);
    bool allowed=bank_controls_allowed(audio);
    SDL_UnlockAudioDevice(audio->device);
    if (!allowed) return SB_WAV_REJECTED;
    sb_wav_data_t data={0};
    sb_wav_result_t result=sb_wav_decode(bytes,length,&data);
    if (result!=SB_WAV_OK) return result;
    SDL_LockAudioDevice(audio->device);
    result=bank_controls_allowed(audio) ? sb_wav_insert(&audio->wav,&data,asset) : SB_WAV_REJECTED;
    SDL_UnlockAudioDevice(audio->device);
    sb_wav_free(&data); /* Empty after successful transfer. */
    return result;
}
sb_wav_result_t sb_audio_wav_unload(sb_audio_t *audio,sb_wav_handle_t asset) {
    if (!audio) return SB_WAV_UNAVAILABLE;
    SDL_LockAudioDevice(audio->device);
    sb_wav_result_t r=bank_controls_allowed(audio) ? sb_wav_unload(&audio->wav,asset) : SB_WAV_REJECTED;
    SDL_UnlockAudioDevice(audio->device);
    return r;
}
sb_wav_result_t sb_audio_wav_play(sb_audio_t *audio,sb_wav_handle_t asset,double volume,double pan,sb_wav_handle_t *playback) {
    if (!playback) return SB_WAV_INVALID;
    *playback=0;
    if (!audio) return SB_WAV_UNAVAILABLE;
    SDL_LockAudioDevice(audio->device);
    sb_wav_result_t r=bank_controls_allowed(audio) ? sb_wav_play(&audio->wav,asset,volume,pan,playback) : SB_WAV_REJECTED;
    SDL_UnlockAudioDevice(audio->device);
    return r;
}
bool sb_audio_wav_playing(sb_audio_t *audio,sb_wav_handle_t playback) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok=sb_wav_playing(&audio->wav,playback);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_wav_stop(sb_audio_t *audio,sb_wav_handle_t playback) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok=bank_controls_allowed(audio) && sb_wav_stop(&audio->wav,playback);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_wav_controls(sb_audio_t *audio,sb_wav_handle_t playback,double volume,double pan) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok=bank_controls_allowed(audio) && sb_wav_controls(&audio->wav,playback,volume,pan);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
