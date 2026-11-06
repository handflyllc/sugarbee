#include "../src/sb_audio_sdl.c"
#include "wav_fixture.h"
#include <math.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"WAV SDL line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float out[2048];
static void render(sb_audio_t *a,unsigned frames) { audio_output(a,(Uint8 *)out,(int)(frames*2*sizeof(float))); }
static bool silent(unsigned frames) {
    for (unsigned i=0;i<frames*2;++i) if (out[i]!=0) return false;
    return true;
}
int main(void) {
    sb_wav_handle_t asset=99,id=99,stereo;
    CHECK(sb_audio_wav_load(NULL,NULL,0,&asset)==SB_WAV_UNAVAILABLE && asset==0);
    CHECK(sb_audio_wav_play(NULL,1,1,0,&id)==SB_WAV_UNAVAILABLE && id==0);
    CHECK(!sb_audio_wav_stop(NULL,1) && !sb_audio_wav_playing(NULL,1));
    sb_audio_t *a=sb_audio_create(); CHECK(a); SDL_PauseAudioDevice(a->device,1);
    size_t n; unsigned char *b=fixture(48000,1,10000,&n); CHECK(b);
    sb_audio_session_end(a);
    CHECK(sb_audio_wav_load(a,b,n,&asset)==SB_WAV_REJECTED && !asset);
    sb_audio_session_begin(a);
    CHECK(sb_audio_wav_load(a,b,n,&asset)==SB_WAV_OK);
    CHECK(sb_audio_wav_play_loop(a,asset,1,-1,&id)==SB_WAV_OK);
    for (unsigned i=0;i<21;++i) render(a,1000);
    CHECK(sb_audio_wav_playing(a,id));
    render(a,1); CHECK(fabs(out[0]-.025*32767/32768)<1e-8 && out[1]==0);
    CHECK(sb_audio_wav_controls(a,id,1,1)); render(a,120);
    CHECK(a->wav.playbacks[0].ramp==360);
    CHECK(sb_audio_wav_controls(a,id,1,1)); CHECK(a->wav.playbacks[0].ramp==360);
    render(a,360); render(a,1); CHECK(out[0]==0 && out[1]>.024);
    sb_audio_session_pause(a,true); render(a,480);
    sb_wav_pool_t frozen=a->wav;
    render(a,1024); CHECK(silent(1024) && !memcmp(&frozen,&a->wav,sizeof frozen));
    sb_audio_session_pause(a,false); render(a,1); CHECK(silent(1));
    render(a,479); CHECK(out[1]>0);
    /* Exact fade output and slot lifetime. */
    CHECK(sb_audio_wav_stop(a,id)); CHECK(!sb_audio_wav_stop(a,id));
    render(a,479);
    for (unsigned i=0;i<479;++i) CHECK(fabs(out[2*i+1]-.025*(32767.0/32768)*(1-i/480.0))<1e-8);
    CHECK(sb_audio_wav_playing(a,id) && sb_audio_wav_unload(a,asset)==SB_WAV_IN_USE);
    render(a,1); CHECK(!sb_audio_wav_playing(a,id));
    render(a,1); CHECK(silent(1));
    CHECK(sb_audio_wav_unload(a,asset)==SB_WAV_OK);
    CHECK(sb_audio_wav_load(a,b,n,&asset)==SB_WAV_OK);
    CHECK(sb_audio_wav_play_loop(a,asset,1,0,&id)==SB_WAV_OK);
    for (unsigned i=0;i<9;++i) render(a,1000);
    render(a,990);
    /* Callback partitioning during a control ramp. Shallow copy is render-only. */
    CHECK(sb_audio_wav_controls(a,id,.2,-.5));
    sb_audio_t copy=*a; float whole[2000],parts[2000];
    audio_output(a,(Uint8 *)whole,sizeof whole);
    for (unsigned i=0;i<1000;i+=37) {
        unsigned count=1000-i<37 ? 1000-i : 37;
        audio_output(&copy,(Uint8 *)(parts+2*i),(int)(count*2*sizeof(float)));
    }
    CHECK(!memcmp(whole,parts,sizeof whole));
    CHECK(!memcmp(a->wav.playbacks,copy.wav.playbacks,sizeof a->wav.playbacks));
    sb_audio_session_fail(a);
    CHECK(sb_audio_wav_play(a,asset,1,0,&id)==SB_WAV_REJECTED && id==0);
    render(a,480); render(a,1); CHECK(silent(1));
    CHECK(a->wav.bytes>0); /* No freeing from the callback. */
    sb_audio_session_end(a); CHECK(!a->wav.bytes);
    sb_audio_session_begin(a);
    CHECK(sb_audio_wav_play(a,asset,1,0,&id)==SB_WAV_BAD_HANDLE);
    CHECK(sb_audio_wav_load(a,b,n,&asset)==SB_WAV_OK);
    CHECK(sb_audio_wav_play_loop(a,asset,1,0,&id)==SB_WAV_OK);
    sb_audio_session_pause(a,true); render(a,20);
    sb_audio_session_prepare_shutdown(a); render(a,1024); CHECK(silent(1024));
    sb_audio_session_fail(a); CHECK(!sb_audio_wav_playing(a,id));
    sb_audio_session_begin(a); CHECK(!a->wav.bytes);
    free(b); b=fixture(44100,2,10000,&n); CHECK(b);
    CHECK(sb_audio_wav_load(a,b,n,&stereo)==SB_WAV_OK);
    CHECK(sb_audio_wav_play(a,stereo,1,-1,&id)==SB_WAV_OK);
    render(a,1); CHECK(out[0]>.024 && out[1]==0);
    CHECK(sb_audio_wav_controls(a,id,1,1)); render(a,480);
    render(a,1); CHECK(out[0]==0 && out[1]>.024);
    CHECK(sb_audio_wav_stop(a,id)); render(a,480);
    /* Eight full-scale stereo WAV voices plus all nine synth voices. */
    for (unsigned i=0;i<8;++i) CHECK(sb_audio_wav_play(a,stereo,1,0,&id)==SB_WAV_OK);
    for (unsigned v=1;v<=9;++v) {
        CHECK(sb_audio_voice_configure(a,v,0,0,1,0));
        CHECK(sb_audio_voice_volume(a,v,1) && sb_audio_voice_pan(a,v,-1));
        CHECK(sb_audio_voice_note_on(a,v,440));
    }
    render(a,1024);
    for (unsigned i=0;i<2048;++i) CHECK(fabs(out[i])<=.991);
    CHECK(fabs(out[1]-.2*32767/32768)<1e-7); /* Stereo center keeps both channels. */
    /* Fresh IDs after teardown, no wait for a paused device callback. */
    sb_audio_session_end(a); CHECK(!a->wav.bytes && !sb_audio_wav_playing(a,id));
    sb_audio_session_begin(a);
    CHECK(sb_audio_wav_load(a,b,n,&asset)==SB_WAV_OK && asset!=stereo);
    CHECK(!sb_audio_wav_stop(a,id));
    free(b); sb_audio_destroy(a); /* Also releases an unloaded-but-owned asset. */
    puts("WAV SDL mixing and lifecycle checks passed");
    return 0;
}
