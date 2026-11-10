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
/* Same sustained chord through the bounded loader; a diagnostic fixture, not
 * an exporter or cartridge loading path. */
static const char owned_json[]=
    "{\"format\":\"sugarbee.music\",\"version\":1,\"timeline_rate\":48000,"
    "\"end_frame\":1488000,\"tracks\":["
#define TRACK(PAN) "{\"instrument\":{\"waveform\":\"triangle\",\"attack\":0.02,\"decay\":0," \
    "\"sustain\":1,\"release\":0.1,\"volume\":0.4,\"pan\":" PAN \
    ",\"filter\":{\"cutoff\":2000,\"bypass\":true}}}"
    TRACK("-0.5") "," TRACK("0") "," TRACK("0.5")
#undef TRACK
    "],\"events\":["
    "{\"frame\":0,\"track\":1,\"type\":\"note_on\",\"hz\":220},"
    "{\"frame\":0,\"track\":2,\"type\":\"note_on\",\"hz\":275},"
    "{\"frame\":0,\"track\":3,\"type\":\"note_on\",\"hz\":330},"
    "{\"frame\":1440000,\"track\":1,\"type\":\"note_off\"},"
    "{\"frame\":1440000,\"track\":2,\"type\":\"note_off\"},"
    "{\"frame\":1440000,\"track\":3,\"type\":\"note_off\"}]}";
static bool start_music(sb_audio_t *a,sb_music_handle_t id) {
    return id ? sb_audio_music_play(a,id,voices,3,true)==SB_MUSIC_ASSET_OK :
        sb_audio_music_start(a,&song,voices,3,true);
}

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
static bool load_music(sb_audio_t *a,const char *cart,const char *path,sb_music_handle_t *id) {
    if (!cart) return sb_audio_music_load(a,owned_json,sizeof owned_json-1,id,NULL)==SB_MUSIC_ASSET_OK;
    sb_asset_result_t read;
    sb_music_load_result_t detail;
    sb_music_asset_code_t code=sb_audio_music_load_cartridge(a,cart,path,id,&read,&detail);
    if (code==SB_MUSIC_ASSET_READ_FAILED) fprintf(stderr,"Music read: %s\n",sb_asset_error_string(read));
    else if (code==SB_MUSIC_ASSET_LOAD_FAILED) fprintf(stderr,"Music load: %s\n",detail.message);
    else if (code!=SB_MUSIC_ASSET_OK) fprintf(stderr,"Music publication refused: %d\n",(int)code);
    return code==SB_MUSIC_ASSET_OK;
}
int main(int argc,char **argv) {
    bool from_cart=argc==4 && !strcmp(argv[1],"--cartridge");
    bool owned=from_cart || (argc==2 && !strcmp(argv[1],"--owned"));
    if (argc!=1 && !owned) {
        fprintf(stderr,"Usage: %s [--owned | --cartridge file.vcart member]\n",argv[0]); return 2;
    }
    const char *cart=from_cart?argv[2]:NULL,*path=from_cart?argv[3]:NULL;
    sb_music_handle_t music=0;
    int result=1;
    puts("Production music session: start with low speaker volume.");
    sb_audio_t *a=sb_audio_create(); if (!a) return 1;
    sb_audio_session_begin(a);
    sb_wav_handle_t asset,playback;
    REQUIRE(load_wav(a,&asset));
    if (owned) {
        puts(from_cart ? "Using music loaded from a cartridge member." : "Using session-owned JSON music asset handles.");
        REQUIRE(load_music(a,cart,path,&music));
    }
    REQUIRE(start_music(a,music));
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
    REQUIRE(start_music(a,music));
    listen("Start music fully paused: remains silent.");
    sb_audio_session_pause(a,false); listen("Resume: opening chord joins saw and WAV.");
    sb_audio_session_fail(a); listen("Session failure: all sources fade out and stay silent.");
    REQUIRE(sb_audio_music_state(a)==SB_MUSIC_IDLE);
    REQUIRE(!sb_audio_wav_playing(a,playback));
    REQUIRE(!start_music(a,music));
    sb_audio_session_end(a); sb_audio_session_begin(a);
    if (owned) {
        REQUIRE(sb_audio_music_play(a,music,voices,3,true)==SB_MUSIC_ASSET_BAD_HANDLE);
        REQUIRE(load_music(a,cart,path,&music));
    }
    REQUIRE(start_music(a,music));
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
