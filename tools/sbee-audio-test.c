/* Opt-in native listening test; no Lua or cartridge required. */
#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>

int main(void) {
    puts("SugarBee audio test: one second of 440 Hz at amplitude 0.05.");
    puts("Use a low speaker/headphone volume for the first run.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    if (!sb_audio_start_test_tone(audio)) {
        sb_audio_destroy(audio);
        return 1;
    }
    Uint32 start = SDL_GetTicks();
    while (sb_audio_test_tone_active(audio) && SDL_GetTicks() - start < 3000)
        SDL_Delay(5);
    bool completed = !sb_audio_test_tone_active(audio);
    /* Brief silence lets queued output progress; not a hardware drain guarantee. */
    if (completed) SDL_Delay(250);
    else fprintf(stderr, "Audio test timed out waiting for sample generation.\n");
    sb_audio_destroy(audio);
    if (completed) puts("Tone generation complete. Confirm audibility by listening.");
    return completed ? 0 : 1;
}
