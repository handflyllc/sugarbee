#include "sb_lua_audio.h"
#include "sb_assets.h"
#include "sb_wav_internal.h"
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define ASSET_MT "sugarbee.wav.asset"
#define PLAY_MT "sugarbee.wav.playback"
struct sb_lua_audio {
    sb_audio_t *audio;
    const char *cartridge;
    sb_wav_pool_t silent;
    bool closed, failed;
};
typedef struct {
    sb_lua_audio_t *owner;
    sb_wav_handle_t id;
    double volume,pan;
} handle_t;
sb_lua_audio_t *sb_lua_audio_create(sb_audio_t *audio,const char *cartridge) {
    if (!cartridge) return NULL;
    sb_lua_audio_t *c=calloc(1,sizeof(*c));
    if (c) { c->audio=audio; c->cartridge=cartridge; }
    return c;
}
void sb_lua_audio_fail(sb_lua_audio_t *c) { if (c) c->failed=true; }
void sb_lua_audio_end(sb_lua_audio_t *c) {
    if (c) { c->closed=true; sb_wav_pool_reset(&c->silent); }
}
void sb_lua_audio_destroy(sb_lua_audio_t *c) { sb_lua_audio_end(c); free(c); }
static sb_lua_audio_t *context(lua_State *L) { return lua_touserdata(L,lua_upvalueindex(1)); }
static bool usable(sb_lua_audio_t *c) { return c && !c->closed && !c->failed; }
static void arity(lua_State *L,int n) {
    if (lua_gettop(L)!=n) luaL_error(L,"audio: expected %d arguments",n);
}
static int failure(lua_State *L,sb_wav_result_t r,bool boolean) {
    if (boolean) lua_pushboolean(L,0); else lua_pushnil(L);
    lua_pushstring(L,sb_wav_result_string(r)); return 2;
}
static double number(lua_State *L,int idx,const char *field,double low,double high) {
    double n=lua_tonumber(L,idx);
    if (lua_type(L,idx)!=LUA_TNUMBER || !isfinite(n) || n<low || n>high)
        luaL_error(L,"audio: %s must be a finite number in [%f, %f]",field,low,high);
    return n;
}
static bool key(lua_State *L,int idx,const char *name) {
    size_t n; const char *s;
    if (lua_type(L,idx)!=LUA_TSTRING) return false;
    s=lua_tolstring(L,idx,&n);
    return n==strlen(name) && !memcmp(s,name,n);
}
static handle_t *check_handle(lua_State *L,const char *type) {
    handle_t *h=luaL_testudata(L,1,type);
    if (!h || h->owner!=context(L) || !h->id) luaL_error(L,"audio: invalid or unloaded handle");
    return h;
}
/* Allocating all Lua-owned handle storage before acquiring native resources
 * avoids leaks if Lua raises a memory error. No __gc owns native audio. */
static handle_t *new_handle(lua_State *L,const char *type) {
    handle_t *h=lua_newuserdatauv(L,sizeof(*h),0);
    *h=(handle_t){context(L),0,0,0}; luaL_setmetatable(L,type); return h;
}
static int load(lua_State *L) {
    arity(L,1);
    size_t length;
    if (lua_type(L,1)!=LUA_TSTRING) return luaL_error(L,"audio.load: path must be a string");
    const char *path=lua_tolstring(L,1,&length);
    if (!length || memchr(path,0,length)) return luaL_error(L,"audio.load: invalid path");
    sb_lua_audio_t *c=context(L);
    if (!usable(c)) return failure(L,SB_WAV_REJECTED,false);
    handle_t *h=new_handle(L,ASSET_MT); /* [path, prepared handle] */
    uint8_t *bytes=NULL; size_t size=0;
    sb_asset_result_t read=sb_asset_read(c->cartridge,path,&bytes,&size);
    if (read!=SB_ASSET_OK) {
        free(bytes);
        return luaL_error(L,"audio.load: %s",sb_asset_error_string(read));
    }
    sb_wav_result_t r;
    if (c->audio) r=sb_audio_wav_load(c->audio,bytes,size,&h->id);
    else {
        sb_wav_data_t data={0};
        r=sb_wav_decode(bytes,size,&data);
        if (r==SB_WAV_OK) r=sb_wav_insert(&c->silent,&data,&h->id);
        sb_wav_free(&data);
    }
    free(bytes); /* No Lua calls occur while temporary native storage is owned. */
    if (r==SB_WAV_OK) return 1;
    lua_pop(L,1);
    if (r==SB_WAV_INVALID || r==SB_WAV_UNSUPPORTED || r==SB_WAV_TOO_LARGE)
        return luaL_error(L,"audio.load: %s",sb_wav_result_string(r));
    return failure(L,r,false);
}
static int play(lua_State *L) {
    int argc=lua_gettop(L);
    if (argc!=1 && argc!=2) return luaL_error(L,"audio.play: expected 1 or 2 arguments");
    handle_t *asset=check_handle(L,ASSET_MT);
    double volume=.5,pan=0;
    if (argc==2) {
        if (lua_type(L,2)!=LUA_TTABLE) return luaL_error(L,"audio.play: options must be a table");
        lua_pushnil(L);
        /* Raw option iteration; inherited fields/metamethods are not used. */
        while (lua_next(L,2)) {
            if (key(L,-2,"volume")) volume=number(L,-1,"volume",0,1);
            else if (key(L,-2,"pan")) pan=number(L,-1,"pan",-1,1);
            else if (key(L,-2,"loop")) {
                if (lua_type(L,-1)!=LUA_TBOOLEAN || lua_toboolean(L,-1))
                    return luaL_error(L,"audio.play: only loop=false is supported");
            } else return luaL_error(L,"audio.play: unknown option");
            lua_pop(L,1);
        }
    }
    sb_lua_audio_t *c=context(L);
    if (!usable(c)) return failure(L,SB_WAV_REJECTED,false);
    if (!c->audio) return failure(L,SB_WAV_UNAVAILABLE,false);
    handle_t *h=new_handle(L,PLAY_MT);
    sb_wav_result_t r=sb_audio_wav_play(c->audio,asset->id,volume,pan,&h->id);
    if (r!=SB_WAV_OK) { lua_pop(L,1); return failure(L,r,false); }
    h->volume=volume; h->pan=pan;
    return 1;
}
static int unload(lua_State *L) {
    arity(L,1); handle_t *h=check_handle(L,ASSET_MT);
    sb_lua_audio_t *c=context(L);
    if (!usable(c)) return failure(L,SB_WAV_REJECTED,true);
    sb_wav_result_t r=c->audio ? sb_audio_wav_unload(c->audio,h->id) : sb_wav_unload(&c->silent,h->id);
    if (r!=SB_WAV_OK) return failure(L,r,true);
    h->id=0; lua_pushboolean(L,1); return 1;
}
static int is_playing(lua_State *L) {
    arity(L,1); handle_t *h=check_handle(L,PLAY_MT); sb_lua_audio_t *c=context(L);
    lua_pushboolean(L,c && !c->closed && sb_audio_wav_playing(c->audio,h->id)); return 1;
}
static int stop(lua_State *L) {
    arity(L,1); handle_t *h=check_handle(L,PLAY_MT); sb_lua_audio_t *c=context(L);
    lua_pushboolean(L,usable(c) && sb_audio_wav_stop(c->audio,h->id)); return 1;
}
static int control(lua_State *L,bool pan) {
    arity(L,2); handle_t *h=check_handle(L,PLAY_MT);
    double value=number(L,2,pan ? "pan" : "volume",pan ? -1 : 0,1);
    double volume=pan ? h->volume : value, balance=pan ? value : h->pan;
    sb_lua_audio_t *c=context(L);
    bool ok=usable(c) && sb_audio_wav_controls(c->audio,h->id,volume,balance);
    if (ok) { h->volume=volume; h->pan=balance; }
    lua_pushboolean(L,ok); return 1;
}
static int set_volume(lua_State *L) { return control(L,false); }
static int set_pan(lua_State *L) { return control(L,true); }
void sb_lua_audio_register(lua_State *L,sb_lua_audio_t *c) {
    const char *types[]={ASSET_MT,PLAY_MT};
    for (unsigned i=0;i<2;++i) {
        luaL_newmetatable(L,types[i]);
        lua_pushliteral(L,"protected audio handle"); lua_setfield(L,-2,"__metatable");
        lua_pop(L,1);
    }
    static const luaL_Reg functions[]={{"load",load},{"play",play},{"unload",unload},
        {"is_playing",is_playing},{"stop",stop},{"set_volume",set_volume},{"set_pan",set_pan},{NULL,NULL}};
    lua_newtable(L); lua_pushlightuserdata(L,c); luaL_setfuncs(L,functions,1);
    lua_setfield(L,-2,"audio");
}
