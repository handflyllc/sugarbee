#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
static bool release(sb_audio_t *audio) {
    if (!sb_audio_voice_note_off(audio,1)) return false;
    Uint32 start=SDL_GetTicks();
    while (sb_audio_voice_active(audio,1) && SDL_GetTicks()-start<2000) SDL_Delay(5);
    return !sb_audio_voice_active(audio,1);
}
int main(void) {
    puts("Simple low-pass: original, softened, muffled, then gradually brighter.");
    puts("Lower cutoffs can also make the sound quieter. No resonance or loudness compensation.");
    sb_audio_t *audio=sb_audio_create();
    if (!audio) return 1;
    bool ok=sb_audio_voice_configure(audio,1,.05,0,1,.15) &&
            sb_audio_voice_volume(audio,1,.8) && sb_audio_voice_pan(audio,1,0) &&
            sb_audio_voice_seed(audio,1,12345);
    for (unsigned kind=0;ok && kind<2;++kind) {
        printf("%s: original (bypass).\n",kind ? "Noise" : "440 Hz saw");
        ok=sb_audio_voice_filter_bypass(audio,1,true) &&
           (kind ? sb_audio_voice_noise_on(audio,1) : sb_audio_voice_saw_on(audio,1,440));
        SDL_Delay(1200);
        puts("Softened: cutoff 1200 Hz.");
        if (ok) ok=sb_audio_voice_filter_cutoff(audio,1,1200) && sb_audio_voice_filter_bypass(audio,1,false);
        SDL_Delay(1200);
        puts("Muffled: cutoff 120 Hz.");
        if (ok) ok=sb_audio_voice_filter_cutoff(audio,1,120);
        SDL_Delay(1200);
        puts("Gradually brighter: cutoff 120 -> 12000 Hz over two seconds.");
        for (unsigned step=1;ok && step<=50;++step) {
            ok=sb_audio_voice_filter_cutoff(audio,1,120*pow(100,step/50.0));
            SDL_Delay(40);
        }
        puts("Original again (bypass).");
        if (ok) ok=sb_audio_voice_filter_bypass(audio,1,true);
        SDL_Delay(1200);
        if (ok) ok=release(audio);
        SDL_Delay(300);
    }
    sb_audio_destroy(audio);
    puts(ok ? "Filter test complete; confirm the tone changes by listening." : "Filter test failed.");
    return ok ? 0 : 1;
}
