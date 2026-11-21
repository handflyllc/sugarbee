#include "sb_stats.h"
#include <math.h>

void sb_stats_reset(sb_stats_t *stats) {
    if (!stats) return;
    stats->head = stats->count = 0;
    stats->now = 0;
    stats->next_refresh = 0.5;
    stats->overflow_at = -2;
    stats->view = (sb_stats_view_t){0};
}

static bool valid(double value) {
    return isfinite(value) && value >= 0;
}

void sb_stats_add(sb_stats_t *stats, double frame, double update, double draw,
                  double present, double dropped) {
    if (!stats || !valid(frame) || frame == 0 || !valid(update) || !valid(draw) ||
        !valid(present) || !valid(dropped) || !isfinite(stats->now + frame)) return;
    stats->now += frame;

    /* Samples belong to the one-second window by their presentation end time.
       A long interval crossing the boundary retains its full cost/spike. */
    while (stats->count &&
           stats->samples[stats->head].end <= stats->now - 1.0 + 1e-9) {
        stats->head = (stats->head + 1) % SB_STATS_CAPACITY;
        --stats->count;
    }
    if (stats->count == SB_STATS_CAPACITY) {
        stats->overflow_at = stats->now;
        stats->head = (stats->head + 1) % SB_STATS_CAPACITY;
        --stats->count;
    }
    size_t tail = (stats->head + stats->count) % SB_STATS_CAPACITY;
    stats->samples[tail] = (sb_stats_sample_t){stats->now, frame, update, draw, present, dropped};
    ++stats->count;
    if (stats->now + 1e-9 < stats->next_refresh) return;
    stats->next_refresh = stats->now + 0.5;

    sb_stats_view_t view = {0};
    view.ready = stats->now + 1e-9 >= 1.0;
    view.limited = stats->now - stats->overflow_at <= 1.0;
    if (view.ready && !view.limited) {
        view.fps = (double)stats->count;
        for (size_t i = 0; i < stats->count; ++i) {
            const sb_stats_sample_t *sample = &stats->samples[(stats->head + i) % SB_STATS_CAPACITY];
            view.frame_average += sample->frame;
            view.update += sample->update;
            view.draw += sample->draw;
            view.present += sample->present;
            view.dropped += sample->dropped;
            if (sample->frame > view.frame_worst) view.frame_worst = sample->frame;
        }
        view.frame_average /= stats->count;
        view.update /= stats->count;
        view.draw /= stats->count;
        view.present /= stats->count;
    }
    stats->view = view;
}
