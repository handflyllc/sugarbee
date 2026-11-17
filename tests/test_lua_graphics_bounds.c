/* Exercise production bindings, including validation before native mutation. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Bounds check failed: %s\n",#x); return 1; } } while (0)
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)==0);
    g_engine.graphics=sb_graphics_create("Bounds",64,48);
    g_engine.input=sb_input_create();
    CHECK(g_engine.graphics && g_engine.input);
    lua_State *L=luaL_newstate(); CHECK(L);
    open_cartridge_libraries(L);
    const luaL_Reg bindings[]={{"rect",l_sugarbee_rect},{"text",l_sugarbee_print},
        {"sprite",l_sugarbee_sprite},{"region",l_sugarbee_sprite_region},
        {"btn",l_sugarbee_btn},{"btnp",l_sugarbee_btnp},{NULL,NULL}};
    for (const luaL_Reg *b=bindings;b->name;++b) {
        lua_pushcfunction(L,b->func); lua_setglobal(L,b->name);
    }
    const uint8_t rgba[]={255,255,255,255};
    sb_texture_t t=sb_graphics_texture_rgba(g_engine.graphics,1,1,rgba,sizeof rgba);
    CHECK(t); lua_pushinteger(L,t); lua_setglobal(L,"handle");
    lua_pushinteger(L,INT_MAX); lua_setglobal(L,"imax");
    lua_pushinteger(L,INT_MIN); lua_setglobal(L,"imin");
    SDL_Event event={0}; event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_LEFT;
    CHECK(SDL_PushEvent(&event)==1); CHECK(sb_input_poll(g_engine.input));
    sb_input_begin_update(g_engine.input);
    const char *script=
        "local function bad(f, args, index, value) "
        "local copy={table.unpack(args)}; copy[index]=value; "
        "assert(not pcall(f,table.unpack(copy))) end "
        "local cases={{rect,{0,0,1,1},1,4},{text,{'A',0,0,255,255,255,1},2,3},"
        "{text,{'A',0,0,255,255,255,1},7,7},{sprite,{handle,0,0},2,3},"
        "{region,{handle,0,0,0,0,1,1,1,1},2,9}} "
        "for _,c in ipairs(cases) do for i=c[3],c[4] do "
        "for _,v in ipairs({imax+1,imin-1,4294967296,-4294967296,0.5,math.huge}) do "
        "bad(c[1],c[2],i,v) end end end "
        "assert(btn('left')); assert(btnp('left',0.0)); "
        "for _,v in ipairs({-1,4,imax+1,imin-1,4294967296,-4294967296}) do "
        "assert(not btn('left',v)); assert(not btnp('left',v)) end "
        "rect(-1.0,-1.0,2.0,2.0); sprite(handle,-1.0,-1.0); "
        "text('A',imax,imin); text('A\\nB',imin,imin,255,255,255,imax); "
        "text('A',0,0,255,255,255,0); text('A',0,0,255,255,255,imin)";
    int status=luaL_dostring(L,script);
    if (status) fprintf(stderr,"%s\n",lua_tostring(L,-1));
    CHECK(status==LUA_OK); CHECK(lua_gettop(L)==0);
    lua_close(L); sb_input_destroy(g_engine.input);
    sb_graphics_destroy(g_engine.graphics); SDL_Quit(); return 0;
}
