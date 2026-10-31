/* SDL2 audio foundation: fixed callback format, host lifetime, opt-in diagnostic tone. */
#include "sb_audio.h"
#include "sb_audio_tone.h"
#include "sb_audio_bank.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sb_audio {
    SDL_AudioDeviceID device;
    sb_audio_tone_t tone;
    sb_audio_triangle_t triangle;
    sb_audio_bank_t bank;
};

/* Caller holds the device lock, or is the callback itself. */
static bool bank_active(const sb_audio_t *audio) {
    for (unsigned voice = 1; voice <= SB_AUDIO_BANK_VOICES; ++voice)
        if (sb_audio_bank_active(&audio->bank, voice)) return true;
    return false;
}

static const float diagnostic_gain = 0.05f;
static const float bank_diagnostic_gain = 0.790569415f; /* 0.25 * 10^(10/20): +10 dB. */

static void audio_output(void *userdata, Uint8 *stream, int length) {
    sb_audio_t *audio = userdata;
    if (length <= 0) return;
    /* Clear any trailing bytes too. SDL provides aligned float-format buffers. */
    memset(stream, 0, (size_t)length);
    if (audio->tone.active)
        sb_audio_tone_render(&audio->tone, (float *)stream,
                             (size_t)length / (2 * sizeof(float)));
    else if (audio->triangle.active || bank_active(audio)) {
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
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(audio);
}

/* Only the control thread locks: SDL excludes the callback while these short
   accesses run. The callback owns tone advancement and never calls Lua. */
bool sb_audio_start_test_tone(sb_audio_t *audio) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool started = !audio->tone.active && !audio->triangle.active && !bank_active(audio);
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
    bool started = !audio->tone.active && !bank_active(audio) && sb_audio_triangle_start(&audio->triangle, frequency);
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
    bool ok = sb_audio_bank_configure(&audio->bank, voice, &settings);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_note_on(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_note_on(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_pitch(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_pitch(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_volume(sb_audio_t *audio, unsigned voice, double volume) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_volume(&audio->bank, voice, volume);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_pan(sb_audio_t *audio, unsigned voice, double pan) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_pan(&audio->bank, voice, pan);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_note_off(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_note_off(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_stop(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_stop(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_active(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_active(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_noise_on(sb_audio_t *audio, unsigned voice) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_noise_on(&audio->bank, voice);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_seed(sb_audio_t *audio, unsigned voice, uint32_t seed) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_seed(&audio->bank, voice, seed);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_saw_on(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_saw_on(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_pulse_on(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_pulse_on(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_pulse_width(sb_audio_t *audio, unsigned voice, double width) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_pulse_width(&audio->bank, voice, width);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}

bool sb_audio_voice_waveform(sb_audio_t *audio, unsigned voice, sb_audio_voice_kind_t kind) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = sb_audio_bank_waveform(&audio->bank, voice, kind);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
bool sb_audio_voice_start_selected(sb_audio_t *audio, unsigned voice, double frequency) {
    if (!audio) return false;
    SDL_LockAudioDevice(audio->device);
    bool ok = !audio->tone.active && !audio->triangle.active &&
              sb_audio_bank_start_selected(&audio->bank, voice, frequency);
    SDL_UnlockAudioDevice(audio->device);
    return ok;
}
