#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Pitch range line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float output[16384];
static const sb_audio_voice_kind_t kinds[] = {
    SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_SAW, SB_AUDIO_VOICE_PULSE
};
int main(void) {
    const double good[] = {110, 110.125, 220, 440, 660, 879.75, 880};
    const double bad[] = {0, -1, 109.999, 880.001, NAN, INFINITY, -INFINITY};
    sb_envelope_settings_t flat = {0,0,1,0};
    sb_audio_bank_t bank = {0}, snapshot;
    for (unsigned k = 0; k < 3; ++k) {
        for (unsigned f = 0; f < sizeof(good)/sizeof(good[0]); ++f) {
            sb_audio_bank_reset(&bank);
            CHECK(sb_audio_bank_configure(&bank,1,&flat));
            CHECK(sb_audio_bank_waveform(&bank,1,kinds[k]));
            snapshot = bank;
            for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
                CHECK(!sb_audio_bank_start_selected(&bank,1,bad[i]));
                CHECK(memcmp(&bank,&snapshot,sizeof bank) == 0);
            }
            CHECK(sb_audio_bank_start_selected(&bank,1,good[f]));
            CHECK(bank.retained_pitch[0] == good[f]);
            CHECK(sb_audio_bank_render(&bank,output,137));
            CHECK(sb_audio_bank_waveform(&bank,1,kinds[(k+1)%3]));
            CHECK(sb_audio_bank_render(&bank,output,73));
            snapshot = bank;
            for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
                CHECK(!sb_audio_bank_start_selected(&bank,1,bad[i]));
                CHECK(!sb_audio_bank_pitch(&bank,1,bad[i]));
                CHECK(memcmp(&bank,&snapshot,sizeof bank) == 0);
            }
            CHECK(sb_audio_bank_start_selected(&bank,1,good[f]));
            CHECK(bank.switch_left[0] == 407);
            CHECK(bank.retained_pitch[0] == good[f]);
            CHECK(sb_audio_bank_render(&bank,output,1024));
            CHECK(!bank.switch_left[0]);
            for (unsigned i=0;i<2048;++i) CHECK(isfinite(output[i]) && fabs(output[i]) <= 1);
        }
    }
    /* Nine full-volume sources at the new endpoints, including moving pulse
     * width, must preserve fixed headroom through simultaneous switches. */
    sb_audio_bank_reset(&bank);
    for (unsigned v=1;v<=9;++v) {
        CHECK(sb_audio_bank_configure(&bank,v,&flat));
        CHECK(sb_audio_bank_volume(&bank,v,1));
        CHECK(sb_audio_bank_pan(&bank,v,-1));
        CHECK(sb_audio_bank_pulse_width(&bank,v,v%2 ? .95 : .05));
        CHECK(sb_audio_bank_pulse_on(&bank,v,v%2 ? 880 : 110));
        CHECK(sb_audio_bank_pulse_width(&bank,v,v%2 ? .05 : .95));
        CHECK(sb_audio_bank_waveform(&bank,v,kinds[v%2]));
    }
    CHECK(sb_audio_bank_render(&bank,output,8192));
    double peak = 0;
    for (unsigned i=0;i<16384;++i) {
        CHECK(isfinite(output[i]) && fabs(output[i]) <= 1);
        if (fabs(output[i]) > peak) peak = fabs(output[i]);
    }
    CHECK(peak > .1);
    /* Direct triangle remains non-band-limited. Compare coherent records to
     * an independent Fourier sum containing only harmonics below Nyquist.
     * This bounds sampled error at these frequencies, not across all pitches. */
    const unsigned bins[] = {19,75,149};
    const double pi = 3.14159265358979323846;
    for (unsigned b=0;b<3;++b) {
        sb_audio_triangle_t triangle = {0};
        double frequency = bins[b]*48000.0/8192;
        CHECK(sb_audio_triangle_configure(&triangle,&flat));
        CHECK(sb_audio_triangle_volume(&triangle,1));
        CHECK(sb_audio_triangle_pan(&triangle,-1));
        CHECK(sb_audio_triangle_start(&triangle,frequency));
        sb_audio_triangle_render(&triangle,output,8192);
        double error = 0, re = 0, im = 0;
        for (unsigned i=0;i<8192;++i) {
            double phase = .25 + (double)i*bins[b]/8192;
            double reference = 0;
            for (unsigned harmonic=1;harmonic*bins[b]<4096;harmonic+=2)
                reference -= 8/(pi*pi)*cos(2*pi*harmonic*phase)/((double)harmonic*harmonic);
            double delta = output[2*i]-reference;
            error += delta*delta;
            re += output[2*i]*cos(2*pi*bins[b]*i/8192);
            im -= output[2*i]*sin(2*pi*bins[b]*i/8192);
        }
        double rms = sqrt(error/8192), amplitude = 2*hypot(re,im)/8192;
        CHECK(rms < .003);
        CHECK(fabs(amplitude-8/(pi*pi)) < .001);
        printf("Triangle %.6f Hz: band-limited reference RMS error %.8f\n", frequency, rms);
    }
    puts("Expanded native pitch range checks passed");
    return 0;
}
