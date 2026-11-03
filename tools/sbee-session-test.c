#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>

int main(void) {
    sb_audio_t *audio = sb_audio_create();
    if (!audio) return 1;
    sb_audio_session_begin(audio);
    puts("440 Hz tone, then pause: expect silence for one second.");
    bool ok = sb_audio_voice_configure(audio,1,.02,0,1,.1) &&
              sb_audio_voice_note_on(audio,1,440);
    SDL_Delay(1000);
    sb_audio_session_pause(audio,true);
    SDL_Delay(1000);
    puts("Resume: the tone returns for one second.");
    sb_audio_session_pause(audio,false);
    SDL_Delay(1000);
    puts("Failure: silence, with new notes rejected.");
    sb_audio_session_fail(audio);
    ok = !sb_audio_voice_note_on(audio,1,880) && ok;
    SDL_Delay(1000);
    sb_audio_session_end(audio);
    puts("Fresh cartridge: a higher tone, then exit to silence.");
    sb_audio_session_begin(audio);
    ok = sb_audio_voice_note_on(audio,1,880) && ok;
    SDL_Delay(1000);
    sb_audio_session_pause(audio,true);
    sb_audio_session_prepare_shutdown(audio);
    ok = sb_audio_voice_note_on(audio,1,220) && ok;
    SDL_Delay(500); /* Paused shutdown's note must remain silent. */
    sb_audio_session_end(audio);
    SDL_Delay(500);
    sb_audio_destroy(audio);
    puts(ok ? "Session checks complete; confirm the transitions by listening." : "Session checks failed.");
    return ok ? 0 : 1;
}
