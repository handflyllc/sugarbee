#include "../src/sb_music_sequence.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Stop line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float actual[960], expected[960];
static int scenario(unsigned at, bool looping, double release, unsigned end) {
    sb_music_instrument_t ins[] = {
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,release},.6,0,0,700,false},
        {SB_AUDIO_VOICE_NOISE,{0,0,1,release},.5,0,0,2000,true}};
    sb_music_event_t events[] = {{0,1,SB_MUSIC_NOTE_ON,440},{0,2,SB_MUSIC_NOTE_ON,0},
        {100,1,SB_MUSIC_NOTE_OFF,0},{100,2,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t song={48000,2,end,ins,events,4};
    sb_music_sequence_t s={0};
    CHECK(looping?sb_music_sequence_start_loop(&s,&song):sb_music_sequence_start(&s,&song));
    for(unsigned n=0;n<at;++n) CHECK(sb_music_sequence_render(&s,actual,1));
    sb_audio_bank_t bank=s.bank;
    bool active=sb_audio_bank_active(&bank,1)||sb_audio_bank_active(&bank,2);
    CHECK(sb_music_sequence_stop(&s));
    CHECK(!s.loop);
    if(active) {
        CHECK(s.state==SB_MUSIC_FINISHING && s.song==&song);
        sb_music_sequence_t snapshot=s;
        CHECK(!sb_music_sequence_start(&s,&song));
        CHECK(!sb_music_sequence_start_loop(&s,&song));
        CHECK(sb_music_sequence_stop(&s));
        CHECK(sb_music_sequence_render(&s,NULL,0));
        CHECK(!memcmp(&snapshot,&s,sizeof(s)));
        CHECK(sb_audio_bank_stop(&bank,1)&&sb_audio_bank_stop(&bank,2));
        CHECK(sb_audio_bank_render(&bank,expected,480));
        /* Interleaved repeated stops must not stretch the 480-frame fade. */
        for(unsigned n=0;n<479;++n) {
            CHECK(sb_music_sequence_stop(&s));
            CHECK(sb_music_sequence_render(&s,actual+2*n,1));
            CHECK(s.state==SB_MUSIC_FINISHING);
        }
        CHECK(sb_music_sequence_render(&s,actual+958,1));
        CHECK(!memcmp(actual,expected,sizeof(actual)));
    }
    CHECK(s.state==SB_MUSIC_IDLE && !s.song);
    sb_music_sequence_t zero={0}; CHECK(!memcmp(&s,&zero,sizeof(s)));
    CHECK(sb_music_sequence_render(&s,actual,480));
    for(unsigned n=0;n<960;++n) CHECK(actual[n]==0);
    CHECK(sb_music_sequence_start(&s,&song));
    CHECK(sb_music_sequence_render(&s,actual,1));
    CHECK(sb_music_sequence_stop(&s));
    CHECK(sb_music_sequence_render(&s,actual,480) && s.state==SB_MUSIC_IDLE);
    return 0;
}
int main(void) {
    CHECK(!sb_music_sequence_stop(NULL));
    sb_music_sequence_t idle={0}; CHECK(sb_music_sequence_stop(&idle));
    CHECK(!scenario(0,true,.001,200)); /* before first note */
    CHECK(!scenario(1,true,.001,200)); /* attack */
    CHECK(!scenario(99,true,.001,200)); /* before scheduled off */
    CHECK(!scenario(101,true,.001,200)); /* release */
    CHECK(!scenario(149,true,.001,200)); /* authored gap */
    CHECK(!scenario(199,true,.001,200));
    CHECK(!scenario(200,true,.001,200)); /* reset boundary, no next on yet */
    CHECK(!scenario(201,true,.001,200)); /* next iteration */
    CHECK(!scenario(101,false,.1,100)); /* natural finishing tail */
    CHECK(!scenario(100,false,0,100)); /* already idle */
    puts("Native stop checks passed"); return 0;
}
