#include "sb_lua_music.h"
#include "sb_audio_music.h"
#include <lauxlib.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define MUSIC_MT "sugarbee.music.asset"
struct sb_lua_music {
    sb_audio_t *audio;
    const char *cartridge;
    sb_music_assets_t silent;
    bool closed, failed;
};
typedef struct { sb_lua_music_t *owner; sb_music_handle_t id; } music_handle_t;
sb_lua_music_t *sb_lua_music_create(sb_audio_t *audio,const char *cartridge) {
    if (!cartridge) return NULL;
    sb_lua_music_t *c=calloc(1,sizeof(*c));
    if (c) { c->audio=audio; c->cartridge=cartridge; }
    return c;
}
void sb_lua_music_fail(sb_lua_music_t *c) { if (c) c->failed=true; }
void sb_lua_music_end(sb_lua_music_t *c) {
    if (c) { c->closed=true; sb_music_assets_reset(&c->silent); }
}
void sb_lua_music_destroy(sb_lua_music_t *c) { sb_lua_music_end(c); free(c); }
static sb_lua_music_t *music_context(lua_State *L) { return lua_touserdata(L,lua_upvalueindex(1)); }
static const char *music_session_reason(sb_lua_music_t *c) {
    if (!c || c->closed) return "session_closed";
    return c->failed ? "session_failed" : NULL;
}
static void music_arity(lua_State *L) {
    if (lua_gettop(L)!=1) luaL_error(L,"music: expected 1 argument");
}
/* Stack [argument, ...] -> [..., nil/false, reason]; return top two values. */
static int music_refuse(lua_State *L,bool boolean,const char *reason) {
    if (boolean) lua_pushboolean(L,0); else lua_pushnil(L);
    lua_pushstring(L,reason); return 2;
}
static music_handle_t *music_check(lua_State *L) {
    music_handle_t *h=luaL_testudata(L,1,MUSIC_MT);
    if (!h || lua_rawlen(L,1)!=sizeof(*h) || h->owner!=music_context(L) || !h->id)
        luaL_error(L,"music: invalid, foreign or unloaded song handle");
    return h;
}
static int music_code(lua_State *L,sb_music_asset_code_t code,bool boolean) {
    const char *reason;
    switch (code) {
        case SB_MUSIC_ASSET_PLAYER_BUSY: reason="music_busy"; break;
        case SB_MUSIC_ASSET_VOICE_BUSY: reason="voice_busy"; break;
        case SB_MUSIC_ASSET_LOOP_INELIGIBLE: reason="loop_ineligible"; break;
        case SB_MUSIC_ASSET_ARGUMENT: return luaL_error(L,"music: invalid playback options");
        case SB_MUSIC_ASSET_CAPACITY: reason="music_capacity"; break;
        case SB_MUSIC_ASSET_IN_USE: reason="music_in_use"; break;
        case SB_MUSIC_ASSET_EXHAUSTED: reason="music_id_exhausted"; break;
        case SB_MUSIC_ASSET_UNAVAILABLE: reason="audio_unavailable"; break;
        case SB_MUSIC_ASSET_BAD_HANDLE: return luaL_error(L,"music: invalid or unloaded song handle");
        default: reason="session_unavailable"; break;
    }
    return music_refuse(L,boolean,reason);
}
/* Stack [path] -> [path, userdata] on success, top userdata is the sole result.
 * Prepare Lua storage BEFORE acquiring native resources; after publication no
 * allocating Lua operation occurs. Lost Lua handles do not free native assets:
 * explicit unload or session teardown owns that lifetime (as with WAV assets). */
static int music_load(lua_State *L) {
    music_arity(L);
    if (lua_type(L,1)!=LUA_TSTRING) return luaL_error(L,"music.load: path must be a string");
    size_t length; const char *path=lua_tolstring(L,1,&length);
    if (!length || memchr(path,0,length)) return luaL_error(L,"music.load: invalid path");
    sb_lua_music_t *c=music_context(L);
    const char *reason=music_session_reason(c);
    if (reason) return music_refuse(L,false,reason);
    music_handle_t *h=lua_newuserdatauv(L,sizeof(*h),0);
    *h=(music_handle_t){c,0}; luaL_setmetatable(L,MUSIC_MT);
    sb_asset_result_t read=SB_ASSET_OK;
    sb_music_load_result_t detail={0};
    sb_music_asset_code_t code;
    if (c->audio) code=sb_audio_music_load_cartridge(c->audio,c->cartridge,path,&h->id,&read,&detail);
    else {
        uint8_t *bytes=NULL; size_t size=0;
        read=sb_asset_read_limited(c->cartridge,path,SB_MUSIC_LOAD_MAX_INPUT,&bytes,&size);
        code=SB_MUSIC_ASSET_READ_FAILED;
        if (read==SB_ASSET_OK) {
            sb_music_asset_t *asset=sb_music_load((const char *)bytes,size,&detail);
            code=asset ? sb_music_assets_insert(&c->silent,&asset,&h->id) : SB_MUSIC_ASSET_LOAD_FAILED;
            sb_music_asset_free(asset);
        }
        free(bytes);
    }
    /* All temporary native storage/locks are gone before Lua may raise. */
    if (code==SB_MUSIC_ASSET_OK) return 1;
    lua_pop(L,1);
    if (code==SB_MUSIC_ASSET_READ_FAILED) {
        if (read==SB_ASSET_INVALID_PATH) return luaL_error(L,"music.load: invalid cartridge-relative path");
        return music_refuse(L,false,sb_asset_error_string(read));
    }
    if (code==SB_MUSIC_ASSET_LOAD_FAILED) {
        if (detail.code==SB_MUSIC_LOAD_MEMORY) return music_refuse(L,false,"out_of_memory");
        lua_pushnil(L); lua_pushfstring(L,"music.load: %s",detail.message); return 2;
    }
    return music_code(L,code,false);
}
/* Stack [handle] -> [..., new metadata table], or [..., nil, reason]. No native
 * pointer survives into table construction; allocation failure changes nothing. */
static int music_info(lua_State *L) {
    music_arity(L); music_handle_t *h=music_check(L);
    sb_lua_music_t *c=music_context(L); const char *reason=music_session_reason(c);
    if (reason) return music_refuse(L,false,reason);
    sb_music_info_t info;
    sb_music_asset_code_t code=c->audio ? sb_audio_music_info(c->audio,h->id,&info) :
        (sb_music_asset_info(sb_music_assets_get(&c->silent,h->id),&info) ? SB_MUSIC_ASSET_OK : SB_MUSIC_ASSET_BAD_HANDLE);
    if (code!=SB_MUSIC_ASSET_OK) return music_code(L,code,false);
    lua_createtable(L,0,4);
    lua_pushinteger(L,info.track_count); lua_setfield(L,-2,"track_count");
    lua_pushinteger(L,(lua_Integer)info.end_frame); lua_setfield(L,-2,"end_frame");
    lua_pushinteger(L,info.timeline_rate); lua_setfield(L,-2,"timeline_rate");
    lua_pushboolean(L,info.loop_eligible); lua_setfield(L,-2,"loop_eligible");
    return 1;
}
/* Stack [handle] -> [..., true] or [..., false, reason]. Successful unload
 * invalidates all Lua references to this userdata; no allocation follows free. */
static int music_unload(lua_State *L) {
    music_arity(L); music_handle_t *h=music_check(L);
    sb_lua_music_t *c=music_context(L); const char *reason=music_session_reason(c);
    if (reason) return music_refuse(L,true,reason);
    sb_music_asset_code_t code=c->audio ? sb_audio_music_unload(c->audio,h->id) :
        sb_music_assets_unload(&c->silent,h->id,NULL);
    if (code!=SB_MUSIC_ASSET_OK) return music_code(L,code,true);
    h->id=0; lua_pushboolean(L,1); return 1;
}
static double music_volume_value(lua_State *L,int index) {
    double value=lua_tonumber(L,index);
    if (lua_type(L,index)!=LUA_TNUMBER || !isfinite(value) || value<0 || value>1)
        luaL_error(L,"music: volume must be a finite number in [0, 1]");
    return value;
}
static bool music_key(lua_State *L,int index,const char *name) {
    size_t n;
    if (lua_type(L,index)!=LUA_TSTRING) return false;
    const char *key=lua_tolstring(L,index,&n);
    return n==strlen(name) && !memcmp(key,name,n);
}
/* Raw table iteration: no inherited fields or metamethods. Each key is a
 * logical track, each value a distinct physical voice. Stack unchanged. */
static size_t music_voices(lua_State *L,int index,unsigned voices[SB_AUDIO_BANK_VOICES]) {
    if (lua_type(L,index)!=LUA_TTABLE) luaL_error(L,"music.play: voices must be a table");
    index=lua_absindex(L,index);
    unsigned tracks=0,physical=0; size_t count=0;
    lua_pushnil(L);
    while (lua_next(L,index)) {
        if (!lua_isinteger(L,-2) || !lua_isinteger(L,-1))
            luaL_error(L,"music.play: voice mapping needs integer keys and values");
        lua_Integer track=lua_tointeger(L,-2),voice=lua_tointeger(L,-1);
        if (track<1 || track>SB_AUDIO_BANK_VOICES || voice<1 || voice>SB_AUDIO_BANK_VOICES ||
            (physical & (1u<<((unsigned)voice-1))))
            luaL_error(L,"music.play: invalid or duplicate voice assignment");
        tracks|=1u<<((unsigned)track-1); physical|=1u<<((unsigned)voice-1);
        voices[(unsigned)track-1]=(unsigned)voice; ++count;
        lua_pop(L,1);
    }
    if (!count || tracks!=((1u<<(unsigned)count)-1))
        luaL_error(L,"music.play: voices must be a complete dense mapping");
    return count;
}
/* Stack [song, options] -> [..., true] or [..., false, reason]. All option and
 * handle validation precedes mutation. No Lua allocation after successful start. */
static int music_play(lua_State *L) {
    if (lua_gettop(L)!=2) return luaL_error(L,"music.play: expected song and options");
    music_handle_t *h=music_check(L);
    if (lua_type(L,2)!=LUA_TTABLE) return luaL_error(L,"music.play: options must be a table");
    unsigned voices[SB_AUDIO_BANK_VOICES]={0}; size_t count=0;
    bool loop=false; double volume=1;
    lua_pushnil(L);
    while (lua_next(L,2)) {
        if (music_key(L,-2,"voices")) count=music_voices(L,-1,voices);
        else if (music_key(L,-2,"volume")) volume=music_volume_value(L,-1);
        else if (music_key(L,-2,"loop")) {
            if (lua_type(L,-1)!=LUA_TBOOLEAN) return luaL_error(L,"music.play: loop must be boolean");
            loop=lua_toboolean(L,-1);
        } else return luaL_error(L,"music.play: unknown option");
        lua_pop(L,1);
    }
    if (!count) return luaL_error(L,"music.play: voices are required");
    sb_lua_music_t *c=music_context(L); const char *reason=music_session_reason(c);
    if (reason) return music_refuse(L,true,reason);
    sb_music_info_t info;
    sb_music_asset_code_t code=c->audio ? sb_audio_music_info(c->audio,h->id,&info) :
        (sb_music_asset_info(sb_music_assets_get(&c->silent,h->id),&info) ? SB_MUSIC_ASSET_OK : SB_MUSIC_ASSET_BAD_HANDLE);
    if (code!=SB_MUSIC_ASSET_OK) return music_code(L,code,true);
    if (count!=info.track_count) return luaL_error(L,"music.play: one voice is required per track");
    if (!c->audio) return music_refuse(L,true,"audio_unavailable");
    code=sb_audio_music_play_volume(c->audio,h->id,voices,count,loop,volume);
    if (code!=SB_MUSIC_ASSET_OK) return music_code(L,code,true);
    lua_pushboolean(L,1); return 1;
}
/* Stack [] -> [state]. Pausing does not add a playback state. Failed sessions
 * remain queryable while native cleanup finishes; closed/no-device is idle. */
static int music_state(lua_State *L) {
    if (lua_gettop(L)) return luaL_error(L,"music.state: expected no arguments");
    sb_lua_music_t *c=music_context(L);
    sb_music_state_t state=c && !c->closed ? sb_audio_music_state(c->audio) : SB_MUSIC_IDLE;
    lua_pushstring(L,state==SB_MUSIC_PLAYING ? "playing" : state==SB_MUSIC_FINISHING ? "finishing" : "idle");
    return 1;
}
/* Stack [] -> [true] or [false, reason]. Idle includes the no-device context. */
static int music_stop(lua_State *L) {
    if (lua_gettop(L)) return luaL_error(L,"music.stop: expected no arguments");
    sb_lua_music_t *c=music_context(L); const char *reason=music_session_reason(c);
    if (reason) return music_refuse(L,true,reason);
    if (c->audio && !sb_audio_music_stop(c->audio)) return music_refuse(L,true,"session_unavailable");
    lua_pushboolean(L,1); return 1;
}
/* Stack [volume] -> [..., true] or [..., false, reason]. Idle never stores gain. */
static int music_set_volume(lua_State *L) {
    music_arity(L); double volume=music_volume_value(L,1);
    sb_lua_music_t *c=music_context(L); const char *reason=music_session_reason(c);
    if (reason) return music_refuse(L,true,reason);
    if (!c->audio || sb_audio_music_state(c->audio)==SB_MUSIC_IDLE)
        return music_refuse(L,true,"music_idle");
    if (!sb_audio_music_gain(c->audio,volume))
        return music_refuse(L,true,"music_idle"); /* Callback may just have finished. */
    lua_pushboolean(L,1); return 1;
}
void sb_lua_music_register(lua_State *L,sb_lua_music_t *c) {
    luaL_newmetatable(L,MUSIC_MT);
    lua_pushliteral(L,"protected music handle"); lua_setfield(L,-2,"__metatable");
    lua_pop(L,1);
    static const luaL_Reg functions[]={{"load",music_load},{"info",music_info},{"unload",music_unload},
        {"play",music_play},{"state",music_state},{"stop",music_stop},{"set_volume",music_set_volume},{NULL,NULL}};
    lua_newtable(L); lua_pushlightuserdata(L,c); luaL_setfuncs(L,functions,1);
    lua_setfield(L,-2,"music");
}
