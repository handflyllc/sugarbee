#include "sb_lua_synth.h"
#include <lauxlib.h>
#include <math.h>
#include <string.h>

typedef struct {
    sb_audio_t *audio;
    sb_audio_voice_settings_t voices[9];
} synth_context_t;

static synth_context_t *context(lua_State *L) {
    return lua_touserdata(L, lua_upvalueindex(1));
}
static void arity(lua_State *L, int count) {
    if (lua_gettop(L) != count) luaL_error(L,"synth: expected %d arguments",count);
}
static unsigned voice_arg(lua_State *L) {
    if (lua_type(L,1)!=LUA_TNUMBER) luaL_error(L,"synth: voice must be an integer 1..9");
    int exact;
    lua_Integer voice = lua_tointegerx(L,1,&exact);
    if (!exact || voice<1 || voice>9) luaL_error(L,"synth: voice must be an integer 1..9");
    return (unsigned)voice;
}
static double number(lua_State *L, int index, unsigned voice,
                     const char *field, double minimum, double maximum) {
    double value = lua_tonumber(L,index);
    if (lua_type(L,index)!=LUA_TNUMBER || !isfinite(value) || value<minimum || value>maximum)
        luaL_error(L,"synth voice %d: %s must be a finite number in [%f, %f]",
                   (int)voice,field,minimum,maximum);
    return value;
}
/* Use length-aware comparisons: embedded NULs are not valid field names. */
static bool key_is(lua_State *L, int index, const char *name) {
    if (lua_type(L,index)!=LUA_TSTRING) return false;
    size_t size;
    const char *key = lua_tolstring(L,index,&size);
    return size==strlen(name) && !memcmp(key,name,size);
}
static void filter_fields(lua_State *L, unsigned voice, sb_audio_voice_settings_t *s) {
    if (lua_type(L,-1)!=LUA_TTABLE) luaL_error(L,"synth voice %d: filter must be a table",(int)voice);
    int table = lua_absindex(L,-1);
    lua_pushnil(L);
    /* [..., table, key, value]; lua_next/raw values never invoke metamethods. */
    while (lua_next(L,table)) {
        if (key_is(L,-2,"cutoff")) s->cutoff=number(L,-1,voice,"filter.cutoff",20,20000);
        else if (key_is(L,-2,"bypass")) {
            if (lua_type(L,-1)!=LUA_TBOOLEAN) luaL_error(L,"synth voice %d: filter.bypass must be boolean",(int)voice);
            s->bypass=lua_toboolean(L,-1);
        } else luaL_error(L,"synth voice %d: unknown filter field %s",(int)voice,
            lua_type(L,-2)==LUA_TSTRING ? lua_tostring(L,-2) : luaL_typename(L,-2));
        lua_pop(L,1);
    }
}
static int result(lua_State *L, bool ok) {
    lua_pushboolean(L,ok);
    if (ok) return 1;
    lua_pushstring(L,context(L)->audio ? "audio_rejected" : "audio_unavailable");
    return 2;
}
static int configure(lua_State *L) {
    arity(L,2);
    unsigned voice=voice_arg(L);
    if (lua_type(L,2)!=LUA_TTABLE) return luaL_error(L,"synth voice %d: configuration must be a table",(int)voice);
    synth_context_t *ctx=context(L);
    sb_audio_voice_settings_t s=ctx->voices[voice-1];
    lua_pushnil(L);
    while (lua_next(L,2)) {
        if (key_is(L,-2,"waveform")) {
            if (key_is(L,-1,"triangle")) s.waveform=SB_AUDIO_VOICE_TRIANGLE;
            else if (key_is(L,-1,"saw")) s.waveform=SB_AUDIO_VOICE_SAW;
            else if (key_is(L,-1,"pulse")) s.waveform=SB_AUDIO_VOICE_PULSE;
            else if (key_is(L,-1,"noise")) s.waveform=SB_AUDIO_VOICE_NOISE;
            else return luaL_error(L,"synth voice %d: waveform must be triangle, saw, pulse, or noise",(int)voice);
        } else if (key_is(L,-2,"attack")) s.attack=number(L,-1,voice,"attack",0,10);
        else if (key_is(L,-2,"decay")) s.decay=number(L,-1,voice,"decay",0,10);
        else if (key_is(L,-2,"sustain")) s.sustain=number(L,-1,voice,"sustain",0,1);
        else if (key_is(L,-2,"release")) s.release=number(L,-1,voice,"release",0,10);
        else if (key_is(L,-2,"volume")) s.volume=number(L,-1,voice,"volume",0,1);
        else if (key_is(L,-2,"pan")) s.pan=number(L,-1,voice,"pan",-1,1);
        else if (key_is(L,-2,"pulse_width")) s.pulse_width=number(L,-1,voice,"pulse_width",.05,.95);
        else if (key_is(L,-2,"filter")) filter_fields(L,voice,&s);
        else return luaL_error(L,"synth voice %d: unknown configuration field %s",(int)voice,
            lua_type(L,-2)==LUA_TSTRING ? lua_tostring(L,-2) : luaL_typename(L,-2));
        lua_pop(L,1);
    }
    /* All Lua validation precedes the native lock. Keep configurations usable
     * without a device, so the same cartridge can run silently. */
    bool ok=!ctx->audio || sb_audio_voice_apply_settings(ctx->audio,voice,&s);
    if (ok) ctx->voices[voice-1]=s;
    return result(L,ok);
}
static int note_on(lua_State *L) {
    unsigned voice=voice_arg(L);
    synth_context_t *ctx=context(L);
    bool noise=ctx->voices[voice-1].waveform==SB_AUDIO_VOICE_NOISE;
    arity(L,noise ? 1 : 2);
    double frequency=noise ? 0 : number(L,2,voice,"frequency",110,880);
    return result(L,noise ? sb_audio_voice_start_selected_noise(ctx->audio,voice) :
                  sb_audio_voice_start_selected(ctx->audio,voice,frequency));
}
static int note_off(lua_State *L) {
    arity(L,1);
    return result(L,sb_audio_voice_note_off(context(L)->audio,voice_arg(L)));
}
static int stop(lua_State *L) {
    arity(L,1);
    return result(L,sb_audio_voice_stop(context(L)->audio,voice_arg(L)));
}
void sb_lua_synth_register(lua_State *L, sb_audio_t *audio) {
    static const luaL_Reg functions[]={{"configure",configure},{"note_on",note_on},
        {"note_off",note_off},{"stop",stop},{NULL,NULL}};
    lua_newtable(L);
    synth_context_t *ctx=lua_newuserdatauv(L,sizeof(*ctx),0);
    ctx->audio=audio;
    for (unsigned i=0;i<9;++i) ctx->voices[i]=sb_audio_voice_defaults();
    /* [sugarbee, synth, context] -> [sugarbee, synth]; each closure owns context. */
    luaL_setfuncs(L,functions,1);
    lua_setfield(L,-2,"synth");
}
