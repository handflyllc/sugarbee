/* Cartridge-local byte reads; no cache and no host filesystem interface. */
#ifndef SB_LUA_DATA_H
#define SB_LUA_DATA_H
#include <lua.h>
#define SB_DATA_MAX_FILE (1024u * 1024u)
#define SB_DATA_MAX_TOTAL (16u * 1024u * 1024u)
/* Stack: [..., sugarbee table] -> unchanged. Call once per fresh Lua session,
 * before main.lua. Copies cartridge path; private counter dies with Lua state.
 * Registration may raise a Lua allocation error; no native resource retained. */
void sb_lua_data_register(lua_State *L, const char *cartridge);
#endif
