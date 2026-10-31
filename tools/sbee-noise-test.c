#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
static bool wait_idle(sb_audio_t *audio, unsigned voice) {
    Uint32 start = SDL_GetTicks();
    while (sb_audio_voice_active(audio, voice) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    return !sb_audio_voice_active(audio, voice);
}
int main(void) {
    puts("Noise test: lower output volume first; broadband noise may sound louder than tones.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    bool ok = sb_audio_voice_configure(audio, 1, .08, .1, .7, .3) &&
              sb_audio_voice_volume(audio, 1, .5) && sb_audio_voice_pan(audio, 1, -1) &&
              sb_audio_voice_seed(audio, 1, 123456789);
    for (unsigned i = 0; ok && i < 2; ++i) {
        puts("Left noise burst: same seed and envelope.");
        ok = sb_audio_voice_noise_on(audio, 1);
        SDL_Delay(450);
        ok = sb_audio_voice_note_off(audio, 1) && ok;
        ok = wait_idle(audio, 1) && ok;
        SDL_Delay(150);
    }
    puts("Center noise: retrigger during release, then short-stop.");
    if (ok) ok = sb_audio_voice_pan(audio, 1, 0) && sb_audio_voice_noise_on(audio, 1);
    SDL_Delay(450);
    if (ok) ok = sb_audio_voice_note_off(audio, 1);
    SDL_Delay(100);
    if (ok) ok = sb_audio_voice_noise_on(audio, 1);
    SDL_Delay(500);
    if (ok) ok = sb_audio_voice_stop(audio, 1) && wait_idle(audio, 1);
    puts("Noise left and right with center triangle, then all nine mixed voices.");
    for (unsigned v = 1; ok && v <= 9; ++v) {
        ok = sb_audio_voice_configure(audio, v, .05, .1, .6, .2) &&
             sb_audio_voice_volume(audio, v, .35) && sb_audio_voice_pan(audio, v, (v-5.0)/4);
        if (v == 1 || v == 9) ok = sb_audio_voice_noise_on(audio, v) && ok;
        if (v == 5) ok = sb_audio_voice_note_on(audio, v, 440) && ok;
    }
    SDL_Delay(700);
    for (unsigned v = 2; ok && v <= 8; ++v) {
        if (v == 5) continue;
        ok = v%2 ? sb_audio_voice_noise_on(audio, v) : sb_audio_voice_note_on(audio, v, 220+v*40);
    }
    SDL_Delay(900);
    for (unsigned v = 1; v <= 9; ++v) ok = sb_audio_voice_note_off(audio, v) && ok;
    for (unsigned v = 1; v <= 9; ++v) ok = wait_idle(audio, v) && ok;
    if (ok) SDL_Delay(250);
    sb_audio_destroy(audio);
    puts(ok ? "Noise test complete; confirm sound by listening." : "Noise test failed or timed out.");
    return ok ? 0 : 1;
}
