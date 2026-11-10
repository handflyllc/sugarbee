#include "../src/sb_music_sequence.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Gain line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float baseline[4096], whole[4096], split[4096];
int main(void) {
    sb_music_instrument_t ins[] = {
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,0},.6,-.5,0,700,false},
        {SB_AUDIO_VOICE_NOISE,{0,0,1,0},.4,.5,0,2000,true}};
    sb_music_event_t events[] = {{0,1,SB_MUSIC_NOTE_ON,440},{0,2,SB_MUSIC_NOTE_ON,0},
        {64,1,SB_MUSIC_NOTE_OFF,0},{64,2,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t song={48000,2,64,ins,events,4};
    sb_music_sequence_t a={0}, b={0}, reference={0};
    CHECK(!sb_music_sequence_set_gain(NULL,.5));
    CHECK(!sb_music_sequence_set_gain(&a,.5));
    CHECK(sb_music_sequence_start_loop(&a,&song));
    CHECK(sb_music_sequence_start_loop(&b,&song));
    CHECK(sb_music_sequence_start_loop(&reference,&song));
    CHECK(a.gain==1 && a.gain_target==1 && !a.gain_left);
    sb_music_sequence_t saved=a;
    const double invalid[]={-1,1.01,NAN,INFINITY,-INFINITY};
    for(size_t j=0;j<5;++j) {
        CHECK(!sb_music_sequence_set_gain(&a,invalid[j]));
        CHECK(!memcmp(&a,&saved,sizeof(a)));
    }
    CHECK(sb_music_sequence_render(&reference,baseline,2048));
    /* Independent gain schedule: 1 -> 0, hold mute, 0 -> 1, then retarget
     * mid-ramp to .25. It crosses many 64-frame loop boundaries. */
    const unsigned positions[]={0,480,640,880,1360,2048};
    const double targets[]={0,0,1,.25,.25};
    for(unsigned part=0;part<5;++part) {
        CHECK(sb_music_sequence_set_gain(&a,targets[part]));
        CHECK(sb_music_sequence_set_gain(&b,targets[part]));
        unsigned start=positions[part], count=positions[part+1]-start;
        CHECK(sb_music_sequence_render(&a,whole+2*start,count));
        for(unsigned n=start;n<start+count;++n) {
            CHECK(sb_music_sequence_set_gain(&b,targets[part])); /* no ramp restart */
            CHECK(sb_music_sequence_render(&b,split+2*n,1));
        }
        CHECK(a.frame==b.frame && a.gain==b.gain && a.gain_left==b.gain_left);
    }
    CHECK(!memcmp(whole,split,sizeof(whole)));
    for(unsigned n=0;n<2048;++n) {
        double gain=n<480 ? 1.0-(double)n/480 : n<640 ? 0 :
            n<880 ? (double)(n-640)/480 : n<1360 ? .5+(.25-.5)*(n-880)/480 : .25;
        CHECK(whole[2*n]==(float)(baseline[2*n]*gain));
        CHECK(whole[2*n+1]==(float)(baseline[2*n+1]*gain));
    }
    CHECK(a.state==SB_MUSIC_PLAYING && a.frame==0 && a.gain==.25);
    CHECK(sb_music_sequence_set_gain(&a,1)); saved=a;
    CHECK(sb_music_sequence_render(&a,NULL,0));
    CHECK(!sb_music_sequence_render(&a,NULL,1));
    CHECK(!sb_music_sequence_render(&a,whole,SIZE_MAX));
    CHECK(!memcmp(&a,&saved,sizeof(a)));
    /* Replay restores full gain, even after a muted previous playback. */
    sb_music_sequence_reset(&a);
    CHECK(a.gain==0 && a.gain_target==0 && !a.gain_left);
    CHECK(sb_music_sequence_start_loop(&a,&song) && a.gain==1);
    CHECK(sb_music_sequence_render(&a,whole,64));
    CHECK(!memcmp(whole,baseline,128*sizeof(float)));
    /* Stop during a ramp: the envelope fade and output gain are independent. */
    sb_music_sequence_reset(&a); sb_music_sequence_reset(&b);
    CHECK(sb_music_sequence_start_loop(&a,&song)&&sb_music_sequence_start_loop(&b,&song));
    CHECK(sb_music_sequence_render(&a,whole,1)&&sb_music_sequence_render(&b,split,1));
    CHECK(sb_music_sequence_set_gain(&a,0));
    CHECK(sb_music_sequence_stop(&a)&&sb_music_sequence_stop(&b));
    CHECK(sb_music_sequence_render(&a,whole,240)&&sb_music_sequence_render(&b,baseline,240));
    for(unsigned n=0;n<240;++n) for(unsigned ch=0;ch<2;++ch)
        CHECK(whole[2*n+ch]==(float)(baseline[2*n+ch]*(1.0-(double)n/480)));
    CHECK(a.state==SB_MUSIC_FINISHING && a.gain==.5);
    CHECK(sb_music_sequence_set_gain(&a,1)); /* gain allowed during finishing */
    CHECK(sb_music_sequence_render(&a,whole,240)&&sb_music_sequence_render(&b,baseline,240));
    for(unsigned n=0;n<240;++n) for(unsigned ch=0;ch<2;++ch)
        CHECK(whole[2*n+ch]==(float)(baseline[2*n+ch]*(.5+.5*n/480)));
    CHECK(a.state==SB_MUSIC_IDLE && !a.song && !a.gain_left && a.gain==0);
    CHECK(!sb_music_sequence_set_gain(&a,1));
    CHECK(sb_music_sequence_start(&a,&song) && a.gain==1);
    CHECK(sb_music_sequence_render(&a,whole,64) && a.state==SB_MUSIC_IDLE);
    puts("Music gain checks passed"); return 0;
}
