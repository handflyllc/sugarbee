/* Optional host diagnostics. Pure C; times are seconds, supplied by the host. */
#ifndef SB_STATS_H
#define SB_STATS_H
#include <stdbool.h>
#include <stddef.h>
#define SB_STATS_CAPACITY 4096

typedef struct { double end, frame, update, draw, present, dropped; } sb_stats_sample_t;
typedef struct {
    bool ready, limited;
    double fps, frame_average, frame_worst, update, draw, present, dropped;
} sb_stats_view_t;
typedef struct {
    sb_stats_sample_t samples[SB_STATS_CAPACITY];
    size_t head, count;
    double now, next_refresh, overflow_at;
    sb_stats_view_t view;
} sb_stats_t;
void sb_stats_reset(sb_stats_t *stats);
void sb_stats_add(sb_stats_t *stats, double frame, double update, double draw,
                  double present, double dropped);
#endif
