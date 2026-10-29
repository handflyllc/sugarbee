/* Each CTest invocation is isolated and externally timed out by CTest.
   Exercise real host Lua dispatch and cleanup without initializing SDL. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

static int events[8], count;
static int record(lua_State *L) {
    int n = (int)luaL_checkinteger(L, 1);
    if (count < 8) events[count] = n;
    ++count;
    printf("close event %d\n", n);
    fflush(stdout);
    return 0;
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    static const char *names[] = {
        "normal", "error", "runaway", "limit_then_runaway",
        "multiple_error", "multiple_runaway"
    };
    int test = 0;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(argv[1], names[i]) == 0) test = (int)i + 1;
    }
    if (!test) {
        fprintf(stderr, "Unknown __close case: %s\n", argv[1]);
        return 2;
    }
    const char *body = NULL;
    switch (test) {
    case 1: body = "local x <close> = setmetatable({}, {__close=function(_,err) assert(err==nil); record(1) end})"; break;
    case 2: body = "local x <close> = setmetatable({}, {__close=function(_,err) assert(tostring(err):find('original error')); record(1) end}); error('original error')"; break;
    case 3: body = "local x <close> = setmetatable({}, {__close=function() record(1); while true do end end})"; break;
    case 4: body = "local x <close> = setmetatable({}, {__close=function(_,err) assert(tostring(err):find('execution limit')); record(1); while true do end end}); while true do end"; break;
    case 5: body = "local a <close> = setmetatable({}, {__close=function() record(1) end}); local b <close> = setmetatable({}, {__close=function() record(2); error('close failure') end}); local c <close> = setmetatable({}, {__close=function() record(3) end})"; break;
    case 6: body = "local a <close> = setmetatable({}, {__close=function() record(1) end}); local b <close> = setmetatable({}, {__close=function() record(2); while true do end end}); local c <close> = setmetatable({}, {__close=function() record(3) end})"; break;
    default: return 2;
    }
    g_engine.L = luaL_newstate();
    if (!g_engine.L) return 2;
    open_cartridge_libraries(g_engine.L);
    lua_pushcfunction(g_engine.L, record);
    lua_setglobal(g_engine.L, "record");
    char source[2048];
    snprintf(source, sizeof(source), "function update(dt) %s end", body);
    if (luaL_dostring(g_engine.L, source) != LUA_OK) {
        fprintf(stderr, "Fixture setup failed: %s\n", lua_tostring(g_engine.L, -1));
        end_cartridge();
        return 2;
    }
    g_engine.cart_loaded = true;
    g_engine.cart_initialized = true;
    puts("calling real update dispatcher"); fflush(stdout);
    call_lua_global_function("update", true, 1.0/60.0);
    printf("returned: guru=%d limit=%d events=%d\n", g_engine.guru_active, g_engine.execution_limit_exceeded, count); fflush(stdout);
    bool valid = count == (test >= 5 ? 3 : 1);
    if (test >= 5) valid = valid && events[0] == 3 && events[1] == 2 && events[2] == 1;
    else valid = valid && events[0] == 1;
    bool limit = test == 3 || test == 4 || test == 6;
    valid = valid && g_engine.execution_limit_exceeded == limit && g_engine.guru_active == (test != 1);
    if (test == 2) valid = valid && strstr(g_engine.error_msg, "original error");
    if (test == 5) valid = valid && strstr(g_engine.error_msg, "close failure");
    if (limit) valid = valid && strstr(g_engine.error_msg, "execution limit exceeded in update()");
    valid = valid && lua_gettop(g_engine.L) == 0 && lua_gethook(g_engine.L) == NULL;
    int closed_count = count;
    end_cartridge();
    valid = valid && g_engine.L == NULL && !g_engine.cart_loaded &&
            !g_engine.cart_initialized && count == closed_count;
    puts("state cleanup returned"); fflush(stdout);
    /* Exercise a fresh healthy Lua session after cleanup, without SDL. */
    g_engine.guru_active = false;
    g_engine.L = luaL_newstate();
    if (!g_engine.L) return 2;
    open_cartridge_libraries(g_engine.L);
    if (luaL_dostring(g_engine.L, "function update() healthy = true end") != LUA_OK) return 2;
    call_lua_global_function("update", true, 1.0/60.0);
    lua_getglobal(g_engine.L, "healthy");
    valid = valid && lua_toboolean(g_engine.L, -1) && !g_engine.guru_active &&
            !g_engine.execution_limit_exceeded;
    lua_pop(g_engine.L, 1);
    end_cartridge();
    puts(valid ? "PASS: cleanup and fresh-session execution" : "FAIL: unexpected result");
    return valid ? 0 : 1;
}
