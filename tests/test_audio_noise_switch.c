#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Noise switch line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float out[4096], expected[4096];
static const sb_audio_voice_kind_t kinds[] = {SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_SAW, SB_AUDIO_VOICE_PULSE};
static double phase(const sb_audio_bank_t *b, sb_audio_voice_kind_t k) {
    return k == SB_AUDIO_VOICE_TRIANGLE ? b->voices[0].phase : k == SB_AUDIO_VOICE_SAW ? b->saw[0].phase : b->pulse[0].phase;
}
static double frequency(const sb_audio_bank_t *b, sb_audio_voice_kind_t k) {
    return k == SB_AUDIO_VOICE_TRIANGLE ? b->voices[0].frequency : k == SB_AUDIO_VOICE_SAW ? b->saw[0].frequency : b->pulse[0].frequency;
}
static double pitched_sample(sb_audio_bank_t *b, sb_audio_voice_kind_t k) {
    float sample = 0;
    if (k == SB_AUDIO_VOICE_SAW) (void)sb_audio_saw_render(&b->saw[0], &sample, 1);
    else if (k == SB_AUDIO_VOICE_PULSE) (void)sb_audio_pulse_render_conditioned(&b->pulse[0], &sample, 1);
    else {
        double value = 1-4*fabs(b->voices[0].phase-.5);
        b->voices[0].phase += b->voices[0].frequency/48000;
        if (b->voices[0].phase >= 1) b->voices[0].phase -= 1;
        return value;
    }
    return sample;
}
int main(void) {
    sb_audio_bank_t b = {0}, ref, snapshot;
    sb_envelope_settings_t flat = {0,0,1,0};
    CHECK(!sb_audio_bank_start_selected_noise(NULL,1));
    CHECK(!sb_audio_bank_start_selected_noise(&b,0));
    CHECK(!sb_audio_bank_start_selected_noise(&b,10));
    CHECK(!sb_audio_bank_start_selected_noise(&b,1));
    CHECK(!sb_audio_bank_pitch(&b,1,440)); /* Idle controls unchanged. */
    for (unsigned k=0;k<3;++k) {
        sb_audio_bank_reset(&b);
        CHECK(sb_audio_bank_configure(&b,1,&flat));
        CHECK(sb_audio_bank_volume(&b,1,1));
        CHECK(sb_audio_bank_pan(&b,1,-1));
        CHECK(sb_audio_bank_seed(&b,1,12345));
        CHECK(sb_audio_bank_pulse_width(&b,1,.25));
        CHECK(sb_audio_bank_waveform(&b,1,kinds[k]));
        CHECK(sb_audio_bank_start_selected(&b,1,330));
        CHECK(sb_audio_bank_render(&b,out,137));
        snapshot = b;
        CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
        CHECK(b.noise[0].state == 12345 && b.retained_pitch[0] == 330);
        CHECK(memcmp(&b.voices[0].envelope,&snapshot.voices[0].envelope,sizeof(sb_envelope_t)) == 0);
        ref = b;
        for (unsigned i=0;i<480;++i) {
            snapshot = b;
            CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
            CHECK(!sb_audio_bank_waveform(&b,1,kinds[k]));
            CHECK(!sb_audio_bank_start_selected(&b,1,440));
            CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
            float noise;
            CHECK(sb_audio_noise_render(&ref.noise[0],&noise,1));
            double pitched = pitched_sample(&ref,kinds[k]);
            float mixed = (float)((1-i/480.0)*pitched+(i/480.0)*noise);
            CHECK(sb_audio_bank_render(&b,out,1));
            CHECK(fabs(out[0]-mixed/9.0) < 2e-8 && out[1] == 0);
            CHECK(b.switch_left[0] == 479-i && b.noise[0].state == ref.noise[0].state);
        }
        double frozen = phase(&b,kinds[k]);
        /* Seed and pitch edits must not change noise samples already playing. */
        ref = b;
        CHECK(sb_audio_bank_seed(&b,1,54321));
        CHECK(sb_audio_bank_pitch(&b,1,880));
        CHECK(sb_audio_bank_render(&b,out,1024));
        CHECK(sb_audio_bank_render(&ref,expected,1024));
        CHECK(memcmp(out,expected,2048*sizeof(float)) == 0);
        CHECK(phase(&b,kinds[k]) == frozen);
        CHECK(b.noise[0].state == ref.noise[0].state);
        CHECK(sb_audio_bank_waveform(&b,1,kinds[k]));
        CHECK(phase(&b,kinds[k]) == (k==0 ? .25 : k==1 ? .5 : 0));
        CHECK(frequency(&b,kinds[k]) == 880);
        if (k==2) CHECK(b.pulse[0].width == .25 && b.pulse[0].width_left == 0);
        CHECK(!sb_audio_bank_start_selected_noise(&b,1));
        ref = b;
        for (unsigned i=0;i<480;++i) {
            float noise;
            CHECK(sb_audio_noise_render(&ref.noise[0],&noise,1));
            double pitched = pitched_sample(&ref,kinds[k]);
            float mixed = (float)((1-i/480.0)*noise+(i/480.0)*pitched);
            CHECK(sb_audio_bank_render(&b,out,1));
            CHECK(fabs(out[0]-mixed/9.0) < 2e-8);
        }
        uint32_t frozen_noise = b.noise[0].state;
        CHECK(sb_audio_bank_render(&b,out,100));
        CHECK(b.noise[0].state == frozen_noise);
        CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
        CHECK(b.noise[0].state == 54321); /* New sequence on return. */
        CHECK(sb_audio_bank_seed(&b,1,777));
        CHECK(sb_audio_bank_render(&b,out,120));
        uint32_t current = b.noise[0].state;
        double level = b.voices[0].envelope.level;
        CHECK(sb_audio_bank_start_selected_noise(&b,1));
        CHECK(b.noise[0].state == current && b.switch_left[0] == 360);
        CHECK(b.voices[0].envelope.level == level);
        CHECK(sb_audio_bank_pitch(&b,1,110)); /* Updates outgoing pitched source. */
        CHECK(frequency(&b,kinds[k]) == 110 && b.retained_pitch[0] == 110);
        snapshot = b;
        CHECK(!sb_audio_bank_pitch(&b,1,NAN));
        CHECK(!sb_audio_bank_pitch(&b,1,109));
        CHECK(!sb_audio_bank_pitch(&b,1,881));
        CHECK(!sb_audio_bank_waveform(&b,1,(sb_audio_voice_kind_t)99));
        CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
        CHECK(sb_audio_bank_render(&b,out,360));
        snapshot = b;
        CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
        CHECK(memcmp(&b,&snapshot,sizeof b) == 0);
        CHECK(sb_audio_bank_start_selected_noise(&b,1));
        CHECK(b.noise[0].state == snapshot.noise[0].state);
        CHECK(sb_audio_bank_waveform(&b,1,kinds[k]));
        CHECK(frequency(&b,kinds[k]) == 110);
        current = b.noise[0].state;
        CHECK(sb_audio_bank_start_selected(&b,1,550));
        CHECK(b.noise[0].state == current && b.switch_left[0] == 480);
        CHECK(frequency(&b,kinds[k]) == 550 && b.retained_pitch[0] == 550);
        CHECK(sb_audio_bank_note_off(&b,1));
        CHECK(!b.voices[0].active && !b.switch_left[0] && b.kind[0] == kinds[k]);
    }
    /* Default 440 Hz, distinct default seeds, reset, and silence on selection. */
    sb_audio_bank_reset(&b);
    ref = b;
    for (unsigned v=1;v<=9;++v) {
        CHECK(sb_audio_bank_waveform(&b,v,SB_AUDIO_VOICE_NOISE));
        CHECK(!b.voices[v-1].active && b.noise[v-1].state == 0);
        CHECK(sb_audio_bank_start_selected_noise(&b,v));
        CHECK(sb_audio_bank_noise_on(&ref,v));
        CHECK(b.noise[v-1].state == ref.noise[v-1].state);
        for (unsigned previous=0;previous<v-1;++previous) CHECK(b.noise[v-1].state != b.noise[previous].state);
    }
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_SAW));
    CHECK(b.saw[0].frequency == 440 && b.saw[0].phase == .5);
    CHECK(sb_audio_bank_seed(&b,1,0));
    CHECK(sb_audio_bank_render(&b,out,480));
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
    CHECK(b.noise[0].state == ref.noise[0].state);
    /* Muted progression; short release and stop do not leave stale transitions. */
    sb_audio_bank_reset(&b);
    sb_envelope_settings_t release = {0,0,1,.002};
    CHECK(sb_audio_bank_configure(&b,1,&release));
    CHECK(sb_audio_bank_volume(&b,1,0));
    CHECK(sb_audio_bank_start_selected(&b,1,440));
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
    CHECK(sb_audio_bank_note_off(&b,1));
    CHECK(sb_audio_bank_render(&b,out,96));
    for (unsigned i=0;i<192;++i) CHECK(out[i] == 0);
    CHECK(!b.voices[0].active && !b.switch_left[0] && b.kind[0] == SB_AUDIO_VOICE_NOISE);
    CHECK(sb_audio_bank_start_selected_noise(&b,1));
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_PULSE));
    CHECK(sb_audio_bank_stop(&b,1));
    CHECK(sb_audio_bank_render(&b,out,480));
    CHECK(!b.voices[0].active && !b.switch_left[0] && b.kind[0] == SB_AUDIO_VOICE_PULSE);
    /* Nine simultaneous noisy transitions retain bounds and chunk independence. */
    sb_audio_bank_reset(&b);
    for (unsigned v=1;v<=9;++v) {
        CHECK(sb_audio_bank_configure(&b,v,&flat));
        CHECK(sb_audio_bank_volume(&b,v,1));
        CHECK(sb_audio_bank_pan(&b,v,(v-5.0)/4));
        CHECK(sb_audio_bank_waveform(&b,v,kinds[v%3]));
        CHECK(sb_audio_bank_start_selected(&b,v,220+40*v));
        CHECK(sb_audio_bank_waveform(&b,v,SB_AUDIO_VOICE_NOISE));
    }
    ref = b;
    CHECK(sb_audio_bank_render(&b,out,2048));
    for (unsigned i=0;i<2048;i+=37) CHECK(sb_audio_bank_render(&ref,expected+2*i,2048-i<37?2048-i:37));
    CHECK(memcmp(out,expected,sizeof out) == 0);
    for (unsigned i=0;i<4096;++i) CHECK(isfinite(out[i]) && fabs(out[i]) <= 1);
    for (unsigned v=1;v<=9;++v) CHECK(sb_audio_bank_waveform(&b,v,kinds[v%3]));
    sb_audio_bank_reset(&b);
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_NOISE));
    CHECK(sb_audio_bank_start_selected_noise(&b,1));
    CHECK(sb_audio_bank_waveform(&b,1,SB_AUDIO_VOICE_TRIANGLE));
    CHECK(b.voices[0].frequency == 440 && b.voices[0].phase == .25 && b.retained_pitch[0] == 0);
    puts("Noise transition checks passed");
    return 0;
}
