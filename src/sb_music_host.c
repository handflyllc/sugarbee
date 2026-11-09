#include "sb_music_host.h"
#include <string.h>

void sb_music_host_reset(sb_music_host_t *h) {
    if (!h) return;
    memset(h, 0, sizeof(*h));
    h->fade.value = h->fade.from = h->fade.target = 1;
}
bool sb_music_host_start(sb_music_host_t *h, const sb_music_song_t *song, bool loop) {
    if (!h) return false;
    bool ok = loop ? sb_music_sequence_start_loop(&h->sequence, song) :
                     sb_music_sequence_start(&h->sequence, song);
    if (ok) {
        h->held_start = h->paused;
        h->stop_pending = false;
    }
    return ok;
}
bool sb_music_host_pause(sb_music_host_t *h, bool paused) {
    if (!h) return false;
    if (h->paused != paused) {
        h->paused = paused;
        if (!paused) h->held_start = false;
        sb_audio_session_gain_ramp(&h->fade, paused ? 0 : 1);
    }
    return true;
}
bool sb_music_host_stop(sb_music_host_t *h) {
    if (!h) return false;
    if (h->held_start || (h->paused && !h->fade.left)) {
        sb_music_sequence_reset(&h->sequence);
        h->held_start = h->stop_pending = false;
    } else {
        (void)sb_music_sequence_stop(&h->sequence);
        h->stop_pending = h->paused && h->sequence.state != SB_MUSIC_IDLE;
    }
    return true;
}
bool sb_music_host_render(sb_music_host_t *h, float *output, size_t frames) {
    if (!h || (frames && !output) || frames > SIZE_MAX/(2*sizeof(float))) return false;
    for (size_t n = 0; n < frames; ++n) {
        output[2*n] = output[2*n+1] = 0;
        if (h->paused && !h->fade.left) continue;
        if (!h->held_start) {
            (void)sb_music_sequence_render(&h->sequence, output+2*n, 1);
            output[2*n] = (float)(output[2*n]*h->fade.value);
            output[2*n+1] = (float)(output[2*n+1]*h->fade.value);
        }
        sb_audio_session_gain_advance(&h->fade);
        if (h->stop_pending && h->paused && !h->fade.left)
            sb_music_sequence_reset(&h->sequence);
        if (h->sequence.state == SB_MUSIC_IDLE) h->stop_pending = false;
    }
    return true;
}
