#ifndef SB_LUA_AUDIO_H
#define SB_LUA_AUDIO_H
#include "sb_audio.h"
#include <lua.h>
typedef struct sb_lua_audio sb_lua_audio_t;
/* Host-owned per-cartridge context. Borrows device/path until destroy. The
 * host starts/ends the native audio session; this owns only silent fallback
 * assets. End before lua_close, destroy after lua_close. All NULL-safe. */
sb_lua_audio_t *sb_lua_audio_create(sb_audio_t *audio,const char *cartridge);
void sb_lua_audio_fail(sb_lua_audio_t *context);
void sb_lua_audio_end(sb_lua_audio_t *context);
void sb_lua_audio_destroy(sb_lua_audio_t *context);
/* Stack: [..., sugarbee table] -> unchanged. Closures borrow context. */
void sb_lua_audio_register(lua_State *L,sb_lua_audio_t *context);
#endif
