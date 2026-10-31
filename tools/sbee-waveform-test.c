#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
static bool wait_idle(sb_audio_t *audio, unsigned voice) {
    Uint32 start = SDL_GetTicks();
    while (sb_audio_voice_active(audio,voice) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    return !sb_audio_voice_active(audio,voice);
}
int main(void) {
    puts("Waveform test: sustained pitch, changing timbre; loudness may vary.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    const sb_audio_voice_kind_t sequence[] = {SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_SAW,
        SB_AUDIO_VOICE_PULSE, SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_PULSE,
        SB_AUDIO_VOICE_SAW, SB_AUDIO_VOICE_TRIANGLE};
    const char *names[] = {"triangle","saw","pulse","triangle","pulse","saw","triangle"};
    bool ok = sb_audio_voice_configure(audio,1,.05,.1,.8,.2) &&
        sb_audio_voice_volume(audio,1,.7) && sb_audio_voice_pulse_width(audio,1,.25) &&
        sb_audio_voice_start_selected(audio,1,330);
    for (unsigned i=0;ok && i<7;++i) {
        printf("Center 330 Hz: %s\n",names[i]);
        ok = sb_audio_voice_waveform(audio,1,sequence[i]);
        SDL_Delay(550);
    }
    if (ok) ok = sb_audio_voice_note_off(audio,1) && wait_idle(audio,1);
    puts("Nine voices across stereo: triangle, then saw, then pulse; release.");
    for (unsigned v=1;ok && v<=9;++v)
        ok = sb_audio_voice_configure(audio,v,.05,.1,.6,.2) &&
             sb_audio_voice_volume(audio,v,.35) && sb_audio_voice_pan(audio,v,(v-5.0)/4) &&
             sb_audio_voice_waveform(audio,v,SB_AUDIO_VOICE_TRIANGLE) &&
             sb_audio_voice_start_selected(audio,v,220+v*40);
    SDL_Delay(500);
    for (unsigned v=1;ok && v<=9;++v) ok = sb_audio_voice_waveform(audio,v,SB_AUDIO_VOICE_SAW);
    SDL_Delay(500);
    for (unsigned v=1;ok && v<=9;++v) ok = sb_audio_voice_waveform(audio,v,SB_AUDIO_VOICE_PULSE);
    SDL_Delay(500);
    for (unsigned v=1;v<=9;++v) ok = sb_audio_voice_note_off(audio,v) && ok;
    for (unsigned v=1;v<=9;++v) ok = wait_idle(audio,v) && ok;
    sb_audio_destroy(audio);
    puts(ok ? "Waveform test complete; confirm sound by listening." : "Waveform test failed.");
    return ok ? 0 : 1;
}
