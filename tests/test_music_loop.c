#include "../src/sb_music_sequence.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Loop line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float reference[128], actual[8192], split[8192];
int main(void) {
    sb_music_instrument_t ins[] = {
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,.001},.6,-.5,0,700,false},
        {SB_AUDIO_VOICE_NOISE,{0,0,1,.001},.4,.5,0,2000,false},
        {SB_AUDIO_VOICE_PULSE,{0,0,1,0},.5,0,.3,1200,false}};
    sb_music_event_t events[] = {
        {0,1,SB_MUSIC_NOTE_ON,440},{0,2,SB_MUSIC_NOTE_ON,0},{0,3,SB_MUSIC_NOTE_ON,220},
        {4,1,SB_MUSIC_NOTE_OFF,0},{4,2,SB_MUSIC_NOTE_OFF,0},
        {4,1,SB_MUSIC_NOTE_ON,330},{4,2,SB_MUSIC_NOTE_ON,0},
        {16,1,SB_MUSIC_NOTE_OFF,0},{16,2,SB_MUSIC_NOTE_OFF,0},
        {64,3,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t song={48000,3,64,ins,events,10};
    sb_music_sequence_t once={0}, loop={0}, pieces={0};
    CHECK(sb_music_sequence_loop_eligible(&song));
    CHECK(sb_music_sequence_start(&once,&song));
    CHECK(sb_music_sequence_render(&once,reference,64) && once.state==SB_MUSIC_IDLE);
    CHECK(sb_music_sequence_start_loop(&loop,&song));
    CHECK(sb_music_sequence_start_loop(&pieces,&song));
    CHECK(sb_music_sequence_render(&loop,actual,4096));
    for(size_t n=0;n<4096;) {
        size_t block=1+n%83; if(block>4096-n) block=4096-n;
        CHECK(sb_music_sequence_render(&pieces,split+2*n,block)); n+=block;
    }
    CHECK(!memcmp(actual,split,sizeof(actual)));
    for(size_t n=0;n<64;++n) CHECK(!memcmp(reference,actual+n*128,sizeof(reference)));
    CHECK(loop.frame==0 && loop.next_event==0 && loop.state==SB_MUSIC_PLAYING);
    CHECK(!memcmp(&loop.bank,&loop.initial_bank,sizeof(loop.bank)));
    sb_music_sequence_t saved=loop;
    CHECK(!sb_music_sequence_start_loop(&loop,&song));
    CHECK(!sb_music_sequence_start(&loop,&song));
    CHECK(!sb_music_sequence_render(&loop,NULL,1));
    CHECK(!sb_music_sequence_render(&loop,actual,SIZE_MAX));
    CHECK(sb_music_sequence_render(&loop,NULL,0));
    CHECK(!memcmp(&loop,&saved,sizeof(loop)));
    /* Exact endpoint reset and frame-zero dispatch across separate calls. */
    CHECK(sb_music_sequence_render(&loop,actual,1));
    CHECK(!memcmp(actual,reference,2*sizeof(float)) && loop.frame==1);
    sb_music_sequence_reset(&loop);
    CHECK(sb_music_sequence_render(&loop,actual,64));
    for(size_t n=0;n<128;++n) CHECK(actual[n]==0);
    CHECK(loop.state==SB_MUSIC_IDLE && !loop.song && !loop.loop);
    /* Last release one frame late refuses atomically, but one-shot works. */
    ins[0].envelope.release=49.0/48000;
    saved=loop;
    CHECK(!sb_music_sequence_loop_eligible(&song));
    CHECK(!sb_music_sequence_start_loop(&loop,&song));
    CHECK(!memcmp(&loop,&saved,sizeof(loop)));
    CHECK(sb_music_sequence_start(&loop,&song));
    CHECK(sb_music_sequence_render(&loop,actual,64) && loop.state==SB_MUSIC_FINISHING);
    CHECK(sb_music_sequence_render(&loop,actual,1) && loop.state==SB_MUSIC_IDLE);
    /* Invalid/null inputs and one-frame silent loops. */
    song.timeline_rate=1;
    saved=loop;
    CHECK(!sb_music_sequence_start_loop(&loop,&song));
    CHECK(!sb_music_sequence_start_loop(&loop,NULL));
    CHECK(!sb_music_sequence_start_loop(NULL,&song));
    CHECK(!sb_music_sequence_loop_eligible(NULL));
    CHECK(!memcmp(&loop,&saved,sizeof(loop)));
    song=(sb_music_song_t){48000,3,1,ins,NULL,0};
    CHECK(sb_music_sequence_start_loop(&loop,&song));
    CHECK(sb_music_sequence_render(&loop,actual,4096));
    for(size_t n=0;n<8192;++n) CHECK(actual[n]==0);
    CHECK(loop.frame==0 && loop.state==SB_MUSIC_PLAYING);
    printf("Loop checks passed; bank snapshot %zu bytes, sequencer %zu bytes\n",
           sizeof(sb_audio_bank_t),sizeof(sb_music_sequence_t));
    return 0;
}
