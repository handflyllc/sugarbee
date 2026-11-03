#include "sb_audio_filter.h"
#include <math.h>

/* Choose a in y[n] = a*x[n] + (1-a)*y[n-1] so magnitude at cutoff is
 * 1/sqrt(2). Solving a*a = 2*(1-a)*(1-cos(omega)) gives this positive root.
 * For supported cutoffs 0<a<1, making every output a convex combination. */
static double coefficient(double cutoff) {
    double k = 1-cos(6.28318530717958647693*cutoff/48000);
    return sqrt(k*k+2*k)-k;
}
static void initialize(sb_audio_filter_t *f) {
    if (f->initialized) return;
    f->cutoff = 2000;
    f->coefficient = f->coefficient_target = coefficient(f->cutoff);
    f->initialized = true;
}
static void set_target(double *value, double *from, double *target,
                       unsigned *left, double next, bool active) {
    if (!active) { *value = *target = next; *left = 0; }
    else if (*target != next) {
        *from = *value;
        *target = next;
        *left = SB_AUDIO_FILTER_FRAMES;
    }
}
static void advance(double *value, double from, double target, unsigned *left) {
    if (!*left) return;
    --*left;
    *value = *left ? from + (target-from)*(SB_AUDIO_FILTER_FRAMES-*left)/SB_AUDIO_FILTER_FRAMES : target;
}
bool sb_audio_filter_cutoff(sb_audio_filter_t *f, double cutoff, bool active) {
    if (!f || !isfinite(cutoff) || cutoff < 20 || cutoff > 20000) return false;
    initialize(f);
    if (f->cutoff == cutoff && active) return true;
    f->cutoff = cutoff;
    set_target(&f->coefficient, &f->coefficient_from, &f->coefficient_target,
               &f->coefficient_left, coefficient(cutoff), active);
    return true;
}
bool sb_audio_filter_bypass(sb_audio_filter_t *f, bool bypass, bool active) {
    if (!f) return false;
    initialize(f);
    set_target(&f->wet, &f->wet_from, &f->wet_target, &f->wet_left, bypass ? 0 : 1, active);
    return true;
}
void sb_audio_filter_start(sb_audio_filter_t *f) {
    initialize(f);
    f->state = 0;
    f->coefficient = f->coefficient_target;
    f->wet = f->wet_target;
    f->coefficient_left = f->wet_left = 0;
}
double sb_audio_filter_sample(sb_audio_filter_t *f, double input) {
    f->state += f->coefficient*(input-f->state);
    double output = f->wet == 0 ? input : (1-f->wet)*input + f->wet*f->state;
    advance(&f->coefficient, f->coefficient_from, f->coefficient_target, &f->coefficient_left);
    advance(&f->wet, f->wet_from, f->wet_target, &f->wet_left);
    return output;
}
