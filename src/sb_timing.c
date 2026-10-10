#include "sb_timing.h"

#include <stddef.h>

#define SB_MAX_FRAME_SECONDS 0.25

void sb_timing_reset(sb_timing_t *timing) {
    if (timing) timing->accumulator_seconds = 0.0;
}

double sb_timing_step_seconds(void) {
    return 1.0 / (double)SB_UPDATES_PER_SECOND;
}

unsigned sb_timing_advance(sb_timing_t *timing, double elapsed_seconds) {
    if (!timing) return 0;

    /* The comparison also rejects NaN without adding a libm dependency. */
    if (!(elapsed_seconds > 0.0)) elapsed_seconds = 0.0;
    if (elapsed_seconds > SB_MAX_FRAME_SECONDS) elapsed_seconds = SB_MAX_FRAME_SECONDS;

    const double step = sb_timing_step_seconds();
    timing->accumulator_seconds += elapsed_seconds;

    unsigned updates = 0;
    /* Tiny tolerance accommodates repeated floating-point additions. */
    while (updates < SB_MAX_UPDATES_PER_FRAME &&
           timing->accumulator_seconds + step * 1e-9 >= step) {
        timing->accumulator_seconds -= step;
        ++updates;
    }
    if (timing->accumulator_seconds < 0.0)
        timing->accumulator_seconds = 0.0;

    if (updates == SB_MAX_UPDATES_PER_FRAME &&
        timing->accumulator_seconds + step * 1e-9 >= step) {
        /* Drop excess backlog instead of entering a catch-up spiral. */
        timing->accumulator_seconds = 0.0;
    }
    return updates;
}
