#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
static bool release(sb_audio_t *audio) {
    if (!sb_audio_voice_note_off(audio,1)) return false;
    Uint32 start = SDL_GetTicks();
    while (sb_audio_voice_active(audio,1) && SDL_GetTicks()-start < 2000) SDL_Delay(5);
    return !sb_audio_voice_active(audio,1);
}
int main(int argc, char **argv) {
    bool piano=argc==2 && !strcmp(argv[1],"--piano");
    if (argc!=1 && !piano) { fprintf(stderr,"Usage: %s [--piano]\n",argv[0]); return 1; }
    puts(piano ? "Piano range: 27.5 / 110 / 880 / 1760 / 3520 / 4186.01 Hz."
               : "Pitch test: centered single voice, 110 / 220 / 440 / 880 Hz.");
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    const sb_audio_voice_kind_t kinds[] = {SB_AUDIO_VOICE_TRIANGLE,SB_AUDIO_VOICE_SAW,SB_AUDIO_VOICE_PULSE};
    const char *names[] = {"triangle","saw","pulse"};
    const double legacy[] = {110,220,440,880};
    const double expanded[] = {27.5,110,880,1760,3520,4186.01};
    const double *pitches=piano?expanded:legacy;
    unsigned count=piano?6:4;
    double low=piano?27.5:110, high=piano?4186.01:880;
    bool ok = sb_audio_voice_configure(audio,1,.03,0,1,.08) &&
              sb_audio_voice_volume(audio,1,.8) && sb_audio_voice_pan(audio,1,0);
    for (unsigned k=0;ok && k<3;++k) {
        for (unsigned f=0;ok && f<count;++f) {
            printf("%s: %.2f Hz, then a quiet gap.\n",names[k],pitches[f]);
            ok = sb_audio_voice_waveform(audio,1,kinds[k]) &&
                 sb_audio_voice_start_selected(audio,1,pitches[f]);
            SDL_Delay(700);
            if (ok) ok = release(audio);
            SDL_Delay(150);
        }
    }
    printf("Pulse: live %.2f -> %.2f -> %.2f Hz, width sweeps at both ends.\n",low,high,low);
    if (ok) ok = sb_audio_voice_pulse_width(audio,1,.05) && sb_audio_voice_pulse_on(audio,1,low);
    SDL_Delay(600);
    if (ok) ok = sb_audio_voice_pulse_width(audio,1,.95);
    SDL_Delay(600);
    if (ok) ok = sb_audio_voice_pitch(audio,1,high) && sb_audio_voice_pulse_width(audio,1,.05);
    SDL_Delay(600);
    if (ok) ok = sb_audio_voice_pitch(audio,1,low) && sb_audio_voice_pulse_width(audio,1,.95);
    SDL_Delay(600);
    if (ok) ok = release(audio);
    SDL_Delay(200);
    printf("%.2f Hz triangle -> saw -> pulse -> noise -> %.2f Hz triangle.\n",high,low);
    if (ok) ok = sb_audio_voice_pulse_width(audio,1,.5) &&
                 sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_TRIANGLE) &&
                 sb_audio_voice_start_selected(audio,1,high);
    SDL_Delay(700);
    for (unsigned k=1;ok && k<3;++k) {
        ok = sb_audio_voice_waveform(audio,1,kinds[k]);
        SDL_Delay(700);
    }
    if (ok) ok = sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_NOISE);
    SDL_Delay(700);
    if (ok) ok = sb_audio_voice_pitch(audio,1,low) &&
                 sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_TRIANGLE);
    SDL_Delay(700);
    if (ok) ok = release(audio);
    sb_audio_destroy(audio);
    puts(ok ? "Pitch test complete; confirm sound by listening." : "Pitch test failed.");
    return ok ? 0 : 1;
}
