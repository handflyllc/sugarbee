#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
static bool wait_idle(sb_audio_t *audio, unsigned voice) {
    Uint32 start = SDL_GetTicks();
    while (sb_audio_voice_active(audio, voice) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    return !sb_audio_voice_active(audio, voice);
}
int main(void) {
    puts("Pulse test: uses conditioned pulses and the existing diagnostic gain.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    bool ok = sb_audio_voice_configure(audio, 1, .08, .1, .7, .3) &&
              sb_audio_voice_volume(audio, 1, .5) && sb_audio_voice_pan(audio, 1, -1);
    puts("Left pulse: 220 Hz, then phase-preserving pitch change to 440 Hz.");
    if (ok) ok = sb_audio_voice_pulse_on(audio, 1, 220);
    SDL_Delay(600);
    if (ok) ok = sb_audio_voice_pitch(audio, 1, 440);
    SDL_Delay(600);
    puts("Center: gradual width sweep from square to 5%, then 95%.");
    if (ok) ok = sb_audio_voice_pan(audio, 1, 0);
    for (unsigned i = 0; ok && i <= 100; ++i) {
        double t = (i <= 50 ? i : i-50)/50.0;
        double width = i <= 50 ? (1-t)*.5+t*.05 : (1-t)*.05+t*.95;
        ok = sb_audio_voice_pulse_width(audio, 1, width);
        SDL_Delay(15);
    }
    puts("Release, then retrigger at 660 Hz while moving to center.");
    if (ok) ok = sb_audio_voice_note_off(audio, 1);
    SDL_Delay(100);
    if (ok) ok = sb_audio_voice_pan(audio, 1, 0) && sb_audio_voice_pulse_on(audio, 1, 660);
    SDL_Delay(600);
    if (ok) ok = sb_audio_voice_stop(audio, 1) && wait_idle(audio, 1);
    puts("Fresh right pulse at 330 Hz, then release to silence.");
    if (ok) ok = sb_audio_voice_pan(audio, 1, 1) && sb_audio_voice_pulse_on(audio, 1, 330);
    SDL_Delay(600);
    if (ok) ok = sb_audio_voice_note_off(audio, 1) && wait_idle(audio, 1);
    puts("Nine mixed voices: triangle, noise, saw, pulse across stereo.");
    for (unsigned v = 1; ok && v <= 9; ++v) {
        ok = sb_audio_voice_configure(audio, v, .05, .1, .6, .2) &&
             sb_audio_voice_volume(audio, v, .35) && sb_audio_voice_pan(audio, v, (v-5.0)/4);
        if (!ok) break;
        if (v%4 == 0) ok = sb_audio_voice_saw_on(audio, v, 220+v*40);
        else if (v%3 == 0) ok = sb_audio_voice_pulse_on(audio, v, 220+v*40);
        else if (v%3 == 1) ok = sb_audio_voice_note_on(audio, v, 220+v*40);
        else ok = sb_audio_voice_noise_on(audio, v);
    }
    SDL_Delay(1000);
    for (unsigned v = 1; v <= 9; ++v) ok = sb_audio_voice_note_off(audio, v) && ok;
    for (unsigned v = 1; v <= 9; ++v) ok = wait_idle(audio, v) && ok;
    if (ok) SDL_Delay(250);
    sb_audio_destroy(audio);
    puts(ok ? "Pulse test complete; confirm sound by listening." : "Pulse test failed or timed out.");
    return ok ? 0 : 1;
}
