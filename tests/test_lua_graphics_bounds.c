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
    const luaL_Reg bindings[]={{"line",l_sugarbee_line},{"rect",l_sugarbee_rect},{"text",l_sugarbee_print},
        {"sprite",l_sugarbee_sprite},{"region",l_sugarbee_sprite_region},
        {"tint",l_sugarbee_sprite_tint},{"tint_reset",l_sugarbee_sprite_tint_reset},
        {"camera",l_sugarbee_camera},{"camera_reset",l_sugarbee_camera_reset},
        {"clip",l_sugarbee_clip},{"clip_reset",l_sugarbee_clip_reset},
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
    CHECK(luaL_dostring(L,
        "assert(select('#',clip(2,3,4,5))==0); "
        "for _,args in ipairs({{0,0,-1,1},{0,0,1,-1},{0.5,0,1,1},"
        "{imax+1,0,1,1},{0,0,1,1,5},{0,0,1}}) do "
        "assert(not pcall(clip,table.unpack(args))) end; "
        "assert(not pcall(clip_reset,nil))") == LUA_OK);
    SDL_Renderer *renderer=SDL_GetRenderer(SDL_GetWindowFromID(1));
    SDL_Rect clip; SDL_RenderGetClipRect(renderer,&clip);
    CHECK(clip.x==2 && clip.y==3 && clip.w==4 && clip.h==5);
    CHECK(luaL_dostring(L,"assert(select('#',clip_reset())==0)")==LUA_OK);
    CHECK(!SDL_RenderIsClipEnabled(renderer));
    sb_graphics_clear(g_engine.graphics,0,0,0);
    CHECK(luaL_dostring(L,
        "assert(select('#',camera(10.0,-20.0))==0); "
        "for _,args in ipairs({{0},{0,0,0},{0.5,0},{0,imax+1},{imin-1,0},"
        "{0,math.huge},{false,0}}) do assert(not pcall(camera,table.unpack(args))) end; "
        "assert(not pcall(camera_reset,nil)); "
        "rect(10,-20,1,1,255,255,255,true)")==LUA_OK);
    Uint8 pixel[4]; SDL_Rect area={0,0,1,1};
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    CHECK(luaL_dostring(L,"assert(select('#',camera_reset())==0)")==LUA_OK);
    sb_graphics_clear(g_engine.graphics,0,0,0);
    CHECK(luaL_dostring(L,"rect(0,0,1,1,255,255,255,true)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255);
    sb_graphics_clear(g_engine.graphics,0,0,0);
    CHECK(luaL_dostring(L,
        "assert(select('#',tint(30.0,60.0,90.0))==0); "
        "local invalid={-1,256,0.5,math.huge,imax+1,false}; "
        "for i=1,4 do for _,v in ipairs(invalid) do "
        "local a={100,120,140,160}; a[i]=v; assert(not pcall(tint,table.unpack(a))) end end; "
        "assert(not pcall(tint,0,0)); assert(not pcall(tint,0,0,0,0,0)); "
        "assert(not pcall(tint,0,0,0,nil)); assert(not pcall(tint_reset,nil)); "
        "sprite(handle,0,0)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==30 && pixel[1]==60 && pixel[2]==90);
    CHECK(luaL_dostring(L,"assert(select('#',tint(255,255,255,0))==0); sprite(handle,0,0)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==30 && pixel[1]==60 && pixel[2]==90);
    CHECK(luaL_dostring(L,"tint(255,255,255); sprite(handle,0,0)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    CHECK(luaL_dostring(L,"rect(0,0,1,1,0,0,0,true); tint(0,0,0,0); assert(select('#',tint_reset())==0); sprite(handle,0,0)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    sb_graphics_clear(g_engine.graphics,0,0,0);
    CHECK(luaL_dostring(L,
        "for _,args in ipairs({{0,0,0},{0,0,0,0,0,0,0,0}}) do "
        "assert(not pcall(line,table.unpack(args))) end; "
        "for i=1,7 do local values=i<=4 and {imax+1,imin-1,0.5,math.huge,false} "
        "or {-1,256,0.5,math.huge,false}; for _,v in ipairs(values) do "
        "local a={0,0,0,0,100,120,140}; a[i]=v; "
        "assert(not pcall(line,table.unpack(a))) end end; "
        "assert(not pcall(line,0,0,0,0,nil))")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==0 && pixel[1]==0 && pixel[2]==0);
    CHECK(luaL_dostring(L,"tint(0,0,0,0); assert(select('#',line(0.0,0,0,0))==0)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    CHECK(luaL_dostring(L,"line(0,0,0,0,20,40,60)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==20 && pixel[1]==40 && pixel[2]==60);
    CHECK(luaL_dostring(L,"line(0,0,0,0,80)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==80 && pixel[1]==255 && pixel[2]==255);
    CHECK(luaL_dostring(L,"line(0,0,0,0,80,90)")==LUA_OK);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==80 && pixel[1]==90 && pixel[2]==255);
    sb_graphics_sprite_tint(g_engine.graphics,0,0,0,0);
    sb_graphics_camera(g_engine.graphics,INT_MAX,INT_MIN);
    CHECK(sb_graphics_clip(g_engine.graphics,0,0,0,0));
    trigger_guru_meditation(1,"Test","Expected clip isolation test",NULL);
    render_guru_meditation(); CHECK(!SDL_RenderIsClipEnabled(renderer));
    sb_graphics_rect(g_engine.graphics,0,0,1,1,255,255,255,true);
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    sb_graphics_clear(g_engine.graphics,0,0,0);
    CHECK(sb_graphics_sprite(g_engine.graphics,t,0,0));
    CHECK(SDL_RenderReadPixels(renderer,&area,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    CHECK(lua_gettop(L)==0);
    lua_close(L); sb_input_destroy(g_engine.input);
    sb_graphics_destroy(g_engine.graphics); SDL_Quit(); return 0;
}
