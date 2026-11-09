#include "../src/sb_music_shared.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Shared pause line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float whole[4096], split[4096], expected[4096];
static int silent(const float *p,size_t n) {
    for(size_t i=0;i<n*2;++i)if(p[i]!=0)return 0;
    return 1;
}
static int same_voice(const sb_audio_bank_t *a,const sb_audio_bank_t *b,unsigned v) {
    sb_audio_bank_t aa={0},bb={0};
    sb_audio_bank_copy_voice(&aa,v,a,v);sb_audio_bank_copy_voice(&bb,v,b,v);
    return !memcmp(&aa,&bb,sizeof(aa));
}
static int effect(sb_music_shared_t *s) {
    sb_envelope_settings_t env={0,0,1,.1};
    CHECK(sb_music_shared_voice_configure(s,8,&env));
    CHECK(sb_music_shared_voice_seed(s,8,123));
    CHECK(sb_music_shared_voice_filter_cutoff(s,8,700));
    CHECK(sb_music_shared_voice_filter_bypass(s,8,false));
    CHECK(sb_music_shared_voice_noise_on(s,8));
    return 0;
}
int main(void) {
    sb_music_instrument_t ins[]={
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,0},.6,-.4,0,900,false},
        {SB_AUDIO_VOICE_NOISE,{0,0,1,0},.3,.4,0,2000,true}};
    sb_music_event_t ev[]={{0,1,SB_MUSIC_NOTE_ON,440},{0,2,SB_MUSIC_NOTE_ON,0},
        {64,1,SB_MUSIC_NOTE_OFF,0},{64,2,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t song={48000,2,64,ins,ev,4};unsigned map[]={6,3};
    sb_music_shared_t a,b,ref;
    sb_music_shared_init(&a);CHECK(!effect(&a));
    CHECK(!sb_music_shared_pause(NULL,true));
    CHECK(sb_music_shared_start(&a,&song,map,2,true));
    CHECK(sb_music_shared_set_gain(&a,0));b=a;ref=a;
    CHECK(sb_music_shared_pause(&a,true) && sb_music_shared_pause(&b,true));
    /* Both music and effect receive the host fade, but music's own ramp
     * independently reaches zero. Exactly one shared render per frame. */
    CHECK(sb_music_shared_render(&a,whole,800));
    CHECK(sb_music_shared_render(&ref,expected,480));
    for(unsigned n=0;n<800;++n) {
        CHECK(sb_music_shared_pause(&b,true));
        CHECK(sb_music_shared_render(&b,split+2*n,1));
        for(unsigned ch=0;ch<2;++ch)
            CHECK(whole[2*n+ch]==(n<480?(float)(expected[2*n+ch]*(1.0-n/480.0)):0));
    }
    CHECK(!memcmp(whole,split,1600*sizeof(float)));
    CHECK(!memcmp(&a.sequence,&ref.sequence,sizeof(a.sequence)));
    sb_music_shared_t frozen=a;
    CHECK(sb_music_shared_render(&a,whole,2048) && silent(whole,2048));
    CHECK(!memcmp(&a,&frozen,sizeof(a)));
    CHECK(!sb_music_shared_render(&a,NULL,1));
    CHECK(!sb_music_shared_render(&a,whole,SIZE_MAX));
    CHECK(sb_music_shared_render(&a,NULL,0));
    CHECK(!sb_music_shared_start(&a,&song,map,2,true));
    CHECK(!sb_music_shared_voice_stop(&a,6));
    CHECK(!memcmp(&a,&frozen,sizeof(a)));
    CHECK(sb_music_shared_set_gain(&a,1) && sb_music_shared_set_gain(&ref,1));
    /* Unreserved controls are allowed while frozen; no samples advance. */
    CHECK(sb_music_shared_voice_waveform(&a,8,SB_AUDIO_VOICE_PULSE));
    CHECK(sb_music_shared_voice_waveform(&ref,8,SB_AUDIO_VOICE_PULSE));
    frozen=a;
    CHECK(sb_music_shared_render(&a,whole,100) && !memcmp(&a,&frozen,sizeof(a)));
    CHECK(sb_music_shared_pause(&a,false));b=a;
    CHECK(sb_music_shared_render(&a,whole,600));
    CHECK(sb_music_shared_render(&ref,expected,600));
    for(unsigned n=0;n<600;++n) {
        CHECK(sb_music_shared_render(&b,split+2*n,1));
        for(unsigned ch=0;ch<2;++ch)
            CHECK(whole[2*n+ch]==(float)(expected[2*n+ch]*(n<480?n/480.0:1)));
    }
    CHECK(!memcmp(whole,split,1200*sizeof(float)));
    CHECK(!memcmp(&a.sequence,&ref.sequence,sizeof(a.sequence)));
    /* Fade reversal from .75 toward 1 and then .875 toward 0. */
    CHECK(sb_music_shared_pause(&a,true));
    CHECK(sb_music_shared_render(&a,whole,120) && sb_music_shared_render(&ref,expected,120));
    CHECK(sb_music_shared_pause(&a,false));
    CHECK(sb_music_shared_render(&a,whole,240) && sb_music_shared_render(&ref,expected,240));
    for(unsigned n=0;n<240;++n)for(unsigned ch=0;ch<2;++ch)
        CHECK(whole[2*n+ch]==(float)(expected[2*n+ch]*(.75+.25*n/480)));
    CHECK(a.transport.fade.value==.875);
    CHECK(sb_music_shared_pause(&a,true));
    CHECK(sb_music_shared_render(&a,whole,480) && sb_music_shared_render(&ref,expected,480));
    for(unsigned n=0;n<480;++n)for(unsigned ch=0;ch<2;++ch)
        CHECK(whole[2*n+ch]==(float)(expected[2*n+ch]*(.875-.875*n/480)));
    CHECK(!memcmp(&a.sequence,&ref.sequence,sizeof(a.sequence)));
    /* Paused stop releases only music, preserving the frozen effect exactly. */
    sb_audio_bank_t fx=a.sequence.bank;
    CHECK(sb_music_shared_stop(&a) && !a.sequence.reserved && !a.sequence.song);
    CHECK(a.transport.paused && same_voice(&a.sequence.bank,&fx,8));
    CHECK(sb_music_shared_voice_volume(&a,6,.5));
    frozen=a;
    CHECK(sb_music_shared_render(&a,whole,600) && !memcmp(&a,&frozen,sizeof(a)));
    CHECK(sb_music_shared_pause(&a,false));
    CHECK(sb_music_shared_render(&a,whole,480));
    for(unsigned n=0;n<480;++n){
        float f[2];CHECK(sb_audio_bank_render_unreserved(&fx,(1u<<5)|(1u<<2),f,1));
        for(unsigned ch=0;ch<2;++ch)CHECK(whole[2*n+ch]==(float)(f[ch]*(n/480.0)));
    }
    CHECK(same_voice(&a.sequence.bank,&fx,8));
    /* Start during pause fade: music stays completely held while the effect
     * continues through the remaining host fade. No second render of effects. */
    sb_music_shared_init(&a);CHECK(!effect(&a));
    CHECK(sb_music_shared_pause(&a,true));
    CHECK(sb_music_shared_render(&a,whole,120));fx=a.sequence.bank;
    CHECK(sb_music_shared_start(&a,&song,map,2,true));
    CHECK(sb_music_shared_set_gain(&a,0));frozen=a;b=a;
    CHECK(sb_music_shared_render(&a,whole,600));
    for(unsigned n=0;n<600;++n){
        CHECK(sb_music_shared_render(&b,split+2*n,1));
        if(n<360){
            float f[2];CHECK(sb_audio_bank_render(&fx,f,1));
            for(unsigned ch=0;ch<2;++ch)CHECK(whole[2*n+ch]==(float)(f[ch]*(1.0-(n+120)/480.0)));
        }else CHECK(whole[2*n]==0 && whole[2*n+1]==0);
    }
    CHECK(!memcmp(whole,split,1200*sizeof(float)));
    CHECK(same_voice(&a.sequence.bank,&fx,8));
    CHECK(same_voice(&a.sequence.bank,&frozen.sequence.bank,6));
    CHECK(same_voice(&a.sequence.bank,&frozen.sequence.bank,3));
    CHECK(a.sequence.frame==0 && a.sequence.next_event==0 && a.sequence.gain_left==480);
    CHECK(sb_music_shared_stop(&a) && !a.sequence.reserved);
    CHECK(same_voice(&a.sequence.bank,&fx,8));
    /* Fully paused held start and resume use ordinary opening samples. */
    CHECK(sb_music_shared_start(&a,&song,map,2,true));ref=a;
    ref.transport.paused=false;ref.transport.held_start=false;
    ref.transport.fade=(sb_audio_session_gain_t){1,1,1,0};
    CHECK(sb_music_shared_pause(&a,false));
    CHECK(sb_music_shared_render(&a,whole,600) && sb_music_shared_render(&ref,expected,600));
    for(unsigned n=0;n<600;++n)for(unsigned ch=0;ch<2;++ch)
        CHECK(whole[2*n+ch]==(float)(expected[2*n+ch]*(n<480?n/480.0:1)));
    CHECK(!memcmp(&a.sequence,&ref.sequence,sizeof(a.sequence)));
    /* Late pause-stop: effect progresses until host silence, then freezes;
     * only music resets at that boundary. */
    CHECK(sb_music_shared_pause(&a,true));CHECK(sb_music_shared_render(&a,whole,120));
    fx=a.sequence.bank;
    CHECK(sb_music_shared_stop(&a));
    CHECK(a.transport.stop_pending && a.sequence.reserved);
    CHECK(sb_music_shared_render(&a,whole,360));
    CHECK(sb_audio_bank_render_unreserved(&fx,(1u<<5)|(1u<<2),expected,360));
    CHECK(!a.sequence.reserved && !a.sequence.song && same_voice(&a.sequence.bank,&fx,8));
    frozen=a;CHECK(sb_music_shared_render(&a,whole,700) && !memcmp(&a,&frozen,sizeof(a)));
    /* Music-only reset preserves fade and effects, including a held start. */
    CHECK(sb_music_shared_start(&a,&song,map,2,true));
    sb_music_shared_reset(&a);
    CHECK(a.transport.paused && !a.transport.held_start && !a.sequence.reserved);
    CHECK(same_voice(&a.sequence.bank,&fx,8));
    CHECK(sb_music_shared_pause(&a,false));
    CHECK(sb_music_shared_start(&a,&song,map,2,true));
    CHECK(sb_music_shared_render(&a,whole,120));fx=a.sequence.bank;
    sb_audio_session_gain_t fade=a.transport.fade;
    sb_music_shared_reset(&a);
    CHECK(!memcmp(&fade,&a.transport.fade,sizeof(fade)) && same_voice(&a.sequence.bank,&fx,8));
    /* Resume during stop cannot resurrect music; effect is never stopped. */
    CHECK(sb_music_shared_start(&a,&song,map,2,true));CHECK(sb_music_shared_render(&a,whole,1));
    CHECK(sb_music_shared_pause(&a,true) && sb_music_shared_stop(&a));
    CHECK(sb_music_shared_pause(&a,false));fx=a.sequence.bank;
    CHECK(sb_music_shared_render(&a,whole,480));
    CHECK(sb_audio_bank_render_unreserved(&fx,(1u<<5)|(1u<<2),expected,480));
    CHECK(!a.sequence.reserved && !a.sequence.song && same_voice(&a.sequence.bank,&fx,8));
    /* Natural completion during a host fade also leaves effects alive. */
    CHECK(sb_music_shared_start(&a,&song,map,2,false));
    CHECK(sb_music_shared_pause(&a,true));fx=a.sequence.bank;
    CHECK(sb_music_shared_render(&a,whole,480));
    CHECK(sb_audio_bank_render_unreserved(&fx,(1u<<5)|(1u<<2),expected,480));
    CHECK(!a.sequence.reserved && a.transport.paused && same_voice(&a.sequence.bank,&fx,8));
    printf("Shared host owner: %zu bytes\n",sizeof(a));
    puts("Shared host pause checks passed");return 0;
}
