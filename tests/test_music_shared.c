#include "../src/sb_music_shared.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Shared line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float out[4096], split[4096];
static int same_voice(const sb_audio_bank_t *a,const sb_audio_bank_t *b,unsigned v) {
    sb_audio_bank_t aa={0},bb={0};
    sb_audio_bank_copy_voice(&aa,v,a,v); sb_audio_bank_copy_voice(&bb,v,b,v);
    return !memcmp(&aa,&bb,sizeof(aa));
}
static int guarded(sb_music_shared_t *s,unsigned v) {
    sb_music_shared_t before=*s;
    sb_envelope_settings_t env={0,0,1,0};
    CHECK(!sb_music_shared_voice_configure(s,v,&env));
    CHECK(!sb_music_shared_voice_note_on(s,v,440));
    CHECK(!sb_music_shared_voice_noise_on(s,v));
    CHECK(!sb_music_shared_voice_saw_on(s,v,440));
    CHECK(!sb_music_shared_voice_pulse_on(s,v,440));
    CHECK(!sb_music_shared_voice_waveform(s,v,SB_AUDIO_VOICE_PULSE));
    CHECK(!sb_music_shared_voice_start_selected(s,v,440));
    CHECK(!sb_music_shared_voice_start_selected_noise(s,v));
    CHECK(!sb_music_shared_voice_pitch(s,v,220));
    CHECK(!sb_music_shared_voice_volume(s,v,.2));
    CHECK(!sb_music_shared_voice_pan(s,v,.1));
    CHECK(!sb_music_shared_voice_pulse_width(s,v,.3));
    CHECK(!sb_music_shared_voice_seed(s,v,123));
    CHECK(!sb_music_shared_voice_filter_cutoff(s,v,600));
    CHECK(!sb_music_shared_voice_filter_bypass(s,v,false));
    CHECK(!sb_music_shared_voice_note_off(s,v));
    CHECK(!sb_music_shared_voice_stop(s,v));
    CHECK(!memcmp(s,&before,sizeof(*s)));
    return 0;
}
int main(void) {
    sb_music_shared_t s,b;
    sb_music_shared_init(&s);
    sb_music_instrument_t ins[]={
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,0},.6,-.4,0,700,false},
        {SB_AUDIO_VOICE_NOISE,{0,0,1,0},.3,.4,0,2000,true}};
    sb_music_event_t events[]={{0,1,SB_MUSIC_NOTE_ON,440},{0,2,SB_MUSIC_NOTE_ON,0},
        {64,1,SB_MUSIC_NOTE_OFF,0},{64,2,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t song={48000,2,64,ins,events,4};
    unsigned map[]={7,2};
    sb_envelope_settings_t env={0,0,1,.1};
    CHECK(!sb_music_shared_start(NULL,&song,map,2,true));
    CHECK(!sb_music_shared_stop(NULL) && !sb_music_shared_render(NULL,out,1));
    CHECK(!sb_music_shared_set_gain(&s,.5));
    /* An unrelated effect carries nontrivial oscillator/filter/switch state. */
    CHECK(sb_music_shared_voice_configure(&s,9,&env));
    CHECK(sb_music_shared_voice_seed(&s,9,876));
    CHECK(sb_music_shared_voice_volume(&s,9,.4));
    CHECK(sb_music_shared_voice_filter_cutoff(&s,9,750));
    CHECK(sb_music_shared_voice_filter_bypass(&s,9,false));
    CHECK(sb_music_shared_voice_noise_on(&s,9));
    CHECK(sb_music_shared_render(&s,out,27));
    CHECK(sb_music_shared_voice_waveform(&s,9,SB_AUDIO_VOICE_PULSE));
    CHECK(sb_music_shared_voice_waveform(&s,8,SB_AUDIO_VOICE_SAW));
    CHECK(sb_music_shared_voice_pulse_width(&s,8,.23));
    CHECK(sb_music_shared_voice_pan(&s,8,.7));
    CHECK(sb_music_shared_voice_seed(&s,8,4321));
    sb_audio_bank_t effect=s.sequence.bank;
    /* Dirty idle settings on music slots must not leak into the new song. */
    CHECK(sb_music_shared_voice_seed(&s,7,999));
    CHECK(sb_music_shared_voice_pan(&s,2,-1));
    sb_music_shared_t before=s;
    unsigned invalid[][2]={{7,7},{0,2},{7,10},{7,9}};
    for(unsigned i=0;i<4;++i) {
        CHECK(!sb_music_shared_start(&s,&song,invalid[i],2,true));
        CHECK(!memcmp(&s,&before,sizeof(s)));
    }
    CHECK(!sb_music_shared_start(&s,&song,map,1,true));
    CHECK(!sb_music_shared_start(&s,&song,NULL,2,true));
    double saved=ins[1].volume; ins[1].volume=2;
    CHECK(!sb_music_shared_start(&s,&song,map,2,true));ins[1].volume=saved;
    CHECK(!memcmp(&s,&before,sizeof(s)));
    CHECK(sb_music_shared_start(&s,&song,map,2,true));
    CHECK(s.sequence.voice_map[0]==7 && s.sequence.voice_map[1]==2);
    CHECK(sb_music_shared_reserved(&s,7) && sb_music_shared_reserved(&s,2));
    CHECK(!sb_music_shared_reserved(&s,9) && same_voice(&s.sequence.bank,&effect,9));
    before=s;
    CHECK(!sb_music_shared_start(&s,&song,map,2,true));
    CHECK(!memcmp(&s,&before,sizeof(s)));
    CHECK(!guarded(&s,7) && !guarded(&s,2));
    CHECK(!guarded(&s,0) && !guarded(&s,10));
    map[0]=1; /* Mapping is copied, not borrowed. */
    CHECK(s.sequence.voice_map[0]==7); map[0]=7;
    b=s;
    sb_audio_bank_t music=s.sequence.initial_bank, initial=music;
    CHECK(sb_music_shared_set_gain(&s,0) && sb_music_shared_set_gain(&b,0));
    CHECK(sb_music_shared_render(&s,out,1024));
    for(unsigned n=0;n<1024;++n) {
        float m[2],fx[2];
        if(n%64==0) {
            music=initial;
            CHECK(sb_audio_bank_start_selected(&music,7,440));
            CHECK(sb_audio_bank_start_selected_noise(&music,2));
        }
        CHECK(sb_audio_bank_render(&music,m,1) && sb_audio_bank_render(&effect,fx,1));
        double gain=n<480?1.0-(double)n/480:0;
        CHECK(sb_music_shared_render(&b,split+2*n,1));
        for(unsigned ch=0;ch<2;++ch)
            CHECK(fabs(out[2*n+ch]-(float)(m[ch]*gain+fx[ch]))<2e-8);
    }
    CHECK(!memcmp(out,split,2048*sizeof(float)));
    CHECK(same_voice(&s.sequence.bank,&effect,9));
    CHECK(same_voice(&s.sequence.bank,&effect,8));
    CHECK(s.sequence.state==SB_MUSIC_PLAYING && s.sequence.frame==0);
    CHECK(!guarded(&s,7)); /* Reserved through a silent boundary/mute. */
    CHECK(sb_music_shared_voice_pitch(&s,9,330));
    CHECK(sb_audio_bank_pitch(&effect,9,330));
    CHECK(sb_music_shared_set_gain(&s,1));
    CHECK(sb_music_shared_render(&s,out,1) && sb_audio_bank_render(&effect,split,1));
    CHECK(sb_music_shared_stop(&s));
    CHECK(s.sequence.state==SB_MUSIC_FINISHING && sb_music_shared_reserved(&s,7));
    CHECK(!guarded(&s,2));
    for(unsigned n=0;n<480;++n) {
        CHECK(sb_music_shared_stop(&s));
        CHECK(sb_music_shared_render(&s,out,1));
        CHECK(sb_audio_bank_render(&effect,split,1));
        CHECK(same_voice(&s.sequence.bank,&effect,9));
    }
    CHECK(s.sequence.state==SB_MUSIC_IDLE && !s.sequence.song && !s.sequence.reserved);
    sb_audio_bank_t empty={0};
    CHECK(same_voice(&s.sequence.bank,&empty,7) && same_voice(&s.sequence.bank,&empty,2));
    CHECK(sb_music_shared_voice_volume(&s,7,.5));
    CHECK(sb_music_shared_start(&s,&song,map,2,true));
    before=s;
    CHECK(sb_music_shared_render(&s,NULL,0));
    CHECK(!sb_music_shared_render(&s,NULL,1));
    CHECK(!sb_music_shared_render(&s,out,SIZE_MAX));
    CHECK(!memcmp(&s,&before,sizeof(s)));
    sb_music_shared_reset(&s);
    CHECK(same_voice(&s.sequence.bank,&effect,9) && !s.sequence.song && !s.sequence.reserved);
    /* Sounding, releasing and stopping requested voices all refuse atomically. */
    CHECK(sb_music_shared_voice_configure(&s,7,&env));
    CHECK(sb_music_shared_voice_note_on(&s,7,440));
    for(unsigned phase=0;phase<3;++phase) {
        before=s;
        CHECK(!sb_music_shared_start(&s,&song,map,2,false));
        CHECK(!memcmp(&s,&before,sizeof(s)));
        CHECK(phase==0?sb_music_shared_voice_note_off(&s,7):sb_music_shared_voice_stop(&s,7));
    }
    CHECK(sb_music_shared_render(&s,out,480));
    CHECK(sb_music_shared_start(&s,&song,map,2,false));
    CHECK(sb_music_shared_render(&s,out,64));
    CHECK(!s.sequence.reserved && s.sequence.state==SB_MUSIC_IDLE && !s.sequence.song);
    /* Positive release tails retain ownership; one-shot accepts ineligible loop. */
    ins[0].envelope.release=.01;
    before=s;
    CHECK(!sb_music_shared_start(&s,&song,map,2,true));
    CHECK(!memcmp(&s,&before,sizeof(s)));
    CHECK(sb_music_shared_start(&s,&song,map,2,false));
    CHECK(sb_music_shared_render(&s,out,64));
    CHECK(s.sequence.state==SB_MUSIC_FINISHING && sb_music_shared_reserved(&s,7));
    CHECK(!guarded(&s,7));
    CHECK(sb_music_shared_render(&s,out,480));
    CHECK(s.sequence.state==SB_MUSIC_IDLE && !s.sequence.reserved);
    /* Empty songs still reserve each declared track and release at duration. */
    song.events=NULL;song.event_count=0;
    CHECK(sb_music_shared_start(&s,&song,map,2,false));
    CHECK(sb_music_shared_reserved(&s,2));
    CHECK(sb_music_shared_render(&s,out,64));
    CHECK(!s.sequence.reserved);
    /* Full nine-voice capacity, reversed map, with no extra pool invented. */
    sb_music_shared_init(&s);
    sb_music_instrument_t nine[9];unsigned all[9];
    for(unsigned n=0;n<9;++n){nine[n]=ins[0];all[n]=9-n;}
    song.track_count=9;song.instruments=nine;
    CHECK(sb_music_shared_start(&s,&song,all,9,true));
    CHECK(s.sequence.reserved==511);
    for(unsigned n=1;n<=9;++n)CHECK(!guarded(&s,n));
    CHECK(sb_music_shared_stop(&s) && !s.sequence.reserved);
    printf("Shared owner: %zu bytes; bank: %zu bytes\n",sizeof(s),sizeof(s.sequence.bank));
    puts("Shared music ownership checks passed");return 0;
}
