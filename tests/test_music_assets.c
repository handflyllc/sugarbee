#include "../src/sb_music_assets.h"
#include "music_asset_fixture.h"
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Music assets line %d: %s\n",__LINE__,#e); return 1; } } while (0)
void *__real_malloc(size_t);
void __real_free(void *);
static size_t calls,live,fail_call;
void *__wrap_malloc(size_t n) {
    if (++calls==fail_call) return NULL;
    void *p=__real_malloc(n); if (p) ++live; return p;
}
void __wrap_free(void *p) { if (p) --live; __real_free(p); }
static char json[SB_MUSIC_LOAD_MAX_INPUT];
static sb_music_asset_t *asset(unsigned events) {
    size_t length=music_json(json,sizeof json,events);
    return length ? sb_music_load(json,length,NULL) : NULL;
}
int main(void) {
    sb_music_assets_t p={0}; sb_music_handle_t ids[8],id;
    CHECK(sb_music_asset_prepared_bytes(NULL)==0);
    CHECK(!sb_music_assets_get(NULL,1) && !sb_music_assets_get(&p,0));
    CHECK(sb_music_assets_unload(NULL,1,NULL)==SB_MUSIC_ASSET_ARGUMENT);
    CHECK(sb_music_assets_unload(&p,0,NULL)==SB_MUSIC_ASSET_BAD_HANDLE);
    sb_music_assets_reset(NULL);
    sb_music_asset_t *a=NULL;
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_ARGUMENT && id==0);
    for (unsigned i=0;i<8;++i) {
        a=asset(2); CHECK(a);
        size_t bytes=sb_music_asset_prepared_bytes(a),before=p.bytes;
        const sb_music_song_t *song=sb_music_asset_song(a);
        CHECK(sb_music_assets_insert(&p,&a,&ids[i])==SB_MUSIC_ASSET_OK && !a);
        CHECK(p.bytes==before+bytes && sb_music_asset_song(sb_music_assets_get(&p,ids[i]))==song);
    }
    CHECK(live==8);
    sb_music_assets_t before=p;
    a=asset(2); CHECK(a);
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_CAPACITY && !id && a);
    CHECK(!memcmp(&before,&p,sizeof p)); sb_music_asset_free(a);
    const sb_music_song_t *borrowed=sb_music_asset_song(sb_music_assets_get(&p,ids[0]));
    CHECK(sb_music_assets_unload(&p,ids[0],borrowed)==SB_MUSIC_ASSET_IN_USE);
    CHECK(!memcmp(&before,&p,sizeof p));
    CHECK(sb_music_assets_unload(&p,ids[1],borrowed)==SB_MUSIC_ASSET_OK);
    CHECK(!sb_music_assets_get(&p,ids[1]));
    a=asset(2); CHECK(a);
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_OK && id!=ids[1]);
    CHECK(sb_music_assets_unload(&p,ids[1],borrowed)==SB_MUSIC_ASSET_BAD_HANDLE);
    sb_music_assets_reset(&p); CHECK(!live && !p.bytes && p.serial==id);
    a=asset(2); CHECK(a);
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_OK && id>8);
    CHECK(!sb_music_assets_get(&p,ids[0])); sb_music_assets_reset(&p);
    /* All eight maximum-event assets currently fit below the aggregate limit.
     * Exercise real accounting first, then inject boundary byte accounting to
     * test the independent guard (future event/layout changes may reach it). */
    for (unsigned i=0;i<8;++i) {
        a=asset(4096); CHECK(a);
        CHECK(sb_music_assets_insert(&p,&a,&ids[i])==SB_MUSIC_ASSET_OK);
    }
    CHECK(p.bytes<=SB_MUSIC_ASSETS_MAX_BYTES);
    printf("Eight maximum-event assets: %zu / %u bytes\n",p.bytes,SB_MUSIC_ASSETS_MAX_BYTES);
    sb_music_assets_reset(&p); CHECK(!live);
    a=asset(2); CHECK(a); size_t bytes=sb_music_asset_prepared_bytes(a);
    p.bytes=SB_MUSIC_ASSETS_MAX_BYTES-bytes+1; before=p;
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_CAPACITY && !id && a);
    CHECK(!memcmp(&before,&p,sizeof p));
    --p.bytes;
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_OK && p.bytes==SB_MUSIC_ASSETS_MAX_BYTES);
    sb_music_assets_reset(&p); CHECK(!live && !p.bytes);
    /* Allocation failures never acquire ownership or leak temporary parser data. */
    size_t length=music_json(json,sizeof json,2); CHECK(length);
    calls=0; a=sb_music_load(json,length,NULL); CHECK(a);
    size_t allocations=calls; sb_music_asset_free(a); CHECK(!live);
    for (size_t i=1;i<=allocations;++i) {
        calls=0; fail_call=i; a=sb_music_load(json,length,NULL);
        CHECK(!a && !live && !p.bytes);
    }
    fail_call=0;
    /* Terminal serial cannot wrap, even after reset. */
    p.serial=UINT64_MAX-1; a=asset(2); CHECK(a);
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_OK && id==UINT64_MAX);
    sb_music_assets_reset(&p); before=p; a=asset(2); CHECK(a);
    CHECK(sb_music_assets_insert(&p,&a,&id)==SB_MUSIC_ASSET_EXHAUSTED && !id && a);
    CHECK(!memcmp(&before,&p,sizeof p)); sb_music_asset_free(a); CHECK(!live);
    puts("Native music asset ownership checks passed");
    return 0;
}
