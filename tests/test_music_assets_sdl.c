/* Own assets through the actual SDL session; suspend dummy callback for timing. */
#include "../src/sb_audio_sdl.c"
#include "music_asset_fixture.h"
#include <math.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Owned music SDL line %d: %s\n",__LINE__,#e); return 1; } } while (0)
void *__real_malloc(size_t);
void __real_free(void *);
static size_t allocations,frees,fail_call,live;
static void *tracked[128];
void *__wrap_malloc(size_t n) {
    if (++allocations==fail_call) return NULL;
    void *p=__real_malloc(n);
    if (p) for (unsigned i=0;i<128;++i) if (!tracked[i]) { tracked[i]=p; ++live; break; }
    return p;
}
void __wrap_free(void *p) {
    ++frees;
    if (p) for (unsigned i=0;i<128;++i) if (tracked[i]==p) { tracked[i]=NULL; --live; break; }
    __real_free(p);
}
static float out[2048];
static bool render(sb_audio_t *a,unsigned frames) {
    size_t m=allocations,f=frees;
    audio_output(a,(Uint8 *)out,(int)(frames*2*sizeof(float)));
    for (unsigned i=0;i<frames*2;++i) if (!isfinite(out[i])) return false;
    return allocations==m && frees==f;
}
static char json[8192];
static const unsigned map[]={3};
static sb_music_asset_code_t load(sb_audio_t *a,sb_music_handle_t *id) {
    return sb_audio_music_load(a,json,strlen(json),id,NULL);
}
int main(void) {
    CHECK(music_json(json,sizeof json,2));
    sb_music_handle_t id=99,other,ids[8]; sb_music_load_result_t detail;
    CHECK(load(NULL,&id)==SB_MUSIC_ASSET_UNAVAILABLE && !id);
    CHECK(sb_audio_music_unload(NULL,1)==SB_MUSIC_ASSET_UNAVAILABLE);
    CHECK(sb_audio_music_play(NULL,1,map,1,false)==SB_MUSIC_ASSET_UNAVAILABLE);
    sb_audio_t *a=sb_audio_create(); CHECK(a); SDL_PauseAudioDevice(a->device,1);
    CHECK(load(a,&id)==SB_MUSIC_ASSET_REJECTED && !id);
    CHECK(sb_audio_music_load(a,json,strlen(json),NULL,NULL)==SB_MUSIC_ASSET_ARGUMENT);
    sb_audio_session_begin(a);
    CHECK(sb_audio_music_load(a,json,strlen(json),&id,&detail)==SB_MUSIC_ASSET_OK);
    CHECK(id && a->music_assets.bytes==detail.prepared_bytes && live==1);
    CHECK(load(a,&other)==SB_MUSIC_ASSET_OK && live==2);
    CHECK(sb_audio_voice_configure(a,9,0,0,1,.1) && sb_audio_voice_saw_on(a,9,660));
    CHECK(sb_audio_music_play(a,id,map,1,true)==SB_MUSIC_ASSET_OK);
    CHECK(render(a,10));
    sb_music_shared_t state=a->music; sb_music_assets_t pool=a->music_assets;
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_IN_USE);
    CHECK(sb_audio_music_play(a,other,map,1,false)==SB_MUSIC_ASSET_REJECTED);
    CHECK(sb_audio_music_play(a,0,map,1,false)==SB_MUSIC_ASSET_BAD_HANDLE);
    CHECK(sb_audio_music_load(a,"{}",2,&ids[0],&detail)==SB_MUSIC_ASSET_LOAD_FAILED && !ids[0]);
    CHECK(detail.code==SB_MUSIC_LOAD_SCHEMA && detail.message[0]);
    CHECK(!memcmp(&state,&a->music,sizeof state) && !memcmp(&pool,&a->music_assets,sizeof pool));
    CHECK(live==2);
    /* Fault every allocation of a successful load while another asset plays. */
    size_t start=allocations; CHECK(load(a,&ids[0])==SB_MUSIC_ASSET_OK);
    size_t steps=allocations-start; CHECK(sb_audio_music_unload(a,ids[0])==SB_MUSIC_ASSET_OK);
    pool=a->music_assets;
    for (size_t i=1;i<=steps;++i) {
        fail_call=allocations+i;
        CHECK(load(a,&ids[0])==SB_MUSIC_ASSET_LOAD_FAILED && !ids[0]);
        CHECK(live==2 && !memcmp(&pool,&a->music_assets,sizeof pool));
        CHECK(!memcmp(&state,&a->music,sizeof state));
    }
    fail_call=0;
    CHECK(sb_audio_music_unload(a,other)==SB_MUSIC_ASSET_OK && live==1);
    CHECK(sb_audio_music_stop(a));
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_IN_USE);
    CHECK(render(a,479)); CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_IN_USE);
    CHECK(render(a,1)); CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE);
    CHECK(sb_audio_voice_active(a,9));
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_OK && !live);
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_BAD_HANDLE);
    CHECK(load(a,&other)==SB_MUSIC_ASSET_OK && other!=id);
    CHECK(sb_audio_music_play(a,id,map,1,false)==SB_MUSIC_ASSET_BAD_HANDLE);
    /* Paused/held music remains in use; cancelling held start releases borrow. */
    sb_audio_session_pause(a,true); CHECK(render(a,480));
    CHECK(sb_audio_music_play(a,other,map,1,true)==SB_MUSIC_ASSET_OK);
    CHECK(sb_audio_music_unload(a,other)==SB_MUSIC_ASSET_IN_USE);
    CHECK(sb_audio_music_stop(a));
    CHECK(sb_audio_music_unload(a,other)==SB_MUSIC_ASSET_OK && !live);
    /* Resumed stop fade still retains the asset until the player finishes. */
    sb_audio_session_pause(a,false); CHECK(render(a,480));
    CHECK(load(a,&id)==SB_MUSIC_ASSET_OK);
    CHECK(sb_audio_music_play(a,id,map,1,true)==SB_MUSIC_ASSET_OK); CHECK(render(a,10));
    sb_audio_session_pause(a,true); CHECK(render(a,5)); CHECK(sb_audio_music_stop(a));
    sb_audio_session_pause(a,false);
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_IN_USE);
    CHECK(render(a,480)); CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_OK);
    /* Capacity refusal preserves the live player and all existing handles. */
    for (unsigned i=0;i<8;++i) CHECK(load(a,&ids[i])==SB_MUSIC_ASSET_OK);
    CHECK(sb_audio_music_play(a,ids[0],map,1,true)==SB_MUSIC_ASSET_OK);
    state=a->music; pool=a->music_assets;
    CHECK(load(a,&id)==SB_MUSIC_ASSET_CAPACITY && !id && live==8);
    CHECK(!memcmp(&state,&a->music,sizeof state) && !memcmp(&pool,&a->music_assets,sizeof pool));
    CHECK(sb_audio_music_reset(a)); CHECK(sb_audio_voice_active(a,9));
    CHECK(sb_audio_music_unload(a,ids[0])==SB_MUSIC_ASSET_OK && live==7);
    /* Failure cancels borrowing without callback frees, keeps assets to teardown. */
    CHECK(sb_audio_music_play(a,ids[1],map,1,true)==SB_MUSIC_ASSET_OK); CHECK(render(a,10));
    sb_audio_session_fail(a); CHECK(render(a,480));
    CHECK(!a->music.sequence.song && live==7 && a->music_assets.bytes);
    CHECK(load(a,&id)==SB_MUSIC_ASSET_REJECTED && !id);
    CHECK(sb_audio_music_unload(a,ids[1])==SB_MUSIC_ASSET_REJECTED);
    CHECK(sb_audio_music_play(a,ids[1],map,1,true)==SB_MUSIC_ASSET_REJECTED);
    sb_audio_session_end(a); CHECK(!live && !a->music_assets.bytes);
    sb_audio_session_begin(a); CHECK(load(a,&id)==SB_MUSIC_ASSET_OK && id>ids[7]);
    CHECK(sb_audio_music_play(a,ids[1],map,1,true)==SB_MUSIC_ASSET_BAD_HANDLE);
    /* Source JSON may be discarded immediately; natural completion frees no asset. */
    memset(json,0,sizeof json);
    CHECK(sb_audio_music_play(a,id,map,1,false)==SB_MUSIC_ASSET_OK);
    for (unsigned i=0;i<101;++i) CHECK(render(a,1000));
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE && live==1);
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_OK && !live);
    CHECK(music_json(json,sizeof json,2)); CHECK(load(a,&id)==SB_MUSIC_ASSET_OK);
    CHECK(sb_audio_music_play(a,id,map,1,true)==SB_MUSIC_ASSET_OK);
    /* Begin also tears down an existing session safely, even without a callback. */
    sb_audio_session_begin(a); CHECK(!live && !a->music.sequence.song);
    CHECK(load(a,&other)==SB_MUSIC_ASSET_OK && other>id);
    CHECK(sb_audio_music_play(a,other,map,1,true)==SB_MUSIC_ASSET_OK);
    sb_audio_session_pause(a,true); CHECK(render(a,480));
    CHECK(sb_audio_music_unload(a,other)==SB_MUSIC_ASSET_IN_USE);
    CHECK(sb_audio_music_reset(a)); CHECK(sb_audio_music_unload(a,other)==SB_MUSIC_ASSET_OK);
    CHECK(load(a,&id)==SB_MUSIC_ASSET_OK);
    CHECK(sb_audio_music_play(a,id,map,1,true)==SB_MUSIC_ASSET_OK);
    sb_audio_destroy(a); CHECK(!live);
    puts("Owned production music lifecycle checks passed");
    return 0;
}
