/* SugarBee fixed-simulation scheduler (experimental v0.1).
 * Pure C, independent of SDL, Lua, and any operating-system clock.
 */
#ifndef SB_TIMING_H
#define SB_TIMING_H

#define SB_UPDATES_PER_SECOND 60
#define SB_MAX_UPDATES_PER_FRAME 5

typedef struct {
    double accumulator_seconds;
} sb_timing_t;

/* Start with an empty accumulator; no simulation update is due initially. */
void sb_timing_reset(sb_timing_t *timing);

/* Fixed dt to pass to each update(dt) callback. */
double sb_timing_step_seconds(void);

/*
 * Add elapsed wall-clock time and return how many fixed updates to run.
 * Never returns more than SB_MAX_UPDATES_PER_FRAME. On stalls, excess time is
 * deliberately discarded so the console cannot get stuck catching up.
 * Negative/non-finite elapsed values are treated as zero.
 */
unsigned sb_timing_advance(sb_timing_t *timing, double elapsed_seconds);

#endif /* SB_TIMING_H */
