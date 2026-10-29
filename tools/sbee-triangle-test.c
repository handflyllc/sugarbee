#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static bool wait_silent(sb_audio_t *audio) {
    Uint32 start = SDL_GetTicks();
    while (sb_audio_triangle_active(audio) && SDL_GetTicks() - start < 2000)
        SDL_Delay(5);
    return !sb_audio_triangle_active(audio);
}
int main(void) {
    puts("Triangle ADSR: use a low speaker/headphone volume (peak 0.05).");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    puts("1. Attack, decay, sustain at 220 Hz; pitch change to 440 Hz.");
    bool ok = sb_audio_configure_triangle(audio, 0.2, 0.2, 0.5, 0.6) &&
              sb_audio_start_triangle(audio, 220);
    if (ok) { SDL_Delay(700); ok = sb_audio_set_triangle_pitch(audio, 440); }
    if (ok) {
        SDL_Delay(300);
        puts("2. Note-off: configured release to silence.");
        sb_audio_note_off_triangle(audio);
        ok = wait_silent(audio);
    }
    if (ok) {
        SDL_Delay(200);
        puts("3. Release at 440 Hz, then retrigger at 660 Hz from current level.");
        ok = sb_audio_start_triangle(audio, 440);
    }
    if (ok) {
        SDL_Delay(600);
        sb_audio_note_off_triangle(audio);
        SDL_Delay(250);
        ok = sb_audio_start_triangle(audio, 660);
    }
    if (ok) {
        SDL_Delay(650);
        sb_audio_note_off_triangle(audio);
        ok = wait_silent(audio);
    }
    if (ok) {
        SDL_Delay(200);
        puts("4. Ten-second release configured, overridden by a 10-ms stop.");
        ok = sb_audio_configure_triangle(audio, 0.1, 0.1, 0.7, 10) &&
             sb_audio_start_triangle(audio, 220);
    }
    if (ok) SDL_Delay(400);
    sb_audio_stop_triangle(audio);
    ok = wait_silent(audio) && ok;
    if (ok) SDL_Delay(250);
    sb_audio_destroy(audio);
    if (ok) puts("ADSR test complete. Confirm the result by listening.");
    else fprintf(stderr, "ADSR test failed or timed out.\n");
    return ok ? 0 : 1;
}
