#ifndef SB_LUA_MUSIC_H
#define SB_LUA_MUSIC_H
#include "sb_audio.h"
#include <lua.h>
typedef struct sb_lua_music sb_lua_music_t;
/* Host-owned cartridge context; borrows device/path until destroy. Native audio
 * session owns device-backed assets; this owns the no-device collection only.
 * End after native session end and before lua_close; destroy after lua_close.
 * One serialized control thread; all lifecycle functions NULL-safe. */
sb_lua_music_t *sb_lua_music_create(sb_audio_t *audio,const char *cartridge);
void sb_lua_music_fail(sb_lua_music_t *context);
void sb_lua_music_end(sb_lua_music_t *context);
void sb_lua_music_destroy(sb_lua_music_t *context);
/* Stack [..., sugarbee table] -> unchanged. Context outlives closures. */
void sb_lua_music_register(lua_State *L,sb_lua_music_t *context);
#endif
