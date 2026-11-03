#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Filter line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float out[4096], dry[4096];
static const double pi = 3.14159265358979323846;
static const sb_audio_voice_kind_t kinds[] = {
    SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_SAW, SB_AUDIO_VOICE_PULSE, SB_AUDIO_VOICE_NOISE
};
static bool start(sb_audio_bank_t *b, unsigned voice, unsigned kind) {
    return sb_audio_bank_waveform(b,voice,kinds[kind]) &&
        (kind==3 ? sb_audio_bank_start_selected_noise(b,voice) : sb_audio_bank_start_selected(b,voice,440));
}
int main(void) {
    sb_audio_filter_t f = {0}, saved;
    CHECK(!sb_audio_filter_cutoff(NULL,2000,false));
    CHECK(!sb_audio_filter_bypass(NULL,true,false));
    const double bad[] = {19.999,20000.001,0,-1,NAN,INFINITY,-INFINITY};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        saved = f;
        CHECK(!sb_audio_filter_cutoff(&f,bad[i],false));
        CHECK(memcmp(&f,&saved,sizeof f)==0);
    }
    sb_audio_filter_start(&f);
    CHECK(f.cutoff==2000 && f.wet==0 && f.state==0);
    CHECK(sb_audio_filter_sample(&f,.37)==.37 && f.state>0); /* Exact bypass, live history. */
    CHECK(sb_audio_filter_bypass(&f,false,true));
    CHECK(sb_audio_filter_cutoff(&f,20,true));
    double from=f.coefficient, target=f.coefficient_target;
    for (unsigned i=0;i<240;++i) (void)sb_audio_filter_sample(&f,.5);
    CHECK(f.wet==.5 && fabs(f.coefficient-(from+target)*.5)<1e-14);
    CHECK(sb_audio_filter_cutoff(&f,20,true) && f.coefficient_left==240);
    CHECK(sb_audio_filter_bypass(&f,false,true) && f.wet_left==240);
    for (unsigned i=0;i<239;++i) (void)sb_audio_filter_sample(&f,-.5);
    CHECK(f.wet_left==1 && f.coefficient_left==1 && f.wet<1);
    (void)sb_audio_filter_sample(&f,-.5);
    CHECK(f.wet==1 && f.coefficient==target && !f.wet_left && !f.coefficient_left);
    CHECK(sb_audio_filter_bypass(&f,true,true));
    for (unsigned i=0;i<120;++i) (void)sb_audio_filter_sample(&f,.2);
    double wet=f.wet;
    CHECK(sb_audio_filter_bypass(&f,false,true) && f.wet==wet && f.wet_left==480);
    CHECK(sb_audio_filter_cutoff(&f,20000,true));
    for (unsigned i=0;i<137;++i) (void)sb_audio_filter_sample(&f,-1);
    from=f.coefficient;
    CHECK(sb_audio_filter_cutoff(&f,200,true));
    CHECK(f.coefficient==from && f.coefficient_from==from && f.coefficient_left==480);
    sb_audio_filter_start(&f);
    CHECK(f.state==0 && f.wet==1 && f.coefficient==f.coefficient_target);
    CHECK(!f.wet_left && !f.coefficient_left);
    /* Settled sinusoidal measurements, independent of coefficient formula:
     * true -3 dB at each requested cutoff, including both extremes. */
    const double cutoffs[] = {20,200,2000,20000};
    for (unsigned c=0;c<4;++c) {
        CHECK(sb_audio_filter_cutoff(&f,cutoffs[c],false));
        sb_audio_filter_start(&f);
        double power=0;
        for (unsigned i=0;i<96000;++i) {
            double y=sb_audio_filter_sample(&f,sin(2*pi*cutoffs[c]*i/48000));
            CHECK(isfinite(y) && fabs(y)<=1);
            if (i>=48000) power+=y*y;
        }
        CHECK(fabs(sqrt(2*power/48000)-sqrt(.5))<1e-9);
    }
    /* Unity DC gain and monotone attenuation with increasing input frequency. */
    CHECK(sb_audio_filter_cutoff(&f,1000,false));
    for (unsigned i=0;i<48000;++i) (void)sb_audio_filter_sample(&f,1);
    CHECK(fabs(f.state-1)<1e-12);
    const double tones[] = {100,1000,10000};
    double previous=2;
    for (unsigned t=0;t<3;++t) {
        sb_audio_filter_start(&f);
        double power=0;
        for (unsigned i=0;i<96000;++i) {
            double y=sb_audio_filter_sample(&f,sin(2*pi*tones[t]*i/48000));
            if (i>=48000) power+=y*y;
        }
        CHECK(power<previous*48000);
        previous=power/48000;
    }
    /* Arbitrary bounded input, rapid retargets and both bypass directions cannot
     * overshoot. This includes cutoff changes while already bypassed. */
    for (unsigned i=0;i<48000;++i) {
        if (i%137==0) {
            CHECK(sb_audio_filter_cutoff(&f,i%2 ? 20 : 20000,true));
            CHECK(sb_audio_filter_bypass(&f,i%3==0,true));
        }
        double y=sb_audio_filter_sample(&f,i%7<3 ? -1 : 1);
        CHECK(isfinite(y) && fabs(y)<=1.000000000001);
    }
    CHECK(sb_audio_filter_bypass(&f,true,true));
    for (unsigned i=0;i<480;++i) (void)sb_audio_filter_sample(&f,.5);
    CHECK(f.wet==0 && sb_audio_filter_sample(&f,-.123)==-.123);
    sb_audio_bank_t b={0}, ref, snapshot;
    sb_envelope_settings_t flat={0,0,1,0};
    CHECK(!sb_audio_bank_filter_cutoff(NULL,1,2000));
    CHECK(!sb_audio_bank_filter_bypass(NULL,1,false));
    CHECK(!sb_audio_bank_filter_cutoff(&b,0,2000));
    CHECK(!sb_audio_bank_filter_bypass(&b,10,false));
    for (unsigned k=0;k<4;++k) {
        sb_audio_bank_reset(&b);
        CHECK(sb_audio_bank_configure(&b,1,&flat));
        CHECK(sb_audio_bank_volume(&b,1,1) && sb_audio_bank_pan(&b,1,-1));
        CHECK(sb_audio_bank_filter_cutoff(&b,1,500));
        CHECK(!sb_audio_bank_active(&b,1));
        CHECK(start(&b,1,k));
        ref=b; /* Identical dry source timeline. */
        CHECK(sb_audio_bank_filter_bypass(&b,1,false));
        CHECK(sb_audio_bank_render(&b,out,480));
        CHECK(sb_audio_bank_render(&ref,dry,480));
        CHECK(b.filters[0].wet==1);
        CHECK(sb_audio_bank_waveform(&b,1,kinds[(k+1)%4]));
        CHECK(sb_audio_bank_waveform(&ref,1,kinds[(k+1)%4]));
        double history=b.filters[0].state, a=b.filters[0].coefficient;
        CHECK(sb_audio_bank_render(&b,out,1024));
        CHECK(sb_audio_bank_render(&ref,dry,1024));
        for (unsigned i=0;i<1024;++i) {
            history+=a*(dry[2*i]*9-history);
            CHECK(fabs(out[2*i]-history/9)<2e-8 && out[2*i+1]==0);
        }
        /* Active selected retrigger preserves filter history; invalid controls
         * preserve the entire bank. Fresh starts clear only filter history. */
        saved=b.filters[0];
        CHECK((k+1)%4==3 ? sb_audio_bank_start_selected_noise(&b,1) : sb_audio_bank_start_selected(&b,1,880));
        CHECK(memcmp(&saved,&b.filters[0],sizeof saved)==0);
        snapshot=b;
        for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) CHECK(!sb_audio_bank_filter_cutoff(&b,1,bad[i]));
        CHECK(memcmp(&b,&snapshot,sizeof b)==0);
        CHECK(sb_audio_bank_filter_cutoff(&b,1,20));
        CHECK(memcmp(b.voices,snapshot.voices,sizeof b.voices)==0);
        CHECK(sb_audio_bank_filter_bypass(&b,1,true));
        CHECK(sb_audio_bank_note_off(&b,1)); /* Zero release ends before ramps. */
        CHECK(!sb_audio_bank_active(&b,1));
        CHECK(start(&b,1,k));
        CHECK(b.filters[0].state==0 && b.filters[0].wet==0 && b.filters[0].cutoff==20);
        CHECK(!b.filters[0].coefficient_left && !b.filters[0].wet_left);
    }
    /* Independent slots, silent/muted advancement, nine-voice bounds and
     * identical samples regardless of render partition. */
    sb_audio_bank_reset(&b);
    for (unsigned v=1;v<=9;++v) {
        CHECK(sb_audio_bank_configure(&b,v,&flat));
        CHECK(sb_audio_bank_volume(&b,v,v==1 ? 0 : 1));
        CHECK(sb_audio_bank_pan(&b,v,-1));
        CHECK(sb_audio_bank_filter_cutoff(&b,v,20+v*500));
        CHECK(sb_audio_bank_filter_bypass(&b,v,false));
        CHECK(start(&b,v,v%4));
        CHECK(sb_audio_bank_filter_cutoff(&b,v,20000));
    }
    snapshot=b;
    CHECK(sb_audio_bank_filter_bypass(&b,1,true));
    for (unsigned i=1;i<9;++i) CHECK(memcmp(&b.filters[i],&snapshot.filters[i],sizeof f)==0);
    ref=b;
    CHECK(sb_audio_bank_render(&b,out,2048));
    for (unsigned i=0;i<2048;i+=37) CHECK(sb_audio_bank_render(&ref,dry+2*i,2048-i<37?2048-i:37));
    CHECK(memcmp(out,dry,sizeof out)==0);
    CHECK(memcmp(b.filters,ref.filters,sizeof b.filters)==0);
    CHECK(b.filters[0].state!=0 && !b.filters[0].wet_left && !b.filters[0].coefficient_left);
    for (unsigned i=0;i<4096;++i) CHECK(isfinite(out[i]) && fabs(out[i])<=1);
    CHECK(sb_audio_bank_stop(&b,2));
    CHECK(sb_audio_bank_render(&b,out,480) && !sb_audio_bank_active(&b,2));
    CHECK(start(&b,2,2) && b.filters[1].state==0);
    sb_audio_bank_reset(&b);
    CHECK(sb_audio_bank_render(&b,out,2048));
    for (unsigned i=0;i<4096;++i) CHECK(out[i]==0);
    CHECK(start(&b,1,0) && b.filters[0].cutoff==2000 && b.filters[0].wet==0);
    puts("Native low-pass filter checks passed");
    return 0;
}
