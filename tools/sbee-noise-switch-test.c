#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
static bool wait_idle(sb_audio_t *audio, unsigned voice) {
    Uint32 start = SDL_GetTicks();
    while (sb_audio_voice_active(audio,voice) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    return !sb_audio_voice_active(audio,voice);
}
int main(void) {
    puts("Noise switch test: noise and pitched waveforms differ in loudness/timbre.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    const sb_audio_voice_kind_t pitched[] = {SB_AUDIO_VOICE_TRIANGLE,SB_AUDIO_VOICE_SAW,SB_AUDIO_VOICE_PULSE};
    const char *names[] = {"triangle","saw","pulse"};
    bool ok = sb_audio_voice_configure(audio,1,.05,.1,.7,.2) &&
        sb_audio_voice_volume(audio,1,.6) && sb_audio_voice_seed(audio,1,12345) &&
        sb_audio_voice_pulse_width(audio,1,.25);
    for (unsigned i=0;ok && i<3;++i) {
        printf("%s at 330 Hz, noise, then return at 550 Hz. Pan: %s.\n", names[i], i==0?"left":i==1?"center":"right");
        ok = sb_audio_voice_pan(audio,1,i-1.0) && sb_audio_voice_waveform(audio,1,pitched[i]) &&
             sb_audio_voice_start_selected(audio,1,330);
        SDL_Delay(450);
        if (ok) ok = sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_NOISE);
        SDL_Delay(200);
        if (ok) ok = sb_audio_voice_pitch(audio,1,550) && sb_audio_voice_seed(audio,1,54321) &&
                     sb_audio_voice_start_selected_noise(audio,1);
        SDL_Delay(250);
        if (ok) ok = sb_audio_voice_waveform(audio,1,pitched[i]);
        SDL_Delay(450);
        if (ok) ok = sb_audio_voice_note_off(audio,1) && wait_idle(audio,1);
    }
    const unsigned counts[] = {1, 3, 9};
    for (unsigned group=0;ok && group<3;++group) {
        unsigned count = counts[group];
        printf("%u voice(s), centered: 440 Hz triangle -> noise -> 440 Hz triangle.\n", count);
        puts("Triangle (one second).");
        for (unsigned v=1;ok && v<=count;++v)
            ok = sb_audio_voice_configure(audio,v,.05,.1,.6,.2) &&
                 sb_audio_voice_volume(audio,v,.3) && sb_audio_voice_pan(audio,v,0) &&
                 sb_audio_voice_seed(audio,v,0) &&
                 sb_audio_voice_waveform(audio,v,SB_AUDIO_VOICE_TRIANGLE) &&
                 sb_audio_voice_start_selected(audio,v,440);
        SDL_Delay(1000);
        puts("Noise (one second).");
        for (unsigned v=1;ok && v<=count;++v)
            ok = sb_audio_voice_waveform(audio,v,SB_AUDIO_VOICE_NOISE);
        SDL_Delay(1000);
        puts("Triangle again (one second).");
        for (unsigned v=1;ok && v<=count;++v)
            ok = sb_audio_voice_waveform(audio,v,SB_AUDIO_VOICE_TRIANGLE);
        SDL_Delay(1000);
        for (unsigned v=1;v<=count;++v) ok = sb_audio_voice_note_off(audio,v) && ok;
        for (unsigned v=1;v<=count;++v) ok = wait_idle(audio,v) && ok;
        if (ok) SDL_Delay(300);
    }
    sb_audio_destroy(audio);
    puts(ok ? "Noise switch test complete; confirm sound by listening." : "Noise switch test failed.");
    return ok ? 0 : 1;
}
