/* Exercise the real backend controls and callback with a paused dummy device.
 * Including the backend lets us compare its output to the pure bank contract. */
#include "../src/sb_audio_sdl.c"
#include <math.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Bank SDL line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float output[2048], expected[2048];
int main(void) {
    CHECK(!sb_audio_voice_waveform(NULL, 1, SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_voice_start_selected(NULL, 1, 440));
    CHECK(!sb_audio_voice_start_selected_noise(NULL, 1));
    CHECK(!sb_audio_voice_note_on(NULL, 1, 440));
    CHECK(!sb_audio_voice_noise_on(NULL, 1));
    CHECK(!sb_audio_voice_saw_on(NULL, 1, 440));
    CHECK(!sb_audio_voice_pulse_on(NULL, 1, 440));
    CHECK(!sb_audio_voice_pulse_width(NULL, 1, .5));
    CHECK(!sb_audio_voice_seed(NULL, 1, 42));
    CHECK(!sb_audio_voice_configure(NULL, 1, 0, 0, 1, 0));
    CHECK(!sb_audio_voice_pitch(NULL, 1, 440));
    CHECK(!sb_audio_voice_volume(NULL, 1, 1));
    CHECK(!sb_audio_voice_pan(NULL, 1, 0));
    CHECK(!sb_audio_voice_note_off(NULL, 1));
    CHECK(!sb_audio_voice_stop(NULL, 1));
    CHECK(!sb_audio_voice_active(NULL, 1));
    sb_audio_t *audio = sb_audio_create();
    CHECK(audio);
    SDL_PauseAudioDevice(audio->device, 1);
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i) CHECK(output[i] == 0);
    CHECK(sb_audio_start_test_tone(audio));
    CHECK(!sb_audio_voice_start_selected(audio, 1, 440));
    CHECK(sb_audio_voice_waveform(audio, 1, SB_AUDIO_VOICE_NOISE));
    CHECK(!sb_audio_voice_start_selected_noise(audio, 1));
    CHECK(!sb_audio_voice_note_on(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 1));
    CHECK(!sb_audio_voice_saw_on(audio, 1, 440));
    CHECK(!sb_audio_voice_pulse_on(audio, 1, 440));
    audio->tone.active = false;
    CHECK(sb_audio_start_triangle(audio, 440));
    CHECK(!sb_audio_voice_start_selected(audio, 1, 440));
    CHECK(sb_audio_voice_waveform(audio, 1, SB_AUDIO_VOICE_NOISE));
    CHECK(!sb_audio_voice_start_selected_noise(audio, 1));
    CHECK(!sb_audio_voice_note_on(audio, 9, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 9));
    CHECK(!sb_audio_voice_saw_on(audio, 9, 440));
    CHECK(!sb_audio_voice_pulse_on(audio, 9, 440));
    audio->triangle = (sb_audio_triangle_t){0};
    const unsigned counts[] = {1, 3, 9};
    for (unsigned group = 0; group < 3; ++group) {
        sb_audio_bank_reset(&audio->bank);
        unsigned count = counts[group];
        for (unsigned v = 1; v <= count; ++v) {
            CHECK(sb_audio_voice_configure(audio, v, 0, 0, 1, .01));
            CHECK(sb_audio_voice_volume(audio, v, 1));
            CHECK(sb_audio_voice_pan(audio, v, -1));
            CHECK(sb_audio_voice_note_on(audio, v, 440));
            CHECK(sb_audio_voice_active(audio, v));
        }
        sb_audio_bank_t reference = audio->bank;
        CHECK(sb_audio_bank_render(&reference, expected, 1024));
        audio_output(audio, (Uint8 *)output, sizeof(output));
        float peak = 0;
        for (unsigned i = 0; i < 2048; ++i) {
            CHECK(isfinite(output[i]));
            CHECK(fabsf(output[i] - expected[i]*0.790569415f) < 1e-8f);
            CHECK(fabsf(output[i]) <= .790570f);
            if (fabsf(output[i]) > peak) peak = fabsf(output[i]);
        }
        CHECK(peak > .78f*count/9); /* Fixed headroom, no active-count scaling. */
        CHECK(!sb_audio_start_test_tone(audio));
        CHECK(!sb_audio_start_triangle(audio, 440));
    }
    unsigned char snapshot[sizeof(audio->bank)];
    memcpy(snapshot, &audio->bank, sizeof(snapshot));
    CHECK(!sb_audio_voice_note_on(audio, 0, 440));
    CHECK(!sb_audio_voice_note_on(audio, 10, 440));
    CHECK(!sb_audio_voice_configure(audio, 1, -1, 0, 1, 0));
    CHECK(!sb_audio_voice_pitch(audio, 1, NAN));
    CHECK(!sb_audio_voice_volume(audio, 1, 2));
    CHECK(!sb_audio_voice_pan(audio, 9, INFINITY));
    CHECK(!sb_audio_voice_note_off(audio, 10));
    CHECK(!sb_audio_voice_stop(audio, 0));
    CHECK(memcmp(snapshot, &audio->bank, sizeof(snapshot)) == 0);
    CHECK(sb_audio_voice_pitch(audio, 1, 660));
    CHECK(sb_audio_voice_volume(audio, 1, .5));
    CHECK(sb_audio_voice_pan(audio, 1, 1));
    CHECK(sb_audio_voice_note_on(audio, 1, 330));
    CHECK(sb_audio_voice_note_off(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1));
    for (unsigned v = 2; v <= 9; ++v) {
        CHECK(sb_audio_voice_active(audio, v));
        CHECK(sb_audio_voice_stop(audio, v));
    }
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!bank_active(audio));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i) CHECK(output[i] == 0);
    CHECK(sb_audio_start_test_tone(audio));
    audio->tone.active = false;
    sb_audio_bank_reset(&audio->bank);
    CHECK(sb_audio_voice_seed(audio, 1, 123456789));
    CHECK(sb_audio_voice_configure(audio, 1, 0, 0, 1, .01));
    CHECK(sb_audio_voice_noise_on(audio, 1));
    CHECK(sb_audio_voice_note_on(audio, 2, 440));
    sb_audio_bank_t reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference, expected, 1024));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(!sb_audio_start_test_tone(audio) && !sb_audio_start_triangle(audio, 440));
    CHECK(!sb_audio_voice_note_on(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 2));
    CHECK(sb_audio_voice_pitch(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 10));
    CHECK(!sb_audio_voice_seed(audio, 0, 42));
    uint32_t current = audio->bank.noise[0].state;
    CHECK(sb_audio_voice_seed(audio, 1, 42));
    CHECK(sb_audio_voice_noise_on(audio, 1));
    CHECK(audio->bank.noise[0].state == current);
    CHECK(sb_audio_voice_note_off(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1) && sb_audio_voice_active(audio, 2));
    CHECK(sb_audio_voice_noise_on(audio, 1));
    CHECK(audio->bank.noise[0].state == 42);
    sb_audio_bank_reset(&audio->bank);
    CHECK(sb_audio_voice_configure(audio, 1, .02, 0, .7, .02));
    CHECK(sb_audio_voice_saw_on(audio, 1, 220));
    CHECK(sb_audio_voice_noise_on(audio, 2));
    CHECK(sb_audio_voice_note_on(audio, 3, 440));
    reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference, expected, 1024));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(!sb_audio_voice_note_on(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 1));
    CHECK(!sb_audio_voice_saw_on(audio, 2, 440));
    CHECK(!sb_audio_voice_saw_on(audio, 3, 440));
    CHECK(!sb_audio_voice_saw_on(audio, 10, 440));
    CHECK(!sb_audio_start_test_tone(audio) && !sb_audio_start_triangle(audio, 440));
    double phase = audio->bank.saw[0].phase;
    CHECK(sb_audio_voice_pitch(audio, 1, 440));
    CHECK(audio->bank.saw[0].phase == phase);
    CHECK(sb_audio_voice_saw_on(audio, 1, 660));
    CHECK(audio->bank.saw[0].phase == phase && audio->bank.saw[0].frequency == 660);
    CHECK(!sb_audio_voice_saw_on(audio, 1, NAN));
    CHECK(audio->bank.saw[0].phase == phase && audio->bank.saw[0].frequency == 660);
    CHECK(sb_audio_voice_note_off(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1));
    CHECK(sb_audio_voice_active(audio, 2) && sb_audio_voice_active(audio, 3));
    CHECK(sb_audio_voice_saw_on(audio, 1, 330));
    CHECK(audio->bank.saw[0].phase == .5);
    CHECK(sb_audio_voice_stop(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1));
    sb_audio_bank_reset(&audio->bank);
    CHECK(sb_audio_voice_configure(audio, 1, 0, 0, 1, .01));
    CHECK(sb_audio_voice_pulse_width(audio, 1, .25));
    CHECK(sb_audio_voice_pulse_on(audio, 1, 440));
    CHECK(sb_audio_voice_pulse_width(audio, 1, .95));
    reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference, expected, 1024));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(audio->bank.pulse[0].width == .95);
    phase = audio->bank.pulse[0].phase;
    CHECK(sb_audio_voice_pitch(audio, 1, 660));
    CHECK(sb_audio_voice_pulse_on(audio, 1, 330));
    CHECK(audio->bank.pulse[0].phase == phase);
    memcpy(snapshot, &audio->bank, sizeof(snapshot));
    CHECK(!sb_audio_voice_pulse_width(audio, 1, NAN));
    CHECK(!sb_audio_voice_pulse_on(audio, 1, 881));
    CHECK(!sb_audio_voice_pulse_on(audio, 10, 440));
    CHECK(!sb_audio_voice_saw_on(audio, 1, 440));
    CHECK(!sb_audio_start_test_tone(audio) && !sb_audio_start_triangle(audio, 440));
    CHECK(memcmp(snapshot, &audio->bank, sizeof(snapshot)) == 0);
    CHECK(sb_audio_voice_note_off(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1));
    CHECK(sb_audio_voice_pulse_on(audio, 1, 440));
    CHECK(audio->bank.pulse[0].phase == 0 && audio->bank.pulse[0].width == .95);
    CHECK(sb_audio_voice_stop(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1));
    sb_audio_bank_reset(&audio->bank);
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_voice_active(audio,1));
    CHECK(sb_audio_voice_start_selected(audio,1,330));
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_PULSE));
    memcpy(snapshot,&audio->bank,sizeof(snapshot));
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_PULSE));
    CHECK(!sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_TRIANGLE));
    CHECK(!sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_NOISE));
    CHECK(!sb_audio_voice_waveform(audio,0,SB_AUDIO_VOICE_SAW));
    CHECK(!sb_audio_voice_start_selected(audio,1,NAN));
    CHECK(memcmp(snapshot,&audio->bank,sizeof(snapshot)) == 0);
    CHECK(sb_audio_voice_start_selected(audio,1,440));
    CHECK(audio->bank.switch_left[0] == 480);
    CHECK(sb_audio_voice_pulse_width(audio,1,.25));
    reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference,expected,1024));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    for (unsigned i=0;i<2048;++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(!audio->bank.switch_left[0] && audio->bank.kind[0] == SB_AUDIO_VOICE_PULSE);
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_TRIANGLE));
    CHECK(sb_audio_voice_stop(audio,1));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    CHECK(!sb_audio_voice_active(audio,1) && !audio->bank.switch_left[0]);
    sb_audio_bank_reset(&audio->bank);
    CHECK(sb_audio_voice_configure(audio,1,0,0,1,.01));
    CHECK(sb_audio_voice_note_on(audio,1,330));
    CHECK(sb_audio_voice_seed(audio,1,12345));
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_NOISE));
    CHECK(audio->bank.noise[0].state == 12345);
    CHECK(sb_audio_voice_seed(audio,1,67890));
    CHECK(sb_audio_voice_start_selected_noise(audio,1));
    CHECK(audio->bank.noise[0].state == 12345 && audio->bank.switch_left[0] == 480);
    memcpy(snapshot,&audio->bank,sizeof(snapshot));
    CHECK(!sb_audio_voice_start_selected(audio,1,440));
    CHECK(!sb_audio_voice_start_selected_noise(audio,0));
    CHECK(!sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_SAW));
    CHECK(memcmp(snapshot,&audio->bank,sizeof(snapshot)) == 0);
    reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference,expected,1024));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    for (unsigned i=0;i<2048;++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    reference = audio->bank;
    CHECK(sb_audio_voice_pitch(audio,1,550));
    CHECK(audio->bank.retained_pitch[0] == 550);
    CHECK(sb_audio_bank_render(&reference,expected,1024));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    for (unsigned i=0;i<2048;++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_PULSE));
    CHECK(audio->bank.pulse[0].phase == 0 && audio->bank.pulse[0].frequency == 550);
    CHECK(!sb_audio_voice_start_selected_noise(audio,1));
    CHECK(sb_audio_voice_start_selected(audio,1,660));
    CHECK(audio->bank.switch_left[0] == 480 && audio->bank.pulse[0].frequency == 660);
    reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference,expected,1024));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    for (unsigned i=0;i<2048;++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(sb_audio_voice_waveform(audio,1,SB_AUDIO_VOICE_NOISE));
    CHECK(audio->bank.noise[0].state == 67890);
    CHECK(sb_audio_voice_stop(audio,1));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    CHECK(!sb_audio_voice_active(audio,1) && !audio->bank.switch_left[0]);
    sb_audio_bank_reset(&audio->bank);
    /* Expanded range through every public pitched start, pitch control, and
     * selected retrigger during a fade; failures must leave all state intact. */
    const double limits[] = {110, 880};
    const double bad_pitch[] = {109.999, 880.001, NAN, INFINITY, -INFINITY};
    for (unsigned k=0;k<3;++k) for (unsigned f=0;f<2;++f) {
        sb_audio_bank_reset(&audio->bank);
        CHECK(sb_audio_voice_configure(audio,1,0,0,1,0));
        CHECK(k==0 ? sb_audio_voice_note_on(audio,1,limits[f]) :
              k==1 ? sb_audio_voice_saw_on(audio,1,limits[f]) :
                     sb_audio_voice_pulse_on(audio,1,limits[f]));
        CHECK(sb_audio_voice_pitch(audio,1,limits[1-f]));
        CHECK(audio->bank.retained_pitch[0] == limits[1-f]);
        memcpy(snapshot,&audio->bank,sizeof(snapshot));
        for (unsigned i=0;i<sizeof(bad_pitch)/sizeof(bad_pitch[0]);++i) {
            CHECK(!sb_audio_voice_pitch(audio,1,bad_pitch[i]));
            CHECK(!(k==0 ? sb_audio_voice_note_on(audio,1,bad_pitch[i]) :
                    k==1 ? sb_audio_voice_saw_on(audio,1,bad_pitch[i]) :
                           sb_audio_voice_pulse_on(audio,1,bad_pitch[i])));
            CHECK(memcmp(snapshot,&audio->bank,sizeof(snapshot)) == 0);
        }
        CHECK(sb_audio_voice_waveform(audio,1,k==0 ? SB_AUDIO_VOICE_SAW : SB_AUDIO_VOICE_TRIANGLE));
        CHECK(sb_audio_voice_start_selected(audio,1,limits[f]));
        CHECK(audio->bank.switch_left[0] == 480);
        CHECK(audio->bank.retained_pitch[0] == limits[f]);
        reference = audio->bank;
        CHECK(sb_audio_bank_render(&reference,expected,1024));
        audio_output(audio,(Uint8 *)output,sizeof(output));
        for (unsigned i=0;i<2048;++i)
            CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    }
    sb_audio_bank_reset(&audio->bank);
    /* Filter controls use the same locked bank state as the callback. */
    CHECK(!sb_audio_voice_filter_cutoff(NULL,1,2000));
    CHECK(!sb_audio_voice_filter_bypass(NULL,1,false));
    CHECK(sb_audio_voice_filter_cutoff(audio,1,500));
    CHECK(sb_audio_voice_filter_bypass(audio,1,false));
    CHECK(!sb_audio_voice_active(audio,1));
    CHECK(sb_audio_voice_configure(audio,1,0,0,1,.01));
    CHECK(sb_audio_voice_saw_on(audio,1,440));
    CHECK(sb_audio_voice_filter_cutoff(audio,1,20));
    CHECK(sb_audio_voice_filter_bypass(audio,1,true));
    memcpy(snapshot,&audio->bank,sizeof(snapshot));
    CHECK(!sb_audio_voice_filter_cutoff(audio,0,2000));
    CHECK(!sb_audio_voice_filter_cutoff(audio,1,NAN));
    CHECK(!sb_audio_voice_filter_cutoff(audio,1,20001));
    CHECK(!sb_audio_voice_filter_bypass(audio,10,false));
    CHECK(memcmp(snapshot,&audio->bank,sizeof(snapshot))==0);
    reference=audio->bank;
    CHECK(sb_audio_bank_render(&reference,expected,1024));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    for (unsigned i=0;i<2048;++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f)<1e-8f);
    CHECK(audio->bank.filters[0].wet==0 && !audio->bank.filters[0].coefficient_left);
    CHECK(sb_audio_voice_filter_bypass(audio,1,false));
    CHECK(sb_audio_voice_note_off(audio,1));
    audio_output(audio,(Uint8 *)output,sizeof(output));
    CHECK(!sb_audio_voice_active(audio,1));
    CHECK(sb_audio_voice_noise_on(audio,1));
    CHECK(audio->bank.filters[0].state==0 && audio->bank.filters[0].wet==1);
    sb_audio_bank_reset(&audio->bank);
    /* Destroy while nine mixed voices are active; no release wait needed. */
    for (unsigned v = 1; v <= 9; ++v) {
        if (v%4 == 0) CHECK(sb_audio_voice_pulse_on(audio, v, 440));
        else if (v%3 == 0) CHECK(sb_audio_voice_saw_on(audio, v, 440));
        else if (v%3 == 1) CHECK(sb_audio_voice_noise_on(audio, v));
        else CHECK(sb_audio_voice_note_on(audio, v, 440));
    }
    CHECK(sb_audio_voice_waveform(audio, 3, SB_AUDIO_VOICE_NOISE));
    CHECK(sb_audio_voice_waveform(audio, 1, SB_AUDIO_VOICE_PULSE));
    SDL_PauseAudioDevice(audio->device, 0);
    sb_audio_destroy(audio);
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    audio = sb_audio_create();
    CHECK(audio);
    SDL_PauseAudioDevice(audio->device, 1);
    CHECK(!bank_active(audio));
    sb_audio_destroy(audio);
    puts("SDL bank checks passed");
    return 0;
}
