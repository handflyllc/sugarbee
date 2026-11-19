/* Contract cases plus rendered placement/state checks using production Lua. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int measure(const char *s,int scale,int w,int h) {
    int actual_w=-1,actual_h=-1;
    return sb_graphics_measure_text(s,strlen(s),scale,&actual_w,&actual_h) && actual_w==w && actual_h==h;
}
static int run(lua_State *L,const char *code) {
    int status=luaL_dostring(L,code);
    if(status!=LUA_OK) fprintf(stderr,"%s\n",lua_tostring(L,-1));
    return status==LUA_OK;
}
int main(void) {
    CHECK(measure("",1,0,0));
    CHECK(measure(" ",1,8,8));
    CHECK(measure(" A  ",2,64,16));
    CHECK(measure("abc\nx",3,72,54));
    CHECK(measure("\n",1,0,18));
    CHECK(measure("A\n",1,8,18));
    CHECK(measure("\nA\n\n",2,16,76));
    CHECK(measure("\t\r\001\177\303\251",1,48,8));
    CHECK(measure("abc\nxyz",0,0,0));
    CHECK(measure("abc",INT_MIN,0,0));
    CHECK(measure("",INT_MAX,0,0));
    CHECK(measure("x",INT_MAX/8,2147483640,2147483640));
    int w,h;
    CHECK(!sb_graphics_measure_text("x",1,INT_MAX/8+1,&w,&h) && w==0 && h==0);
    CHECK(!sb_graphics_measure_text("xx",2,INT_MAX/8,&w,&h) && w==0 && h==0);
    CHECK(!sb_graphics_measure_text("\n",1,INT_MAX/8,&w,&h) && w==0 && h==0);
    const char binary[]={'A',0,'B','\n'};
    CHECK(sb_graphics_measure_text(binary,sizeof(binary),1,&w,&h) && w==8 && h==8);
    const char span[]={'A','B'}; /* No trailing NUL; length is authoritative. */
    CHECK(sb_graphics_measure_text(span,2,1,&w,&h) && w==16 && h==8);
    char *large=malloc(SB_TEXT_MEASURE_MAX_BYTES+1);
    CHECK(large); memset(large,'x',SB_TEXT_MEASURE_MAX_BYTES+1);
    CHECK(sb_graphics_measure_text(large,SB_TEXT_MEASURE_MAX_BYTES,1,&w,&h) && w==2097152 && h==8);
    CHECK(!sb_graphics_measure_text(large,SB_TEXT_MEASURE_MAX_BYTES+1,1,&w,&h) && w==0 && h==0);
    memset(large,'\n',SB_TEXT_MEASURE_MAX_BYTES);
    CHECK(sb_graphics_measure_text(large,SB_TEXT_MEASURE_MAX_BYTES,1,&w,&h) && w==0 && h==2621448);
    free(large);
    CHECK(!sb_graphics_measure_text(NULL,0,1,&w,&h));
    CHECK(!sb_graphics_measure_text("x",1,1,NULL,&h));
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    g_engine.graphics=sb_graphics_create("Text metrics",64,48);
    CHECK(g_engine.graphics);
    lua_State *L=luaL_newstate(); CHECK(L);
    luaL_openlibs(L); register_sugarbee_api(L);
    CHECK(run(L,"local m=sugarbee.measure_text; local function check(s,z,w,h) "
        "local a,b=m(s,z); assert(a==w and b==h); assert(math.type(a)=='integer'); assert(select('#',m(s,z))==2) end; "
        "check('abc',nil,24,8); check('a\\nb',2.0,16,36); check('',1,0,0); check('x',0,0,0); check('x',-1,0,0); "
        "check('x'..string.char(0)..'y',1,8,8); check(123,1,24,8); assert(m('a')==8); "
        "assert(not pcall(m)); assert(not pcall(m,nil)); assert(not pcall(m,{})); assert(not pcall(m,true)); "
        "assert(not pcall(m,'x',1,2)); for _,v in ipairs({false,0.5,math.huge,2147483648,-2147483649,2147483647}) do "
        "assert(not pcall(m,'x',v)) end; assert(not pcall(m,string.rep('x',262145))); "
        "assert(not pcall(m,string.rep('x',262144),2147483647)); check('a',1,8,8)"));
    CHECK(lua_gettop(L)==0);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
    sb_graphics_clear(g_engine.graphics,13,17,19);
    sb_graphics_camera(g_engine.graphics,-100,200);
    CHECK(sb_graphics_clip(g_engine.graphics,2,3,4,5));
    sb_graphics_sprite_tint(g_engine.graphics,30,40,50,60);
    SDL_Rect before,after; SDL_RenderGetClipRect(r,&before);
    SDL_Texture *target=SDL_GetRenderTarget(r);
    CHECK(run(L,"local w,h=sugarbee.measure_text('outside viewport',3); assert(w==384 and h==24)"));
    SDL_RenderGetClipRect(r,&after);
    CHECK(before.x==after.x && before.y==after.y && before.w==after.w && before.h==after.h);
    CHECK(SDL_GetRenderTarget(r)==target);
    uint8_t pixels[64*48*4];
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,pixels,64*4)==0);
    for(size_t i=0;i<sizeof(pixels);i+=4) CHECK(pixels[i]==13 && pixels[i+1]==17 && pixels[i+2]==19);
    /* Camera/tint survive measurement: a white texture lands at (2,3), tinted. */
    const uint8_t white[]={255,255,255,255};
    sb_texture_t texture=sb_graphics_texture_rgba(g_engine.graphics,1,1,white,4);
    CHECK(texture && sb_graphics_sprite(g_engine.graphics,texture,-98,203));
    uint8_t p[4]; SDL_Rect area={2,3,1,1};
    CHECK(SDL_RenderReadPixels(r,&area,SDL_PIXELFORMAT_RGBA32,p,4)==0);
    CHECK(p[0]!=13 || p[1]!=17 || p[2]!=19);
    CHECK(p[0]!=255 && p[1]!=255 && p[2]!=255);
    /* Render within measured boxes on both lines. Every lit pixel must fit;
     * an F stem on each row also checks the 10-pixel newline advance. */
    CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
    CHECK(sb_graphics_measure_text("F \nF",4,2,&w,&h) && w==32 && h==36);
    sb_graphics_text(g_engine.graphics,"F \nF",4,4,255,255,255,2);
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,pixels,64*4)==0);
    int lit_first=0,lit_second=0;
    for(int y=0;y<48;++y) for(int x=0;x<64;++x) {
        size_t i=(size_t)(y*64+x)*4;
        if(pixels[i]) {
            CHECK(x>=4 && x<4+w && y>=4 && y<4+h);
            if(y<20) ++lit_first; else if(y>=24) ++lit_second; else CHECK(0);
        }
    }
    CHECK(lit_first>0 && lit_first==lit_second);
    CHECK(sb_graphics_show_game_frame(g_engine.graphics));
    CHECK(run(L,"local w,h=sugarbee.measure_text('F \\nF',2); assert(w==32 and h==36)"));
    lua_close(L); sb_graphics_destroy(g_engine.graphics); g_engine.graphics=NULL;
    SDL_Quit(); puts("text metrics checks pass"); return 0;
}
