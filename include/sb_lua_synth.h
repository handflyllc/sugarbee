#ifndef SB_LUA_SYNTH_H
#define SB_LUA_SYNTH_H
#include <lua.h>
#include "sb_audio.h"
/* Stack: [..., sugarbee table] -> unchanged. Create a fresh per-Lua-state
 * configuration context. Borrow audio until this Lua state is closed; the host
 * owns device lifetime and begins the native session before registration. */
void sb_lua_synth_register(lua_State *L, sb_audio_t *audio);
#endif
