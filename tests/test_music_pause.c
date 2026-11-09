#include "../src/sb_music_host.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Pause line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float actual[4096], split[4096], reference[4096];
static int silence(const float *p, size_t frames) {
    for (size_t i=0; i<frames*2; ++i) if (p[i]!=0) return 0;
    return 1;
}
int main(void) {
    sb_music_instrument_t ins[] = {
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,0},.6,-.5,0,700,false},
        {SB_AUDIO_VOICE_NOISE,{0,0,1,0},.4,.5,0,2000,true}};
    sb_music_event_t events[] = {{0,1,SB_MUSIC_NOTE_ON,440},{0,2,SB_MUSIC_NOTE_ON,0},
        {64,1,SB_MUSIC_NOTE_OFF,0},{64,2,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t song={48000,2,64,ins,events,4};
    sb_music_host_t a, b;
    sb_music_host_reset(&a); sb_music_host_reset(&b);
    sb_music_sequence_t ref={0};
    CHECK(!sb_music_host_pause(NULL,true) && !sb_music_host_stop(NULL));
    CHECK(!sb_music_host_start(NULL,&song,true));
    CHECK(!sb_music_host_render(NULL,actual,1));
    CHECK(sb_music_host_start(&a,&song,true));
    CHECK(sb_music_host_start(&b,&song,true));
    CHECK(sb_music_sequence_start_loop(&ref,&song));
    CHECK(sb_music_sequence_set_gain(&a.sequence,0));
    CHECK(sb_music_sequence_set_gain(&b.sequence,0));
    CHECK(sb_music_sequence_set_gain(&ref,0));
    /* Pause after a partial music gain ramp. Independent host fade formula,
     * exact samples and split renders across many loop boundaries. */
    CHECK(sb_music_host_render(&a,actual,20));
    CHECK(sb_music_host_render(&b,split,20));
    CHECK(sb_music_sequence_render(&ref,reference,20));
    CHECK(!memcmp(actual,reference,40*sizeof(float)));
    CHECK(sb_music_host_pause(&a,true) && sb_music_host_pause(&b,true));
    CHECK(sb_music_host_render(&a,actual,720));
    for(unsigned i=0;i<720;++i) {
        CHECK(sb_music_host_pause(&b,true));
        CHECK(sb_music_host_render(&b,split+2*i,1));
    }
    CHECK(!memcmp(actual,split,1440*sizeof(float)));
    CHECK(sb_music_sequence_render(&ref,reference,480));
    for(unsigned n=0;n<480;++n) for(unsigned ch=0;ch<2;++ch)
        CHECK(actual[2*n+ch]==(float)(reference[2*n+ch]*(1.0-(double)n/480)));
    CHECK(silence(actual+960,240));
    CHECK(!memcmp(&a.sequence,&ref,sizeof(ref)));
    sb_music_host_t frozen=a;
    CHECK(sb_music_host_render(&a,actual,2048) && silence(actual,2048));
    CHECK(!memcmp(&a,&frozen,sizeof(a))); /* Includes phase, filter, RNG, gain. */
    CHECK(sb_music_host_render(&a,NULL,0));
    CHECK(!sb_music_host_render(&a,NULL,1));
    CHECK(!sb_music_host_render(&a,actual,SIZE_MAX));
    CHECK(!sb_music_host_start(&a,&song,true));
    CHECK(!memcmp(&a,&frozen,sizeof(a)));
    /* A gain change while frozen is accepted, but its ramp advances on resume. */
    CHECK(sb_music_sequence_set_gain(&a.sequence,1));
    CHECK(sb_music_sequence_set_gain(&ref,1));
    frozen=a;
    CHECK(sb_music_host_render(&a,actual,100) && !memcmp(&a,&frozen,sizeof(a)));
    CHECK(sb_music_host_pause(&a,false));
    CHECK(sb_music_host_render(&a,actual,600));
    CHECK(sb_music_sequence_render(&ref,reference,600));
    for(unsigned n=0;n<600;++n) for(unsigned ch=0;ch<2;++ch)
        CHECK(actual[2*n+ch]==(float)(reference[2*n+ch]*(n<480?(double)n/480:1)));
    CHECK(!memcmp(&a.sequence,&ref,sizeof(ref)));
    /* Reversal from .75 goes toward 1 over 480 frames, not from zero. */
    CHECK(sb_music_host_pause(&a,true));
    CHECK(sb_music_host_render(&a,actual,120));
    CHECK(sb_music_sequence_render(&ref,reference,120));
    CHECK(a.fade.value==.75);
    CHECK(sb_music_host_pause(&a,false));
    b=a;
    CHECK(sb_music_host_render(&a,actual,600));
    CHECK(sb_music_sequence_render(&ref,reference,600));
    for(unsigned n=0;n<600;++n) {
        CHECK(sb_music_host_pause(&b,false));
        CHECK(sb_music_host_render(&b,split+2*n,1));
        for(unsigned ch=0;ch<2;++ch)
            CHECK(actual[2*n+ch]==(float)(reference[2*n+ch]*(n<480?.75+.25*n/480:1)));
    }
    CHECK(!memcmp(actual,split,1200*sizeof(float)));
    /* Pause again during fade-in, from current .875, and freeze exactly. */
    CHECK(sb_music_host_pause(&a,true));
    CHECK(sb_music_host_render(&a,actual,480));
    CHECK(sb_music_host_pause(&a,false));
    CHECK(sb_music_host_render(&a,actual,420));
    CHECK(a.fade.value==.875);
    CHECK(sb_music_host_pause(&a,true));
    CHECK(sb_music_host_render(&a,actual,480));
    CHECK(a.fade.value==0 && !a.fade.left);
    CHECK(sb_music_host_stop(&a));
    CHECK(a.sequence.state==SB_MUSIC_IDLE && !a.sequence.song);
    CHECK(sb_music_host_stop(&a) && sb_music_host_pause(&a,false));
    CHECK(sb_music_host_render(&a,actual,600) && silence(actual,600));
    /* Start during pause fade must not dispatch opening events. Failed starts
     * are atomic. A held start can be silently stopped before resume. */
    sb_music_host_reset(&a);
    CHECK(sb_music_host_pause(&a,true));
    frozen=a;
    CHECK(!sb_music_host_start(&a,NULL,false) && !memcmp(&a,&frozen,sizeof(a)));
    CHECK(sb_music_host_start(&a,&song,true));
    CHECK(a.sequence.state==SB_MUSIC_PLAYING && a.held_start);
    ref=a.sequence;
    CHECK(sb_music_host_render(&a,actual,600) && silence(actual,600));
    CHECK(!memcmp(&a.sequence,&ref,sizeof(ref)));
    CHECK(sb_music_host_stop(&a) && a.sequence.state==SB_MUSIC_IDLE);
    CHECK(sb_music_host_start(&a,&song,true));
    CHECK(sb_music_host_pause(&a,false));
    CHECK(sb_music_host_render(&a,actual,600));
    CHECK(sb_music_sequence_render(&ref,reference,600));
    for(unsigned n=0;n<600;++n) for(unsigned ch=0;ch<2;++ch)
        CHECK(actual[2*n+ch]==(float)(reference[2*n+ch]*(n<480?(double)n/480:1)));
    /* Stop late in pause fade: clean up at silence, before the independent
     * voice stop fade could finish. Repeated requests cannot postpone it. */
    CHECK(sb_music_host_pause(&a,true));
    CHECK(sb_music_host_render(&a,actual,350));
    CHECK(sb_music_host_stop(&a));
    CHECK(a.stop_pending && !a.sequence.loop && a.sequence.state==SB_MUSIC_FINISHING);
    frozen=a;
    CHECK(!sb_music_host_start(&a,&song,true) && !memcmp(&a,&frozen,sizeof(a)));
    for(unsigned n=0;n<129;++n) {
        CHECK(sb_music_host_stop(&a) && sb_music_host_render(&a,actual,1));
    }
    CHECK(a.sequence.state==SB_MUSIC_FINISHING);
    CHECK(sb_music_host_render(&a,actual,1));
    CHECK(a.sequence.state==SB_MUSIC_IDLE && !a.sequence.song && !a.stop_pending);
    CHECK(sb_music_host_pause(&a,false));
    CHECK(sb_music_host_render(&a,actual,600) && silence(actual,600));
    /* Resume before a latched stop completes: it still drains, never restarts. */
    CHECK(sb_music_host_start(&a,&song,true));
    CHECK(sb_music_host_render(&a,actual,1));
    CHECK(sb_music_host_pause(&a,true));
    CHECK(sb_music_host_render(&a,actual,120));
    CHECK(sb_music_host_stop(&a) && sb_music_host_pause(&a,false));
    CHECK(sb_music_host_render(&a,actual,480));
    CHECK(a.sequence.state==SB_MUSIC_IDLE && !a.sequence.song && !a.stop_pending);
    /* Natural release remains frozen as well, then can stop while paused. */
    ins[0].envelope.release=ins[1].envelope.release=.1;
    CHECK(sb_music_host_start(&a,&song,false));
    CHECK(sb_music_host_render(&a,actual,64));
    CHECK(a.sequence.state==SB_MUSIC_FINISHING);
    CHECK(sb_music_host_pause(&a,true));
    CHECK(sb_music_host_render(&a,actual,480));
    frozen=a;
    CHECK(sb_music_host_render(&a,actual,2048) && !memcmp(&a,&frozen,sizeof(a)));
    CHECK(sb_music_host_stop(&a) && !a.sequence.song);
    sb_music_host_reset(&a);
    CHECK(a.fade.value==1 && !a.paused && !a.sequence.song && !a.stop_pending);
    puts("Music host pause checks passed");
    return 0;
}
