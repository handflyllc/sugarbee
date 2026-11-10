#include "sb_lua_music.h"
#include "sb_audio_music.h"
#include <lauxlib.h>
#include <stdlib.h>
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
void sb_lua_music_register(lua_State *L,sb_lua_music_t *c) {
    luaL_newmetatable(L,MUSIC_MT);
    lua_pushliteral(L,"protected music handle"); lua_setfield(L,-2,"__metatable");
    lua_pop(L,1);
    static const luaL_Reg functions[]={{"load",music_load},{"info",music_info},{"unload",music_unload},{NULL,NULL}};
    lua_newtable(L); lua_pushlightuserdata(L,c); luaL_setfuncs(L,functions,1);
    lua_setfield(L,-2,"music");
}
