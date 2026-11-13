#include "../src/sb_music_sequence.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Sequence line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float whole[8192], split[8192], reference[8192];
static sb_music_instrument_t instrument(sb_audio_voice_kind_t wave) {
    sb_music_instrument_t i = {wave, {0,0,1,0}, .5, 0, .5, 2000, true};
    return i;
}
static bool setup(sb_audio_bank_t *b, unsigned v, const sb_music_instrument_t *i) {
    return sb_audio_bank_waveform(b,v,i->waveform) &&
        sb_audio_bank_configure(b,v,&i->envelope) &&
        sb_audio_bank_volume(b,v,i->volume) && sb_audio_bank_pan(b,v,i->pan) &&
        sb_audio_bank_pulse_width(b,v,i->pulse_width) &&
        sb_audio_bank_filter_cutoff(b,v,i->filter_cutoff) &&
        sb_audio_bank_filter_bypass(b,v,i->filter_bypass);
}
int main(void) {
    sb_music_sequence_t a = {0}, b = {0};
    sb_music_instrument_t ins[3] = {instrument(SB_AUDIO_VOICE_TRIANGLE),
        instrument(SB_AUDIO_VOICE_SAW), instrument(SB_AUDIO_VOICE_PULSE)};
    ins[0].envelope.release = .002;
    ins[1].filter_bypass = false;
    ins[1].filter_cutoff = 700;
    sb_music_event_t events[] = {
        {17,1,SB_MUSIC_NOTE_ON,220}, {17,2,SB_MUSIC_NOTE_ON,275}, {17,3,SB_MUSIC_NOTE_ON,330},
        {117,1,SB_MUSIC_NOTE_OFF,0}, {117,2,SB_MUSIC_NOTE_OFF,0}, {117,3,SB_MUSIC_NOTE_OFF,0},
        {117,1,SB_MUSIC_NOTE_ON,440}, {317,1,SB_MUSIC_NOTE_OFF,0}
    };
    sb_music_song_t song = {48000,3,317,ins,events,8};
    CHECK(sb_music_sequence_render(&a,whole,4096));
    for (unsigned n=0;n<8192;++n) CHECK(whole[n]==0);
    CHECK(sb_music_sequence_start(&a,&song));
    unsigned char snapshot[sizeof(a)]; memcpy(snapshot,&a,sizeof(a));
    CHECK(!sb_music_sequence_start(&a,&song));
    CHECK(!sb_music_sequence_render(&a,NULL,1));
    CHECK(!sb_music_sequence_render(&a,whole,SIZE_MAX));
    CHECK(sb_music_sequence_render(&a,NULL,0));
    CHECK(memcmp(snapshot,&a,sizeof(a))==0);
    CHECK(sb_music_sequence_start(&b,&song));
    CHECK(sb_music_sequence_render(&a,whole,4096));
    for (size_t pos=0;pos<4096;) {
        size_t count=1+(pos*13)%71;
        if(count>4096-pos) count=4096-pos;
        CHECK(sb_music_sequence_render(&b,split+2*pos,count)); pos+=count;
    }
    CHECK(memcmp(whole,split,sizeof(whole))==0);
    CHECK(a.state==SB_MUSIC_IDLE && b.state==SB_MUSIC_IDLE && !a.song);
    /* Independent manual schedule: simultaneous chord, same-frame off/on,
     * changed pitch, and final release exactly on the declared boundary. */
    sb_audio_bank_t bank={0};
    for(unsigned v=1;v<=3;++v) CHECK(setup(&bank,v,&ins[v-1]));
    for(unsigned n=0;n<4096;++n) {
        if(n==17) {
            CHECK(sb_audio_bank_start_selected(&bank,1,220));
            CHECK(sb_audio_bank_start_selected(&bank,2,275));
            CHECK(sb_audio_bank_start_selected(&bank,3,330));
        }
        if(n==117) {
            for(unsigned v=1;v<=3;++v) CHECK(sb_audio_bank_note_off(&bank,v));
            CHECK(sb_audio_bank_start_selected(&bank,1,440));
        }
        if(n==317) CHECK(sb_audio_bank_note_off(&bank,1));
        CHECK(sb_audio_bank_render(&bank,reference+2*n,1));
    }
    CHECK(memcmp(whole,reference,sizeof(whole))==0);
    double energy=0;
    for(unsigned n=0;n<8192;++n) {
        CHECK(isfinite(whole[n]) && fabs(whole[n])<=1); energy+=fabs(whole[n]);
        if(n<34 || n>=2*(317+96)) CHECK(whole[n]==0);
    }
    CHECK(energy>1);
    CHECK(sb_music_sequence_start(&a,&song));
    CHECK(sb_music_sequence_render(&a,whole,317));
    CHECK(a.state==SB_MUSIC_FINISHING && a.next_event==8);
    CHECK(sb_music_sequence_render(&a,whole,95)); CHECK(a.state==SB_MUSIC_FINISHING);
    CHECK(sb_music_sequence_render(&a,whole,1)); CHECK(a.state==SB_MUSIC_IDLE);
    CHECK(!a.bank.voices[0].configured); /* Silent defaults after finish. */

    /* Rejections preserve an idle sequence, including errors late in a song. */
    memcpy(snapshot,&a,sizeof(a));
#define BAD(change, undo) do { change; CHECK(!sb_music_sequence_start(&a,&song)); \
    CHECK(memcmp(snapshot,&a,sizeof(a))==0); undo; } while(0)
    BAD(song.timeline_rate=44100, song.timeline_rate=48000);
    BAD(song.track_count=10, song.track_count=3);
    BAD(song.end_frame=0, song.end_frame=317);
    BAD(song.end_frame=316, song.end_frame=317);
    BAD(song.event_count=4097, song.event_count=8);
    BAD(song.events=NULL, song.events=events);
    BAD(song.instruments=NULL, song.instruments=ins);
    BAD(ins[2].volume=NAN, ins[2].volume=.5);
    BAD(ins[2].pulse_width=1, ins[2].pulse_width=.5);
    BAD(ins[2].waveform=(sb_audio_voice_kind_t)99, ins[2].waveform=SB_AUDIO_VOICE_PULSE);
    BAD(ins[2].envelope.release=11, ins[2].envelope.release=0);
    BAD(events[7].track=0, events[7].track=1);
    BAD(events[7].hz=1, events[7].hz=0);
    BAD(events[7].kind=SB_MUSIC_NOTE_ON, events[7].kind=SB_MUSIC_NOTE_OFF);
    BAD(events[6].hz=INFINITY, events[6].hz=440);
    BAD(events[6].hz=27.499, events[6].hz=440);
    BAD(events[6].frame=116, events[6].frame=117);
    BAD(events[2].track=2, events[2].track=3);
    BAD(events[0].kind=SB_MUSIC_NOTE_OFF, events[0].kind=SB_MUSIC_NOTE_ON);
    BAD(song.event_count=7, song.event_count=8);
    /* Same-frame off then on without a prior held note is invalid too. */
    sb_music_event_t short_events[]={{0,1,SB_MUSIC_NOTE_ON,440},{1,1,SB_MUSIC_NOTE_OFF,0}};
    song=(sb_music_song_t){48000,1,1,ins,short_events,2};
    BAD(short_events[1].frame=0, short_events[1].frame=1);
    CHECK(sb_music_sequence_start(&a,&song));
    sb_music_sequence_reset(&a);
    /* Noise, zero release, and one-frame song ending exactly at a buffer edge. */
    ins[0]=instrument(SB_AUDIO_VOICE_NOISE); short_events[0].hz=0;
    CHECK(sb_music_sequence_start(&a,&song));
    CHECK(sb_music_sequence_render(&a,whole,1)); CHECK(a.state==SB_MUSIC_IDLE);
    CHECK(whole[0]!=0);
    memcpy(snapshot,&a,sizeof(a));
    BAD(short_events[0].hz=440, short_events[0].hz=0);
    /* Empty prototype songs preserve their explicit silence duration. */
    song.events=NULL; song.event_count=0; song.end_frame=100;
    CHECK(sb_music_sequence_start(&a,&song));
    CHECK(sb_music_sequence_render(&a,whole,99)); CHECK(a.state==SB_MUSIC_PLAYING);
    CHECK(sb_music_sequence_render(&a,whole,1)); CHECK(a.state==SB_MUSIC_IDLE);
    /* Sustained-zero envelope may finish before its authored note-off. */
    song.events=short_events; song.event_count=2;
    ins[0].envelope.sustain=0;
    CHECK(sb_music_sequence_start(&a,&song));
    CHECK(sb_music_sequence_render(&a,whole,100)); CHECK(a.state==SB_MUSIC_IDLE);
    CHECK(!sb_music_sequence_start(NULL,&song));
    CHECK(!sb_music_sequence_start(&a,NULL));
    CHECK(!sb_music_sequence_render(NULL,whole,1));
    sb_music_sequence_reset(NULL);
    /* Expanded endpoints traverse sequencer validation and actual rendering. */
    const double good_hz[]={27.5,4186.009044809578,4186.01};
    ins[0]=instrument(SB_AUDIO_VOICE_PULSE);
    short_events[1]=(sb_music_event_t){1024,1,SB_MUSIC_NOTE_OFF,0};
    song=(sb_music_song_t){48000,1,1024,ins,short_events,2};
    for (unsigned i=0;i<3;++i) {
        sb_music_sequence_reset(&a);
        short_events[0]=(sb_music_event_t){0,1,SB_MUSIC_NOTE_ON,good_hz[i]};
        CHECK(sb_music_sequence_start(&a,&song));
        CHECK(sb_music_sequence_render(&a,whole,512));
        CHECK(a.bank.retained_pitch[0]==good_hz[i]);
        for (unsigned j=0;j<1024;++j) CHECK(isfinite(whole[j]) && fabs(whole[j])<=1);
    }
    sb_music_sequence_reset(&a);
    memcpy(snapshot,&a,sizeof(a));
    BAD(short_events[0].hz=nextafter(27.5,-INFINITY),short_events[0].hz=440);
    BAD(short_events[0].hz=nextafter(4186.01,INFINITY),short_events[0].hz=440);
    puts("Prepared music sequence checks passed.");
    return 0;
}
