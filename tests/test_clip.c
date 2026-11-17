#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Clip line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int white(SDL_Renderer *r,int x,int y) {
    Uint8 p[4]; SDL_Rect area={x,y,1,1};
    if (SDL_RenderReadPixels(r,&area,SDL_PIXELFORMAT_RGBA32,p,4)) return -1;
    return p[0]==255 && p[1]==255 && p[2]==255;
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Clip",32,24); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    CHECK(sb_graphics_begin_game_frame(g));
    uint8_t bytes[4*4*4]; for (unsigned i=0;i<sizeof bytes;++i) bytes[i]=255;
    sb_texture_t t=sb_graphics_texture_rgba(g,4,4,bytes,sizeof bytes); CHECK(t);
    for (int mode=0;mode<4;++mode) {
        sb_graphics_clear(g,0,0,0); CHECK(sb_graphics_clip(g,3,3,2,2));
        if (mode==0) sb_graphics_rect(g,0,0,10,10,255,255,255,true);
        if (mode==1) CHECK(sb_graphics_sprite(g,t,2,2));
        if (mode==2) CHECK(sb_graphics_sprite_region(g,t,0,0,1,1,2,2,12,12,true,true));
        if (mode==3) sb_graphics_rect(g,3,3,2,2,255,255,255,false);
        for (int y=0;y<24;++y) for (int x=0;x<32;++x)
            CHECK(white(r,x,y)==(x>=3 && x<5 && y>=3 && y<5));
    }
    /* Text is clipped at a glyph's individual pixels. */
    sb_graphics_clear(g,0,0,0); CHECK(sb_graphics_clip(g,1,0,1,8));
    sb_graphics_text(g,"F",0,0,255,255,255,1);
    CHECK(white(r,1,5)==1); CHECK(white(r,2,0)==0);
    /* Replace rather than intersect; clear ignores but preserves the clip. */
    CHECK(sb_graphics_clip(g,20,10,2,2)); sb_graphics_clear(g,255,255,255);
    CHECK(white(r,0,0)==1); sb_graphics_clear(g,0,0,0);
    CHECK(!sb_graphics_clip(g,0,0,-1,1));
    sb_graphics_rect(g,0,0,32,24,255,255,255,true);
    CHECK(white(r,20,10)==1 && white(r,1,5)==0);
    const int areas[][4]={{0,0,0,10},{0,0,10,0},{INT_MAX,INT_MAX,INT_MAX,INT_MAX},
                         {INT_MIN,INT_MIN,INT_MAX,INT_MAX},{32,24,1,1}};
    for (unsigned i=0;i<sizeof areas/sizeof areas[0];++i) {
        CHECK(sb_graphics_clip(g,areas[i][0],areas[i][1],areas[i][2],areas[i][3]));
        sb_graphics_clear(g,0,0,0); sb_graphics_rect(g,0,0,32,24,255,255,255,true);
        for (int y=0;y<24;++y) for (int x=0;x<32;++x) CHECK(white(r,x,y)==0);
    }
    CHECK(sb_graphics_clip(g,-2,-2,4,4));
    sb_graphics_rect(g,0,0,32,24,255,255,255,true);
    CHECK(white(r,0,0)==1 && white(r,1,1)==1 && white(r,2,2)==0);
    CHECK(sb_graphics_clip_reset(g)); sb_graphics_rect(g,0,0,32,24,255,255,255,true);
    CHECK(white(r,31,23)==1);
    CHECK(sb_graphics_clip(g,0,0,0,0));
    CHECK(sb_graphics_show_game_frame(g)); CHECK(!SDL_RenderIsClipEnabled(r));
    CHECK(white(r,31,23)==1); /* Retained image is not clipped during copy. */
    sb_graphics_rect(g,0,0,32,24,0,0,0,true); CHECK(white(r,31,23)==0);
    CHECK(sb_graphics_begin_game_frame(g)); CHECK(!SDL_RenderIsClipEnabled(r));
    sb_graphics_rect(g,0,0,32,24,255,255,255,true); CHECK(white(r,31,23)==1);
    CHECK(sb_graphics_clip(g,0,0,0,0)); sb_graphics_clear_cartridge_textures(g);
    CHECK(!SDL_RenderIsClipEnabled(r)); CHECK(!sb_graphics_sprite(g,t,0,0));
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
