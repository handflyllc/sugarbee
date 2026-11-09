/* Audible production-backend checkpoint; generated PCM fixture, no asset IO. */
#include "sb_audio_music.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const sb_music_instrument_t instruments[]={
    {SB_AUDIO_VOICE_TRIANGLE,{.02,0,1,.1},.4,-.5,.5,2000,true},
    {SB_AUDIO_VOICE_TRIANGLE,{.02,0,1,.1},.4,0,.5,2000,true},
    {SB_AUDIO_VOICE_TRIANGLE,{.02,0,1,.1},.4,.5,.5,2000,true}
};
/* Sustained chord avoids authored note onsets near wall-clock transitions. */
static const sb_music_event_t events[]={
    {0,1,SB_MUSIC_NOTE_ON,220},{0,2,SB_MUSIC_NOTE_ON,275},{0,3,SB_MUSIC_NOTE_ON,330},
    {1440000,1,SB_MUSIC_NOTE_OFF,0},{1440000,2,SB_MUSIC_NOTE_OFF,0},{1440000,3,SB_MUSIC_NOTE_OFF,0}
};
static const sb_music_song_t song={48000,3,1488000,instruments,events,6};
static const unsigned voices[]={1,2,3};
static void put16(unsigned char *p,unsigned v) { p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8); }
static void put32(unsigned char *p,unsigned v) { for (unsigned i=0;i<4;++i) p[i]=(unsigned char)(v>>(8*i)); }
static bool load_wav(sb_audio_t *a,sb_wav_handle_t *asset) {
    const unsigned frames=48000,bytes=frames*2;
    unsigned char *data=calloc(1,44+bytes); if (!data) return false;
    memcpy(data,"RIFF",4); put32(data+4,36+bytes); memcpy(data+8,"WAVEfmt ",8);
    put32(data+16,16); put16(data+20,1); put16(data+22,1); put32(data+24,48000);
    put32(data+28,96000); put16(data+32,2); put16(data+34,16);
    memcpy(data+36,"data",4); put32(data+40,bytes);
    for (unsigned i=0;i<frames;++i) {
        int sample=(int)(24000*sin(6.283185307179586*880*i/48000));
        put16(data+44+2*i,(unsigned)(uint16_t)sample);
    }
    bool ok=sb_audio_wav_load(a,data,44+bytes,asset)==SB_WAV_OK;
    free(data); return ok;
}
#define REQUIRE(e) do { if (!(e)) { fprintf(stderr,"Check failed: %s\n",#e); goto done; } } while (0)
static void listen(const char *message) { puts(message); SDL_Delay(1000); }
int main(void) {
    int result=1;
    puts("Production music session: start with low speaker volume.");
    sb_audio_t *a=sb_audio_create(); if (!a) return 1;
    sb_audio_session_begin(a);
    sb_wav_handle_t asset,playback;
    REQUIRE(load_wav(a,&asset));
    REQUIRE(sb_audio_music_start(a,&song,voices,3,true));
    listen("Music: sustained triangle chord.");
    REQUIRE(!sb_audio_voice_note_on(a,1,880));
    REQUIRE(sb_audio_voice_configure(a,9,.02,0,1,.1));
    REQUIRE(sb_audio_voice_volume(a,9,.15)); REQUIRE(sb_audio_voice_pan(a,9,-1));
    REQUIRE(sb_audio_voice_saw_on(a,9,440));
    listen("Add independent saw effect on the left.");
    REQUIRE(sb_audio_wav_play_loop(a,asset,.6,1,&playback)==SB_WAV_OK);
    listen("Add high sine WAV on the right.");
    sb_audio_session_pause(a,true); listen("Pause: entire mix fades to silence.");
    sb_audio_session_pause(a,false); listen("Resume: entire mix returns.");
    REQUIRE(sb_audio_music_gain(a,0)); listen("Mute music: saw and WAV continue.");
    REQUIRE(sb_audio_music_gain(a,1)); listen("Restore chord; effects unchanged.");
    sb_audio_session_pause(a,true);
    REQUIRE(sb_audio_music_stop(a)); listen("Stop music during pause fade: silence.");
    sb_audio_session_pause(a,false); listen("Resume: only saw and WAV return.");
    REQUIRE(sb_audio_music_state(a)==SB_MUSIC_IDLE);
    sb_audio_session_pause(a,true); SDL_Delay(100);
    REQUIRE(sb_audio_music_start(a,&song,voices,3,true));
    listen("Start music fully paused: remains silent.");
    sb_audio_session_pause(a,false); listen("Resume: opening chord joins saw and WAV.");
    sb_audio_session_fail(a); listen("Session failure: all sources fade out and stay silent.");
    REQUIRE(sb_audio_music_state(a)==SB_MUSIC_IDLE);
    REQUIRE(!sb_audio_wav_playing(a,playback));
    REQUIRE(!sb_audio_music_start(a,&song,voices,3,true));
    sb_audio_session_end(a); sb_audio_session_begin(a);
    REQUIRE(sb_audio_music_start(a,&song,voices,3,true));
    listen("Fresh session: chord alone; no previous effects.");
    sb_audio_session_pause(a,true); sb_audio_session_prepare_shutdown(a);
    REQUIRE(sb_audio_voice_note_on(a,9,880));
    listen("Paused shutdown: silence, including shutdown-created notes.");
    sb_audio_session_end(a);
    result=0;
done:
    sb_audio_destroy(a);
    puts(result ? "Production music session checks FAILED." :
        "Production session checks complete; listening acceptance still required.");
    return result;
}
