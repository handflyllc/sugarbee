#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>

int main(void) {
    puts("Triangle test: 220 -> 440 -> 660 Hz, then fade to silence.");
    puts("Peak amplitude 0.05. Start with low speaker/headphone volume.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    bool ok = sb_audio_start_triangle(audio, 220);
    if (ok) {
        SDL_Delay(700);
        ok = sb_audio_set_triangle_pitch(audio, 440);
    }
    if (ok) {
        SDL_Delay(700);
        ok = sb_audio_set_triangle_pitch(audio, 660);
    }
    if (ok) SDL_Delay(700);
    sb_audio_stop_triangle(audio);
    Uint32 start = SDL_GetTicks();
    while (sb_audio_triangle_active(audio) && SDL_GetTicks() - start < 2000)
        SDL_Delay(5);
    ok = ok && !sb_audio_triangle_active(audio);
    if (ok) SDL_Delay(250);
    sb_audio_destroy(audio);
    if (ok) puts("Triangle test complete. Confirm the result by listening.");
    else fprintf(stderr, "Triangle request failed or stop timed out.\n");
    return ok ? 0 : 1;
}
