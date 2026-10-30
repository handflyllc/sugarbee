#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static bool any_active(sb_audio_t *audio) {
    for (unsigned voice = 1; voice <= 9; ++voice)
        if (sb_audio_voice_active(audio, voice)) return true;
    return false;
}
int main(void) {
    puts("Nine-voice test: start with low output volume; use stereo output.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    bool ok = true;
    const double pitches[9] = {220, 275, 330, 440, 550, 660, 247.5, 371.25, 495};
    for (unsigned v = 1; ok && v <= 9; ++v)
        ok = sb_audio_voice_configure(audio, v, .05, .1, .8, .3) &&
             sb_audio_voice_volume(audio, v, .5) &&
             sb_audio_voice_pan(audio, v, (v-5.0)/4);
    puts("One voice, left; fixed 1/9 headroom (intentionally quiet).");
    if (ok) ok = sb_audio_voice_note_on(audio, 1, pitches[0]);
    SDL_Delay(700);
    puts("Three voices: left, center, right.");
    if (ok) ok = sb_audio_voice_note_on(audio, 5, pitches[4]) &&
                 sb_audio_voice_note_on(audio, 9, pitches[8]);
    SDL_Delay(900);
    puts("All nine voices.");
    for (unsigned v = 1; ok && v <= 9; ++v)
        if (v != 1 && v != 5 && v != 9)
            ok = sb_audio_voice_note_on(audio, v, pitches[v-1]);
    SDL_Delay(1200);
    puts("Release odd voices; even voices continue.");
    for (unsigned v = 1; ok && v <= 9; v += 2)
        ok = sb_audio_voice_note_off(audio, v);
    SDL_Delay(800);
    puts("Short-stop remaining voices; silence.");
    for (unsigned v = 2; ok && v <= 9; v += 2)
        ok = sb_audio_voice_stop(audio, v);
    Uint32 start = SDL_GetTicks();
    while (any_active(audio) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    ok = !any_active(audio) && ok;
    if (ok) SDL_Delay(250);
    sb_audio_destroy(audio);
    if (ok) puts("Bank test complete. Confirm output by listening.");
    else fprintf(stderr, "Bank test failed or timed out.\n");
    return ok ? 0 : 1;
}
