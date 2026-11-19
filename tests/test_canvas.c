/* Real renderer pixels and real cartridge startup/failure/relaunch paths. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s (%s)\n",__LINE__,#x,SDL_GetError()); return 1; } } while(0)
static int pixel(SDL_Renderer *r,int x,int y,int red,int green,int blue) {
    SDL_Rect rect={x,y,1,1}; Uint8 p[4];
    return SDL_RenderReadPixels(r,&rect,SDL_PIXELFORMAT_RGBA32,p,4)==0 &&
           p[0]==red && p[1]==green && p[2]==blue;
}
static int raw_pixels(SDL_Renderer *r) {
    return SDL_RenderSetLogicalSize(r,0,0)==0 && SDL_RenderSetViewport(r,NULL)==0 &&
           SDL_RenderSetScale(r,1,1)==0;
}
int main(int argc,char **argv) {
    CHECK(argc==2 && init_engine());
    SDL_Window *window=SDL_GetWindowFromID(1);
    SDL_Renderer *r=SDL_GetRenderer(window);
    const char *modes[]={"small","medium","wide","large","default","absent"};
    const int widths[]={300,320,640,960,1280,1280};
    const int heights[]={200,240,360,720,720,720};
    const int xs[]={190,160,0,160,0,0}, ys[]={60,0,0,0,0,0};
    const int scales[]={3,3,2,1,1,1};
    char path[4096];
    for(int i=0;i<6;++i) {
        snprintf(path,sizeof(path),"%s/%s.vcart",argv[1],modes[i]);
        CHECK(start_cartridge(path));
        CHECK(luaL_dostring(g_engine.L,"assert(select('#',sugarbee.width())==1); assert(select('#',sugarbee.height())==1); assert(not pcall(sugarbee.width,nil)); assert(not pcall(sugarbee.height,1))")==LUA_OK);
        call_lua_global_function("update",true,1.0/60);
        CHECK(!g_engine.guru_active);
        CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
        CHECK(sb_graphics_width(g_engine.graphics)==widths[i]);
        CHECK(sb_graphics_height(g_engine.graphics)==heights[i]);
        call_lua_global_function("draw",false,0);
        /* Distinct corner pixels verify exact canvas boundaries and nearest sampling. */
        sb_graphics_rect(g_engine.graphics,0,0,1,1,0,255,0,true);
        sb_graphics_rect(g_engine.graphics,widths[i]-1,heights[i]-1,1,1,0,0,255,true);
        CHECK(sb_graphics_show_game_frame(g_engine.graphics));
        CHECK(sb_graphics_width(g_engine.graphics)==1280);
        CHECK(sb_graphics_canvas_width(g_engine.graphics)==widths[i]);
        CHECK(raw_pixels(r));
        CHECK(pixel(r,xs[i],ys[i],0,255,0));
        CHECK(pixel(r,xs[i]+scales[i]-1,ys[i]+scales[i]-1,0,255,0));
        CHECK(pixel(r,xs[i]+scales[i],ys[i],200,0,0));
        CHECK(pixel(r,xs[i]+widths[i]*scales[i]-1,ys[i]+heights[i]*scales[i]-1,0,0,255));
        if(xs[i]) CHECK(pixel(r,xs[i]-1,360,0,0,0));
        if(ys[i]) CHECK(pixel(r,640,ys[i]-1,0,0,0));
        /* Host overlays must not enter the retained frame; repeated pause presentation. */
        CHECK(sb_graphics_begin_host_frame(g_engine.graphics));
        sb_graphics_rect(g_engine.graphics,600,350,80,20,255,255,255,true);
        CHECK(pixel(r,640,360,255,255,255));
        CHECK(sb_graphics_show_game_frame(g_engine.graphics));
        CHECK(pixel(r,640,360,200,0,0));
        sb_graphics_camera(g_engine.graphics,90,90);
        CHECK(sb_graphics_clip(g_engine.graphics,0,0,1,1));
        end_cartridge();
        CHECK(!g_engine.L && !g_engine.guru_active && !SDL_GetRenderTarget(r));
        CHECK(sb_graphics_canvas_width(g_engine.graphics)==1280);
        CHECK(!SDL_RenderIsClipEnabled(r));
    }
    /* Non-multiple output: integer 2x centered with odd leftover pixels. */
    SDL_SetWindowSize(window,1001,701);
    CHECK(sb_graphics_set_canvas(g_engine.graphics,320,240));
    CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
    sb_graphics_clear(g_engine.graphics,200,0,0);
    CHECK(sb_graphics_show_game_frame(g_engine.graphics) && raw_pixels(r));
    CHECK(pixel(r,180,110,200,0,0) && pixel(r,819,589,200,0,0));
    CHECK(pixel(r,179,110,0,0,0) && pixel(r,820,589,0,0,0));
    /* Too-small output: 300x200 -> 250x166, centered at (0,7). */
    SDL_SetWindowSize(window,250,180);
    CHECK(sb_graphics_set_canvas(g_engine.graphics,300,200));
    CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
    sb_graphics_clear(g_engine.graphics,200,0,0);
    CHECK(sb_graphics_show_game_frame(g_engine.graphics) && raw_pixels(r));
    CHECK(pixel(r,0,7,200,0,0) && pixel(r,249,172,200,0,0));
    CHECK(pixel(r,0,6,0,0,0) && pixel(r,249,173,0,0,0));
    /* Portrait output exercises height-constrained downscale. */
    SDL_SetWindowSize(window,180,100);
    CHECK(sb_graphics_show_game_frame(g_engine.graphics) && raw_pixels(r));
    CHECK(pixel(r,15,0,200,0,0) && pixel(r,164,99,200,0,0));
    CHECK(pixel(r,14,0,0,0,0) && pixel(r,165,99,0,0,0));
    SDL_SetWindowSize(window,1280,720);
    end_cartridge();
    const char *bad[]={"bad","oversized","init_error"};
    for(int i=0;i<3;++i) {
        snprintf(path,sizeof(path),"%s/%s.vcart",argv[1],bad[i]);
        CHECK(!start_cartridge(path) && g_engine.guru_active && !g_engine.L);
        CHECK(sb_graphics_width(g_engine.graphics)==1280 && !SDL_GetRenderTarget(r));
        render_guru_meditation();
        CHECK(sb_graphics_height(g_engine.graphics)==720);
        g_engine.guru_active=false;
    }
    snprintf(path,sizeof(path),"%s/draw_error.vcart",argv[1]);
    CHECK(start_cartridge(path) && sb_graphics_begin_game_frame(g_engine.graphics));
    call_lua_global_function("draw",false,0);
    CHECK(g_engine.guru_active && sb_graphics_show_game_frame(g_engine.graphics));
    render_guru_meditation(); end_cartridge(); g_engine.guru_active=false;
    snprintf(path,sizeof(path),"%s/absent.vcart",argv[1]);
    CHECK(start_cartridge(path)); /* Recovery to legacy default. */
    shutdown_engine(); puts("canvas checks pass"); return 0;
}
