#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
int main(void) {
    puts("Volume/pan test: use low output volume and stereo speakers/headphones.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    bool ok = sb_audio_configure_triangle(audio, .05, 0, 1, .1) &&
              sb_audio_set_triangle_volume(audio, .5) &&
              sb_audio_set_triangle_pan(audio, -1) && sb_audio_start_triangle(audio, 440);
    const double pans[] = {-1, 0, 1, 0};
    const char *labels[] = {"Left", "Center", "Right", "Center"};
    for (unsigned i = 0; ok && i < 4; ++i) {
        puts(labels[i]);
        ok = sb_audio_set_triangle_pan(audio, pans[i]);
        SDL_Delay(650);
    }
    const double volumes[] = {.2, 0, .5};
    const char *levels[] = {"Quieter", "Muted (voice continues)", "Volume restored"};
    for (unsigned i = 0; ok && i < 3; ++i) {
        puts(levels[i]);
        ok = sb_audio_set_triangle_volume(audio, volumes[i]);
        SDL_Delay(650);
    }
    sb_audio_note_off_triangle(audio);
    Uint32 start = SDL_GetTicks();
    while (sb_audio_triangle_active(audio) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    ok = !sb_audio_triangle_active(audio) && ok;
    if (ok) SDL_Delay(250);
    sb_audio_destroy(audio);
    if (ok) puts("Pan test complete. Confirm output by listening.");
    else fprintf(stderr, "Pan test failed or timed out.\n");
    return ok ? 0 : 1;
}
