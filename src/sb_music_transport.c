#include "sb_music_transport.h"
#include <string.h>
void sb_music_transport_init(sb_music_transport_t *t) {
    memset(t,0,sizeof(*t));
    t->fade.value=t->fade.from=t->fade.target=1;
}
void sb_music_transport_started(sb_music_transport_t *t) {
    t->held_start=t->paused;
    t->stop_pending=false;
}
void sb_music_transport_reset_music(sb_music_transport_t *t, sb_music_sequence_t *s) {
    sb_music_sequence_reset(s);
    t->held_start=t->stop_pending=false;
}
bool sb_music_transport_pause(sb_music_transport_t *t, bool paused) {
    if (!t) return false;
    if (t->paused!=paused) {
        t->paused=paused;
        if (!paused) t->held_start=false;
        sb_audio_session_gain_ramp(&t->fade,paused?0:1);
    }
    return true;
}
bool sb_music_transport_stop(sb_music_transport_t *t, sb_music_sequence_t *s) {
    if (!t || !s) return false;
    if (t->held_start || (t->paused && !t->fade.left))
        sb_music_transport_reset_music(t,s);
    else {
        (void)sb_music_sequence_stop(s);
        t->stop_pending=t->paused && s->state!=SB_MUSIC_IDLE;
    }
    return true;
}
bool sb_music_transport_render(sb_music_transport_t *t, sb_music_sequence_t *s,
                               float *output, size_t frames) {
    if (!t || !s || (frames && !output) || frames>SIZE_MAX/(2*sizeof(float))) return false;
    for (size_t n=0;n<frames;++n) {
        output[2*n]=output[2*n+1]=0;
        if (t->paused && !t->fade.left) continue;
        if (!t->held_start)
            (void)sb_music_sequence_render(s,output+2*n,1);
        else if (s->shared)
            /* A held music start must not freeze an already sounding effect
             * during the host fade. Skip reserved slots entirely. */
            (void)sb_audio_bank_render_unreserved(&s->bank,s->reserved,output+2*n,1);
        output[2*n]=(float)(output[2*n]*t->fade.value);
        output[2*n+1]=(float)(output[2*n+1]*t->fade.value);
        sb_audio_session_gain_advance(&t->fade);
        if (t->stop_pending && t->paused && !t->fade.left)
            sb_music_transport_reset_music(t,s);
        if (s->state==SB_MUSIC_IDLE) t->stop_pending=false;
    }
    return true;
}
