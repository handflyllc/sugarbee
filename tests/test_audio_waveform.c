#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Waveform line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float out[4096], split[4096];
static const sb_audio_voice_kind_t kinds[] = {SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_SAW, SB_AUDIO_VOICE_PULSE};
static double phase(const sb_audio_bank_t *b, unsigned s, sb_audio_voice_kind_t k) {
    return k == SB_AUDIO_VOICE_TRIANGLE ? b->voices[s].phase : k == SB_AUDIO_VOICE_SAW ? b->saw[s].phase : b->pulse[s].phase;
}
static double pitch(const sb_audio_bank_t *b, unsigned s, sb_audio_voice_kind_t k) {
    return k == SB_AUDIO_VOICE_TRIANGLE ? b->voices[s].frequency : k == SB_AUDIO_VOICE_SAW ? b->saw[s].frequency : b->pulse[s].frequency;
}
/* Independent source samples; envelope and mixing expectations are checked below. */
static double sample(sb_audio_bank_t *b, sb_audio_voice_kind_t k) {
    float x = 0;
    if (k == SB_AUDIO_VOICE_SAW) (void)sb_audio_saw_render(&b->saw[0], &x, 1);
    else if (k == SB_AUDIO_VOICE_PULSE) (void)sb_audio_pulse_render_conditioned(&b->pulse[0], &x, 1);
    else {
        double v = 1-4*fabs(b->voices[0].phase-.5);
        b->voices[0].phase += b->voices[0].frequency/48000;
        if (b->voices[0].phase >= 1) b->voices[0].phase -= 1;
        return v;
    }
    return x;
}
int main(void) {
    sb_audio_bank_t b = {0}, ref, snapshot;
    sb_envelope_settings_t flat = {0,0,1,0};
    CHECK(!sb_audio_bank_waveform(NULL, 1, SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_bank_waveform(&b, 0, SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_bank_waveform(&b, 10, SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_bank_waveform(&b, 1, (sb_audio_voice_kind_t)99));
    CHECK(sb_audio_bank_waveform(&b, 1, SB_AUDIO_VOICE_NOISE));
    CHECK(!sb_audio_bank_start_selected(NULL, 1, 440));
    /* All six directions and both pitch extremes; idle selection stays silent.
     * Verify first/middle/last blend samples, exact deadline and continued phase. */
    for (unsigned from = 0; from < 3; ++from) for (unsigned to = 0; to < 3; ++to) {
        if (from == to) continue;
        for (unsigned f = 0; f < 2; ++f) {
            sb_audio_bank_reset(&b);
            CHECK(sb_audio_bank_waveform(&b, 1, kinds[from]));
            CHECK(!sb_audio_bank_active(&b, 1));
            CHECK(sb_audio_bank_render(&b, out, 1) && out[0] == 0 && out[1] == 0);
            CHECK(sb_audio_bank_configure(&b, 1, &flat));
            CHECK(sb_audio_bank_volume(&b, 1, 1));
            CHECK(sb_audio_bank_pan(&b, 1, -1));
            CHECK(sb_audio_bank_pulse_width(&b, 1, f ? .05 : .95));
            CHECK(sb_audio_bank_start_selected(&b, 1, f ? 880 : 110));
            CHECK(phase(&b, 0, kinds[from]) == (from == 0 ? .25 : from == 1 ? .5 : 0));
            CHECK(sb_audio_bank_render(&b, out, 137));
            snapshot = b;
            CHECK(sb_audio_bank_waveform(&b, 1, kinds[from]));
            CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
            double old_phase = phase(&b,0,kinds[from]);
            CHECK(sb_audio_bank_waveform(&b, 1, kinds[to]));
            CHECK(phase(&b,0,kinds[to]) == old_phase);
            CHECK(memcmp(&b.voices[0].envelope,&snapshot.voices[0].envelope,sizeof(sb_envelope_t)) == 0);
            ref = b;
            for (unsigned i = 0; i < 481; ++i) {
                if (i < 480) {
                    snapshot = b;
                    CHECK(sb_audio_bank_waveform(&b, 1, kinds[to]));
                    CHECK(!sb_audio_bank_waveform(&b, 1, kinds[from]));
                    CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
                }
                double a = sample(&ref, kinds[from]), z = sample(&ref, kinds[to]);
                double mix = i < 480 ? i/480.0 : 1;
                float mono = (float)((1-mix)*a+mix*z);
                CHECK(sb_audio_bank_render(&b,out,1));
                CHECK(fabs(out[0]-mono/9.0) < 2e-8 && out[1] == 0);
                CHECK(b.switch_left[0] == (i < 480 ? 479-i : 0));
                CHECK(phase(&b,0,kinds[to]) == phase(&ref,0,kinds[to]));
            }
            CHECK(b.kind[0] == kinds[to]);
        }
    }
    /* A fade advances ADSR and volume/pan once, matching a nonswitching voice. */
    sb_audio_bank_reset(&b);
    sb_envelope_settings_t adsr = {.02,.03,.7,.004};
    CHECK(sb_audio_bank_configure(&b,1,&adsr));
    CHECK(sb_audio_bank_start_selected(&b,1,440));
    CHECK(sb_audio_bank_volume(&b,1,.2));
    CHECK(sb_audio_bank_pan(&b,1,1));
    ref = b;
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_PULSE));
    CHECK(sb_audio_bank_render(&b,out,100));
    CHECK(sb_audio_bank_render(&ref,split,100));
    CHECK(memcmp(&b.voices[0].envelope,&ref.voices[0].envelope,sizeof(sb_envelope_t)) == 0);
    CHECK(b.voices[0].volume == ref.voices[0].volume && b.voices[0].pan == ref.voices[0].pan);
    CHECK(sb_audio_bank_pitch(&b,1,660));
    CHECK(pitch(&b,0,SB_AUDIO_VOICE_TRIANGLE) == 660 && pitch(&b,0,SB_AUDIO_VOICE_PULSE) == 660);
    snapshot = b;
    CHECK(!sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_bank_start_selected(&b,1,NAN));
    CHECK(!sb_audio_bank_pitch(&b,1,INFINITY));
    CHECK(!sb_audio_bank_note_on(&b,1,440));
    CHECK(!sb_audio_bank_pulse_on(&b,1,440));
    CHECK(!sb_audio_bank_saw_on(&b,1,440));
    CHECK(!sb_audio_bank_noise_on(&b,1));
    CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
    double level = b.voices[0].envelope.level;
    CHECK(sb_audio_bank_start_selected(&b,1,330));
    CHECK(b.switch_left[0] == 380 && b.voices[0].envelope.level == level);
    CHECK(phase(&b,0,SB_AUDIO_VOICE_PULSE) == phase(&snapshot,0,SB_AUDIO_VOICE_PULSE));
    CHECK(sb_audio_bank_pulse_width(&b,1,.05));
    CHECK(b.pulse[0].width_left == 480);
    CHECK(sb_audio_bank_note_off(&b,1));
    CHECK(sb_audio_bank_render(&b,out,192));
    CHECK(!b.voices[0].active && b.switch_left[0] == 0 && b.kind[0] == SB_AUDIO_VOICE_PULSE);
    CHECK(sb_audio_bank_start_selected(&b,1,440));
    CHECK(b.pulse[0].phase == 0 && b.pulse[0].width == .05);
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_SAW));
    CHECK(sb_audio_bank_pulse_width(&b,1,.95)); /* Outgoing pulse still audible. */
    CHECK(b.pulse[0].width_left == 480);
    CHECK(sb_audio_bank_stop(&b,1));
    CHECK(sb_audio_bank_render(&b,out,480));
    CHECK(!b.voices[0].active && !b.switch_left[0] && b.kind[0] == SB_AUDIO_VOICE_SAW);
    CHECK(sb_audio_bank_configure(&b,1,&flat));
    CHECK(sb_audio_bank_start_selected(&b,1,440));
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_TRIANGLE));
    CHECK(sb_audio_bank_note_off(&b,1)); /* Zero release cancels fade immediately. */
    CHECK(!b.switch_left[0] && b.kind[0] == SB_AUDIO_VOICE_TRIANGLE);
    CHECK(sb_audio_bank_noise_on(&b,1));
    snapshot = b;
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_SAW));
    CHECK(b.switch_left[0] == 480 && b.saw[0].phase == .5);
    /* Nine simultaneous transitions, partition independence and bounded mix. */
    sb_audio_bank_reset(&b);
    for (unsigned v=1; v<=9; ++v) {
        CHECK(sb_audio_bank_configure(&b,v,&flat));
        CHECK(sb_audio_bank_volume(&b,v,1));
        CHECK(sb_audio_bank_pan(&b,v,(v-5.0)/4));
        CHECK(sb_audio_bank_waveform(&b,v,kinds[v%3]));
        CHECK(sb_audio_bank_start_selected(&b,v,220+v*40));
        CHECK(sb_audio_bank_waveform(&b,v,kinds[(v+1)%3]));
    }
    ref = b;
    CHECK(sb_audio_bank_render(&b,out,2048));
    for (unsigned i=0; i<2048; i+=37) CHECK(sb_audio_bank_render(&ref,split+2*i,2048-i<37?2048-i:37));
    CHECK(memcmp(out,split,sizeof out) == 0);
    for (unsigned i=0; i<4096; ++i) CHECK(isfinite(out[i]) && fabs(out[i]) <= 1);
    /* Muted voices still finish switches; reset discards all transition state. */
    CHECK(sb_audio_bank_volume(&b,1,0));
    CHECK(sb_audio_bank_render(&b,out,480));
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_SAW));
    snapshot = b;
    CHECK(sb_audio_bank_render(&b,NULL,0));
    CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
    CHECK(sb_audio_bank_render(&b,out,480));
    CHECK(!b.switch_left[0] && b.kind[0] == SB_AUDIO_VOICE_SAW);
    for (unsigned v=2;v<=9;++v) CHECK(b.kind[v-1] == snapshot.kind[v-1]);
    sb_audio_bank_reset(&b);
    CHECK(sb_audio_bank_render(&b,out,2048));
    for (unsigned i=0;i<4096;++i) CHECK(out[i] == 0);
    puts("Waveform switching checks passed");
    return 0;
}
