#ifndef SB_WAV_INTERNAL_H
#define SB_WAV_INTERNAL_H
#include "sb_wav.h"
#include <stdbool.h>
typedef struct { sb_wav_handle_t id; sb_wav_data_t data; } sb_wav_asset_t;
typedef struct {
    sb_wav_handle_t id;
    unsigned asset;
    size_t position;
    bool active, stopping;
    double volume, pan, gain[2], from[2], target[2];
    unsigned ramp;
} sb_wav_playback_t;
typedef struct {
    sb_wav_asset_t assets[SB_WAV_ASSETS];
    sb_wav_playback_t playbacks[SB_WAV_PLAYBACKS];
    size_t bytes;
    sb_wav_handle_t serial;
} sb_wav_pool_t;
/* All pool access is externally serialized. Reset/free only on control thread;
 * stop_all and render never allocate/free. Reset preserves serial numbers. */
void sb_wav_pool_reset(sb_wav_pool_t *pool);
void sb_wav_stop_all(sb_wav_pool_t *pool);
sb_wav_result_t sb_wav_insert(sb_wav_pool_t *pool, sb_wav_data_t *data, sb_wav_handle_t *id);
sb_wav_result_t sb_wav_unload(sb_wav_pool_t *pool, sb_wav_handle_t id);
sb_wav_result_t sb_wav_play(sb_wav_pool_t *pool, sb_wav_handle_t asset, double volume, double pan, sb_wav_handle_t *id);
bool sb_wav_playing(const sb_wav_pool_t *pool, sb_wav_handle_t id);
bool sb_wav_stop(sb_wav_pool_t *pool, sb_wav_handle_t id);
bool sb_wav_controls(sb_wav_pool_t *pool, sb_wav_handle_t id, double volume, double pan);
/* Replace stereo output, fixed 0.2/8 gain per voice; no clipping/normalization. */
void sb_wav_render(sb_wav_pool_t *pool, float *out, size_t frames);
#endif
