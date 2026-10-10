/* Deterministic timing tests; do not require a display or real-time sleeps. */
#include "sb_timing.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Timing test failed at line %d: %s\n", __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void) {
    sb_timing_t timer;
    const double dt = sb_timing_step_seconds();
    CHECK(dt > 0.0);
    sb_timing_reset(&timer);

    /* A fast rendering frame may have no simulation update. */
    CHECK(sb_timing_advance(&timer, dt * 0.25) == 0);
    CHECK(sb_timing_advance(&timer, dt * 0.75) == 1);

    /* A slower rendering frame may need multiple simulation updates. */
    CHECK(sb_timing_advance(&timer, dt * 3.0) == 3);
    CHECK(sb_timing_advance(&timer, 0.0) == 0);

    /* Time remaining below one tick is retained. */
    CHECK(sb_timing_advance(&timer, dt * 0.5) == 0);
    CHECK(sb_timing_advance(&timer, dt * 0.5) == 1);

    /* Catch-up is capped and excess backlog is discarded. */
    CHECK(sb_timing_advance(&timer, 20.0) == SB_MAX_UPDATES_PER_FRAME);
    CHECK(sb_timing_advance(&timer, 0.0) == 0);

    /* Erroneous elapsed inputs cannot drive the scheduler backwards. */
    CHECK(sb_timing_advance(&timer, -1.0) == 0);
    CHECK(sb_timing_advance(&timer, NAN) == 0);
    CHECK(sb_timing_advance(NULL, dt) == 0);
    sb_timing_reset(NULL);

    /* Over one simulated second, 60 fixed updates are delivered. */
    sb_timing_reset(&timer);
    unsigned total = 0;
    for (int i = 0; i < 120; ++i)
        total += sb_timing_advance(&timer, dt * 0.5);
    CHECK(total == SB_UPDATES_PER_SECOND);
    return EXIT_SUCCESS;
}
