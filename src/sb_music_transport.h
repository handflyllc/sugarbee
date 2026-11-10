/* Internal host lifecycle shared by private and shared-bank owners. */
#ifndef SB_MUSIC_TRANSPORT_H
#define SB_MUSIC_TRANSPORT_H
#include "sb_music_sequence.h"
#include "sb_audio_session_gain.h"
typedef struct {
    sb_audio_session_gain_t fade;
    bool paused, held_start, stop_pending;
} sb_music_transport_t;
void sb_music_transport_init(sb_music_transport_t *transport);
void sb_music_transport_started(sb_music_transport_t *transport);
void sb_music_transport_reset_music(sb_music_transport_t *transport, sb_music_sequence_t *sequence);
bool sb_music_transport_pause(sb_music_transport_t *transport, bool paused);
bool sb_music_transport_stop(sb_music_transport_t *transport, sb_music_sequence_t *sequence);
bool sb_music_transport_render(sb_music_transport_t *transport, sb_music_sequence_t *sequence,
                               float *output, size_t frames);
/* Internal non-null, serialized frame protocol: unfaded synth render; caller
 * mixes other sources and multiplies by fade.value; advance exactly once.
 * False means frozen silence: do not advance other sources or the transport. */
bool sb_music_transport_render_frame(sb_music_transport_t *transport,
    sb_music_sequence_t *sequence, float output[2]);
void sb_music_transport_advance_frame(sb_music_transport_t *transport,
    sb_music_sequence_t *sequence);
#endif
