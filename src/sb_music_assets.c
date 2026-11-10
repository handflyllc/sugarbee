#include "sb_music_assets.h"
#include <string.h>
const sb_music_asset_t *sb_music_assets_get(const sb_music_assets_t *p,sb_music_handle_t id) {
    if (!p || !id) return NULL;
    for (unsigned i=0;i<SB_MUSIC_ASSETS;++i)
        if (p->slots[i].id==id) return p->slots[i].asset;
    return NULL;
}
sb_music_asset_code_t sb_music_assets_insert(sb_music_assets_t *p,
    sb_music_asset_t **asset,sb_music_handle_t *id) {
    if (!id) return SB_MUSIC_ASSET_ARGUMENT;
    *id=0;
    if (!p || !asset || !*asset) return SB_MUSIC_ASSET_ARGUMENT;
    size_t bytes=sb_music_asset_prepared_bytes(*asset);
    if (bytes>SB_MUSIC_ASSETS_MAX_BYTES || p->bytes>SB_MUSIC_ASSETS_MAX_BYTES-bytes)
        return SB_MUSIC_ASSET_CAPACITY;
    unsigned i;
    for (i=0;i<SB_MUSIC_ASSETS && p->slots[i].asset;++i) {}
    if (i==SB_MUSIC_ASSETS) return SB_MUSIC_ASSET_CAPACITY;
    if (p->serial==UINT64_MAX) return SB_MUSIC_ASSET_EXHAUSTED;
    *id=++p->serial;
    p->slots[i]=(sb_music_asset_slot_t){*id,*asset};
    p->bytes+=bytes;
    *asset=NULL;
    return SB_MUSIC_ASSET_OK;
}
sb_music_asset_code_t sb_music_assets_unload(sb_music_assets_t *p,
    sb_music_handle_t id,const sb_music_song_t *borrowed) {
    if (!p) return SB_MUSIC_ASSET_ARGUMENT;
    if (!id) return SB_MUSIC_ASSET_BAD_HANDLE;
    for (unsigned i=0;i<SB_MUSIC_ASSETS;++i) if (p->slots[i].id==id) {
        sb_music_asset_t *asset=p->slots[i].asset;
        if (sb_music_asset_song(asset)==borrowed) return SB_MUSIC_ASSET_IN_USE;
        p->bytes-=sb_music_asset_prepared_bytes(asset);
        sb_music_asset_free(asset);
        p->slots[i]=(sb_music_asset_slot_t){0};
        return SB_MUSIC_ASSET_OK;
    }
    return SB_MUSIC_ASSET_BAD_HANDLE;
}
void sb_music_assets_reset(sb_music_assets_t *p) {
    if (!p) return;
    for (unsigned i=0;i<SB_MUSIC_ASSETS;++i) sb_music_asset_free(p->slots[i].asset);
    memset(p->slots,0,sizeof p->slots);
    p->bytes=0;
}
