#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Opacity check line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int red(SDL_Renderer *r, int x, int y) {
    unsigned char p[4]; SDL_Rect area={x,y,1,1};
    if (SDL_RenderReadPixels(r,&area,SDL_PIXELFORMAT_RGBA32,p,4)) return -1;
    return p[0];
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Opacity",64,48); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    sb_graphics_clear(g,0,0,0);
    sb_graphics_rect_alpha(g,0,0,8,8,255,255,255,true,128);
    CHECK(red(r,2,2)>=127 && red(r,2,2)<=128);
    sb_graphics_rect_alpha(g,0,0,8,8,255,255,255,true,0);
    CHECK(red(r,2,2)>=127 && red(r,2,2)<=128);
    sb_graphics_rect(g,9,0,1,1,255,255,255,true); CHECK(red(r,9,0)==255);
    /* Ordinary source-over ordering: half red over opaque blue. */
    sb_graphics_clear(g,0,0,255);
    sb_graphics_rect_alpha(g,0,0,8,8,255,0,0,true,128);
    unsigned char pixel[4]; SDL_Rect sample={2,2,1,1};
    CHECK(SDL_RenderReadPixels(r,&sample,SDL_PIXELFORMAT_RGBA32,pixel,4)==0);
    CHECK(pixel[0]>=127 && pixel[0]<=128 && pixel[1]==0 && pixel[2]>=126 && pixel[2]<=127);
    /* Borders have uniform coverage, even for degenerate positive dimensions. */
    for(int w=1;w<=5;++w) for(int h=1;h<=5;++h) {
        sb_graphics_clear(g,0,0,0);
        sb_graphics_rect_alpha(g,2,2,w,h,255,255,255,false,128);
        for(int y=0;y<10;++y) for(int x=0;x<10;++x) {
            int edge=x>=2 && x<2+w && y>=2 && y<2+h &&
                (x==2 || x==1+w || y==2 || y==1+h);
            int v=red(r,x,y); CHECK(edge ? (v>=127 && v<=128) : v==0);
        }
    }
    sb_graphics_clear(g,0,0,0);
    sb_graphics_text_alpha(g,"F\nF",0,0,255,255,255,2,128);
    CHECK(red(r,2,10)>=127 && red(r,2,10)<=128);
    CHECK(red(r,2,30)>=127 && red(r,2,30)<=128);
    CHECK(red(r,12,10)==0);
    sb_graphics_text_alpha(g,"F",0,0,0,0,0,2,0);
    CHECK(red(r,2,10)>=127 && red(r,2,10)<=128);
    sb_graphics_text(g,"F",20,0,255,255,255,1); CHECK(red(r,21,5)==255);
    SDL_BlendMode mode; CHECK(SDL_GetRenderDrawBlendMode(r,&mode)==0);
    CHECK(mode==SDL_BLENDMODE_NONE);
    sb_graphics_clear(g,0,0,0);
    sb_graphics_camera(g,10,10); CHECK(sb_graphics_clip(g,2,2,2,2));
    sb_graphics_rect_alpha(g,10,10,8,8,255,255,255,true,128);
    CHECK(red(r,2,2)>=127 && red(r,2,2)<=128); CHECK(red(r,1,2)==0);
    CHECK(sb_graphics_clip_reset(g)); sb_graphics_camera_reset(g);
    /* Wide coordinates clip without wrapping or repeating coverage. */
    sb_graphics_clear(g,0,0,0);
    sb_graphics_camera(g,INT_MIN,INT_MIN);
    sb_graphics_rect_alpha(g,INT_MAX,INT_MAX,INT_MAX,INT_MAX,255,255,255,false,128);
    CHECK(red(r,0,0)==0); sb_graphics_camera_reset(g);
    sb_graphics_text_alpha(g,"F",-INT_MAX,0,255,255,255,INT_MAX,128);
    CHECK(red(r,0,0)>=127 && red(r,0,0)<=128);
    CHECK(sb_graphics_begin_host_frame(g));
    sb_graphics_rect(g,0,0,2,2,255,255,255,true); CHECK(red(r,0,0)==255);
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
