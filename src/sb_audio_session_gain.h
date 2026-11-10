/* Internal host fade shared by the SDL session and native music diagnostic.
 * Fixed 48 kHz; use current value for a sample, then advance once per frame. */
#ifndef SB_AUDIO_SESSION_GAIN_H
#define SB_AUDIO_SESSION_GAIN_H
#define SB_AUDIO_SESSION_FADE_FRAMES 480u
typedef struct {
    double value, from, target;
    unsigned left;
} sb_audio_session_gain_t;
static inline void sb_audio_session_gain_ramp(sb_audio_session_gain_t *g, double target) {
    g->from = g->value;
    g->target = target;
    g->left = g->value == target ? 0 : SB_AUDIO_SESSION_FADE_FRAMES;
}
static inline void sb_audio_session_gain_advance(sb_audio_session_gain_t *g) {
    if (g->left) {
        --g->left;
        g->value = g->left ? g->from + (g->target-g->from)*
            (SB_AUDIO_SESSION_FADE_FRAMES-g->left)/SB_AUDIO_SESSION_FADE_FRAMES : g->target;
    }
}
#endif
