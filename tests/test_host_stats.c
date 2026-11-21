#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static bool pixel(SDL_Renderer *r,int red,int green,int blue) {
    SDL_Rect area={10,10,1,1}; Uint8 p[4];
    return SDL_RenderReadPixels(r,&area,SDL_PIXELFORMAT_RGBA32,p,4)==0 &&
        p[0]==red && p[1]==green && p[2]==blue;
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    CHECK(init_engine());
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1));
    sb_stats_t stats; sb_stats_reset(&stats);
    CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
    sb_graphics_clear(g_engine.graphics,200,30,40);
    CHECK(sb_graphics_show_game_frame(g_engine.graphics));
    draw_stats(&stats);
    CHECK(pixel(r,200,30,40)); /* Disabled does not draw. */
    g_engine.stats_enabled=true; g_engine.cart_loaded=true;
    CHECK(stats_gameplay());
    sb_graphics_camera(g_engine.graphics,100,100);
    CHECK(sb_graphics_clip(g_engine.graphics,900,500,1,1));
    draw_stats(&stats);
    CHECK(pixel(r,0,0,0) && !SDL_RenderIsClipEnabled(r));
    for(int i=0;i<60;++i) sb_stats_add(&stats,1.0/60,0.001,0.002,0.01,0);
    draw_stats(&stats); CHECK(pixel(r,0,0,0));
    g_engine.paused=true; CHECK(!stats_gameplay()); draw_stats(&stats);
    CHECK(sb_graphics_show_game_frame(g_engine.graphics));
    CHECK(pixel(r,200,30,40)); /* Overlay never enters retained game image. */
    g_engine.paused=false; g_engine.guru_active=true; CHECK(!stats_gameplay());
    draw_stats(&stats);
    g_engine.guru_active=false; g_engine.cart_loaded=false; CHECK(!stats_gameplay());
    draw_stats(&stats);
    /* Exercise every example setting with production bindings and Lua budgets. */
    g_engine.L=luaL_newstate(); CHECK(g_engine.L);
    open_cartridge_libraries(g_engine.L);
    register_sugarbee_api(g_engine.L);
    CHECK(luaL_dofile(g_engine.L,argv[1])==LUA_OK);
    CHECK(luaL_dostring(g_engine.L,
        "sugarbee.btnp=function(b) return b==test_button end")==LUA_OK);
    for (int mode=0;mode<2;++mode) for (int level=0;level<3;++level) {
        call_lua_global_function("init",false,0);
        if(mode) {
            lua_pushstring(g_engine.L,"b"); lua_setglobal(g_engine.L,"test_button");
            call_lua_global_function("update",true,1.0/60);
        }
        lua_pushstring(g_engine.L,"a"); lua_setglobal(g_engine.L,"test_button");
        for(int i=0;i<level;++i) call_lua_global_function("update",true,1.0/60);
        lua_pushnil(g_engine.L); lua_setglobal(g_engine.L,"test_button");
        call_lua_global_function("update",true,1.0/60);
        CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
        call_lua_global_function("draw",false,0);
        CHECK(!g_engine.guru_active);
    }
    shutdown_engine();
    return 0;
}
