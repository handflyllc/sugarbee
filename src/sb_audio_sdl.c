/* SDL2 audio foundation: fixed callback format, host lifetime, opt-in diagnostic tone. */
#include "sb_audio.h"
#include "sb_audio_tone.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sb_audio {
    SDL_AudioDeviceID device;
    sb_audio_tone_t tone;
};

static void audio_output(void *userdata, Uint8 *stream, int length) {
    sb_audio_t *audio = userdata;
    if (length <= 0) return;
    /* Clear any trailing bytes too. SDL provides aligned float-format buffers. */
    memset(stream, 0, (size_t)length);
    if (audio->tone.active)
        sb_audio_tone_render(&audio->tone, (float *)stream,
                             (size_t)length / (2 * sizeof(float)));
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
    bool started = !audio->tone.active;
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
