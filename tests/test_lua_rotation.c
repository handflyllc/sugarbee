#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Lua rotation line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv) {
    CHECK(argc==2);
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    g_engine.graphics=sb_graphics_create("Lua rotation",64,48); CHECK(g_engine.graphics);
    lua_State *L=luaL_newstate(); CHECK(L);
    open_cartridge_libraries(L); register_sugarbee_api(L);
    int status=luaL_dofile(L,argv[1]);
    if(status) fprintf(stderr,"%s\n",lua_tostring(L,-1));
    CHECK(status==LUA_OK);
    lua_getglobal(L,"init"); status=lua_pcall(L,0,0,0);
    if(status) fprintf(stderr,"%s\n",lua_tostring(L,-1));
    CHECK(status==LUA_OK);
    CHECK(lua_gettop(L)==0);
    sb_graphics_clear(g_engine.graphics,0,0,0);
    CHECK(luaL_dostring(L,
        "local t=sugarbee.texture_rgba(2,2,string.char(255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255)); "
        "sugarbee.sprite_rotated(t,10,10,90)")==LUA_OK);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1));
    unsigned char p[4]; SDL_Rect a={11,10,1,1};
    CHECK(SDL_RenderReadPixels(r,&a,SDL_PIXELFORMAT_RGBA32,p,4)==0);
    CHECK(p[0]==255 && p[1]==0 && p[2]==0);
    lua_close(L); sb_graphics_destroy(g_engine.graphics); SDL_Quit(); return 0;
}
