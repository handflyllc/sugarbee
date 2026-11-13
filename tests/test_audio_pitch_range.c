#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Pitch range line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float output[16384];
static const sb_audio_voice_kind_t kinds[] = {
    SB_AUDIO_VOICE_TRIANGLE, SB_AUDIO_VOICE_SAW, SB_AUDIO_VOICE_PULSE
};
/* Independent numerical triangular-kernel integration of a rectangular PWM
 * signal. Signed edge coordinates avoid cancellation near stationary edges. */
static double pulse_reference(const sb_audio_pulse_t *p) {
    double next = p->width;
    if (p->width_left) {
        unsigned left = p->width_left-1;
        next = left ? p->width_from+(p->width_target-p->width_from)*
            (480-left)/480 : p->width_target;
    }
    double sum=0;
    for (unsigned i=0;i<16384;++i) {
        double t=(i+.5)*2/16384-1;
        double step=t<0?p->previous_step:p->frequency/48000;
        double falling=t<0?p->previous_falling_step:step-(next-p->width);
        double raw=2*(floor(p->phase+t*step)-floor(p->phase-p->width+t*falling))-1;
        sum+=raw*(1-fabs(t));
    }
    return sum*2/16384;
}
static int pulse_edges(void) {
    const double pitches[]={27.5,90,880,4186.01}, widths[]={.05,.5,.95};
    const double offsets[]={-.04,-.0003,0,.0003,.04};
    float sample;
    for (unsigned f=0;f<4;++f) for (unsigned w=0;w<3;++w)
    for (unsigned prior=0;prior<2;++prior) for (unsigned offset=0;offset<5;++offset) {
        sb_audio_pulse_t p;
        CHECK(sb_audio_pulse_reset(&p,pitches[f],widths[w]));
        CHECK(sb_audio_pulse_width(&p,w==2?.05:.95));
        p.phase=p.width+offsets[offset];
        p.previous_step=(prior?4186.01:27.5)/48000;
        /* Physically reachable prior width motion in the opposite direction. */
        p.previous_falling_step=p.previous_step-(w==2?.001875:-.001875);
        double ref=pulse_reference(&p);
        CHECK(sb_audio_pulse_render(&p,&sample,1));
        CHECK(fabs(sample-ref)<.0004 && fabs(sample)<=1);
    }
    /* Exactly stationary edge: no division by zero at the crossing. */
    sb_audio_pulse_t p;
    CHECK(sb_audio_pulse_reset(&p,50,.25));
    CHECK(sb_audio_pulse_width(&p,.75));
    double delta=.25+(.75-.25)/480-.25;
    p.frequency=delta*48000;
    CHECK(p.frequency/48000-delta==0);
    p.phase=p.width;
    double ref=pulse_reference(&p);
    CHECK(sb_audio_pulse_render(&p,&sample,1));
    CHECK(fabs(sample-ref)<.0004);
    /* Repeated retargets plus chunk boundaries exercise persisted signed history. */
    static float whole[701],split[701];
    CHECK(sb_audio_pulse_reset(&p,27.5,.05));
    for (unsigned round=0;round<100;++round) {
        CHECK(sb_audio_pulse_pitch(&p,pitches[round%4]));
        CHECK(sb_audio_pulse_width(&p,round%2?.05:.95));
        sb_audio_pulse_t copy=p;
        CHECK(sb_audio_pulse_render_conditioned(&p,whole,701));
        for (unsigned i=0;i<701;i+=37)
            CHECK(sb_audio_pulse_render_conditioned(&copy,split+i,701-i<37?701-i:37));
        CHECK(!memcmp(whole,split,sizeof whole));
        for (unsigned i=0;i<701;++i) CHECK(isfinite(whole[i]) && fabs(whole[i])<=.9500001);
    }
    /* Corrupt history refuses without state or output mutation. */
    p.previous_falling_step=.1;
    sb_audio_pulse_t copy=p;
    sample=17;
    CHECK(!sb_audio_pulse_render(&p,&sample,1));
    CHECK(sample==17 && !memcmp(&p,&copy,sizeof p));
    return 0;
}
int main(void) {
    CHECK(pulse_edges()==0);
    const double good[] = {27.5, 27.5001, 110, 110.125, 220, 440, 660, 879.75, 880, 1760, 3520, 4186.009044809578, 4186.01};
    const double bad[] = {0, -1, nextafter(27.5, -INFINITY), nextafter(4186.01, INFINITY), NAN, INFINITY, -INFINITY};
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
        CHECK(sb_audio_bank_pulse_on(&bank,v,v%2 ? 4186.01 : 27.5));
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
    /* Expanded retained pitch survives noise and returns to a pitched source. */
    for (unsigned f=0;f<sizeof(good)/sizeof(good[0]);++f) {
        sb_audio_bank_reset(&bank);
        CHECK(sb_audio_bank_noise_on(&bank,1));
        CHECK(sb_audio_bank_pitch(&bank,1,good[f]));
        CHECK(sb_audio_bank_waveform(&bank,1,SB_AUDIO_VOICE_PULSE));
        CHECK(sb_audio_bank_render(&bank,output,512));
        CHECK(bank.pulse[0].frequency==good[f] && bank.retained_pitch[0]==good[f]);
    }
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
