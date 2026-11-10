/* Experimental host lifecycle adapter. No SDL, Lua, allocation or shared voices.
 * This is host pause, not an independently pausable music API. */
#ifndef SB_MUSIC_HOST_H
#define SB_MUSIC_HOST_H
#include "sb_music_sequence.h"
#include "sb_music_transport.h"
typedef struct {
    sb_music_sequence_t sequence;
    sb_music_transport_t transport;
} sb_music_host_t;
/* Initialize/end with reset: silent idle, unpaused, host gain 1. All calls must
 * be serialized with rendering. Never operate the embedded sequence directly
 * except set_gain/query; bypassing this adapter loses paused-stop semantics. */
void sb_music_host_reset(sb_music_host_t *host);
/* Preparation only. Same atomic refusal and borrowed lifetime as sequence start.
 * A start while pause is requested stays at frame zero, including during fade. */
bool sb_music_host_start(sb_music_host_t *host, const sb_music_song_t *song, bool loop);
bool sb_music_host_pause(sb_music_host_t *host, bool paused);
/* Stop fully paused/held playback immediately. During pause fade, cancel events
 * immediately and clean up no later than silence. Resume never cancels stop. */
bool sb_music_host_stop(sb_music_host_t *host);
/* Replace stereo output. Fully paused render emits zero without advancing music.
 * Invalid buffers/zero frames do not advance. Fade reversals take 480 frames
 * from current level; repeated same pause requests do not restart a fade. */
bool sb_music_host_render(sb_music_host_t *host, float *output, size_t frames);
#endif
