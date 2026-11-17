/* Compare translated draws with equivalent screen-space draws, pixel for pixel. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Camera line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void draw(sb_graphics_t *g, sb_texture_t t, int x, int y) {
    sb_graphics_rect(g,x,y,20,14,80,120,160,true);
    sb_graphics_rect(g,x-3,y-2,28,20,255,255,255,false);
    sb_graphics_text(g,"F\nA",x,y,255,100,0,1);
    sb_graphics_sprite(g,t,x+15,y);
    sb_graphics_sprite_region(g,t,x+8,y+8,0,0,2,2,8,8,true,true);
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Camera",32,24); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    const uint8_t rgba[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
    sb_texture_t t=sb_graphics_texture_rgba(g,2,2,rgba,sizeof rgba); CHECK(t);
    uint8_t reference[32*24*4], actual[sizeof reference];
    CHECK(sb_graphics_begin_game_frame(g));
    CHECK(sb_graphics_clip(g,2,2,26,18));
    draw(g,t,1,1);
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,reference,32*4)==0);
    const int positions[][2]={{100,200},{-100,-200},{INT_MAX-100,INT_MAX-100},
                             {INT_MIN+100,INT_MIN+100}};
    for (unsigned i=0;i<sizeof positions/sizeof positions[0];++i) {
        int cx=positions[i][0], cy=positions[i][1];
        sb_graphics_camera(g,7,9); /* Next call replaces, never accumulates. */
        sb_graphics_camera(g,cx,cy);
        CHECK(sb_graphics_clip(g,2,2,26,18)); /* Also set clip after camera. */
        sb_graphics_clear(g,0,0,0); /* Clear preserves both states. */
        draw(g,t,cx+1,cy+1);
        CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,actual,32*4)==0);
        CHECK(memcmp(reference,actual,sizeof actual)==0);
    }
    CHECK(sb_graphics_clip_reset(g));
    for (int direction=0;direction<2;++direction) {
        int camera=direction ? INT_MIN : INT_MAX;
        int world=direction ? INT_MAX : INT_MIN;
        sb_graphics_camera(g,camera,camera);
        sb_graphics_clear(g,0,0,0);
        draw(g,t,world==INT_MAX ? world-30 : world+30,
                 world==INT_MAX ? world-30 : world+30);
        CHECK(sb_graphics_sprite_region(g,t,world,world,0,0,2,2,INT_MAX,INT_MAX,true,false));
        CHECK(!sb_graphics_sprite(g,SB_TEXTURE_INVALID,world,world));
        CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,actual,32*4)==0);
        for (unsigned i=0;i<sizeof actual;i+=4)
            CHECK(actual[i]==0 && actual[i+1]==0 && actual[i+2]==0);
    }
    /* A huge visible outline keeps its real edges, without inventing a
     * right/bottom border where the viewport ends. */
    sb_graphics_camera_reset(g);
    sb_graphics_clear(g,0,0,0);
    sb_graphics_rect(g,1,2,INT_MAX,INT_MAX,255,255,255,false);
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,actual,32*4)==0);
    for (int y=0;y<24;++y) for (int x=0;x<32;++x) {
        int expected=(x==1 && y>=2) || (y==2 && x>=1);
        CHECK(actual[(y*32+x)*4]==(expected ? 255 : 0));
    }
    /* Every lifecycle boundary restores screen coordinates. */
    for (int reset=0;reset<4;++reset) {
        sb_graphics_camera(g,INT_MAX,INT_MIN);
        if (reset==0) sb_graphics_camera_reset(g);
        if (reset==1) CHECK(sb_graphics_begin_game_frame(g));
        if (reset==2) CHECK(sb_graphics_show_game_frame(g));
        if (reset==3) sb_graphics_clear_cartridge_textures(g);
        sb_graphics_clear(g,0,0,0);
        sb_graphics_rect(g,0,0,32,24,255,255,255,true);
        CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,actual,32*4)==0);
        for (unsigned i=0;i<sizeof actual;++i) CHECK(actual[i]==255);
    }
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
