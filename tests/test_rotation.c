#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Rotation line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int red(SDL_Renderer *r,int x,int y) {
    unsigned char p[4]; SDL_Rect a={x,y,1,1};
    if(SDL_RenderReadPixels(r,&a,SDL_PIXELFORMAT_RGBA32,p,4)) return -1;
    return p[0];
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Rotation",64,48); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    const unsigned char rgba[]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    sb_texture_t tex=sb_graphics_texture_rgba(g,2,2,rgba,sizeof rgba); CHECK(tex);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,10,10,90,false,0,0));
    CHECK(red(r,11,10)==255); CHECK(red(r,10,10)==0);
    CHECK(red(r,11,11)==0); CHECK(red(r,10,11)==255);
    for (int quarter=0;quarter<4;++quarter) {
        const int xs[]={10,11,11,10}, ys[]={10,10,11,11};
        sb_graphics_clear(g,0,0,0);
        CHECK(sb_graphics_sprite_rotated(g,tex,10,10,quarter*90,false,0,0));
        unsigned char p[4]; SDL_Rect a={xs[quarter],ys[quarter],1,1};
        CHECK(SDL_RenderReadPixels(r,&a,SDL_PIXELFORMAT_RGBA32,p,4)==0);
        CHECK(p[0]==255 && p[1]==0 && p[2]==0);
    }
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,10,10,90,false,0,0));
    unsigned char first[64*48*4], next[sizeof first];
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,first,64*4)==0);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,10,10,-270,false,0,0));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,next,64*4)==0);
    CHECK(memcmp(first,next,sizeof first)==0);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,10,10,450,false,0,0));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,next,64*4)==0);
    CHECK(memcmp(first,next,sizeof first)==0);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite(g,tex,10,10));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,first,64*4)==0);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,10,10,360,true,0.5,-2.5));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,next,64*4)==0);
    CHECK(memcmp(first,next,sizeof first)==0);
    /* An external pivot rotates an entirely offscreen destination into view. */
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,-12,10,180,true,12,1));
    CHECK(red(r,10,10)==255);
    /* Region scaling, clip, tint and opacity use the rotated destination. */
    sb_graphics_clear(g,0,0,0);
    sb_graphics_sprite_tint(g,128,255,255,128);
    sb_graphics_camera(g,5,5); CHECK(sb_graphics_clip(g,11,10,2,2));
    CHECK(sb_graphics_sprite_region_rotated(g,tex,15,15,0,0,1,1,4,4,90,false,0,0));
    CHECK(red(r,11,10)>=63 && red(r,11,10)<=65); CHECK(red(r,10,10)==0);
    CHECK(sb_graphics_clip_reset(g)); sb_graphics_camera_reset(g);
    sb_graphics_sprite_tint_reset(g);
    CHECK(sb_graphics_sprite_rotated(g,tex,30,10,45.5,true,0.5,1.5));
    CHECK(!sb_graphics_sprite_rotated(g,tex,0,0,NAN,false,0,0));
    CHECK(!sb_graphics_sprite_rotated(g,tex,0,0,0,true,INFINITY,0));
    CHECK(!sb_graphics_sprite_region_rotated(g,tex,0,0,1,1,2,2,2,2,90,false,0,0));
    /* Oversized destinations are rejected before temporary rotation allocation. */
    CHECK(!sb_graphics_sprite_region_rotated(g,tex,INT_MIN,INT_MIN,0,0,2,2,INT_MAX,INT_MAX,45,true,INT_MAX,INT_MAX));
    CHECK(sb_graphics_sprite_rotated(g,tex,INT_MAX,INT_MIN,1e300,true,INT_MIN,INT_MAX));
    unsigned char odd_pixels[3*5*4]; memset(odd_pixels,255,sizeof odd_pixels);
    sb_texture_t odd=sb_graphics_texture_rgba(g,3,5,odd_pixels,sizeof odd_pixels); CHECK(odd);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,odd,10,10,37.5,false,0,0));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,first,64*4)==0);
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_sprite_rotated(g,odd,10,10,37.5,true,1.5,2.5));
    CHECK(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,next,64*4)==0);
    CHECK(memcmp(first,next,sizeof first)==0);
    sb_graphics_clear(g,0,0,0);
    sb_graphics_sprite_tint(g,255,255,255,0);
    CHECK(sb_graphics_sprite_rotated(g,tex,10,10,90,false,0,0)); CHECK(red(r,11,10)==0);
    sb_graphics_sprite_tint_reset(g);
    CHECK(sb_graphics_sprite(g,tex,10,10)); CHECK(red(r,10,10)==255);
    CHECK(sb_graphics_texture_destroy(g,odd));
    CHECK(sb_graphics_texture_destroy(g,tex));
    CHECK(!sb_graphics_sprite_rotated(g,tex,0,0,90,false,0,0));
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
