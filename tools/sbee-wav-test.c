/* Native diagnostic: synthesize PCM WAV containers, then use the public loader.
 * No files, Lua bindings, or external recordings are needed. */
#include "sb_audio.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void le16(unsigned char *b,unsigned v) { b[0]=(unsigned char)v; b[1]=(unsigned char)(v>>8); }
static void le32(unsigned char *b,uint32_t v) {
    for (unsigned i=0;i<4;++i) b[i]=(unsigned char)(v>>(8*i));
}
static sb_wav_result_t load_tone(sb_audio_t *a,unsigned rate,unsigned channels,sb_wav_handle_t *id) {
    unsigned frames=rate*2; size_t size=44+(size_t)frames*channels*2;
    unsigned char *b=calloc(1,size);
    if (!b) return SB_WAV_NO_MEMORY;
    memcpy(b,"RIFF",4); le32(b+4,(uint32_t)size-8); memcpy(b+8,"WAVEfmt ",8);
    le32(b+16,16); le16(b+20,1); le16(b+22,channels); le32(b+24,rate);
    le32(b+28,rate*channels*2); le16(b+32,channels*2); le16(b+34,16);
    memcpy(b+36,"data",4); le32(b+40,(uint32_t)size-44);
    for (unsigned i=0;i<frames;++i) for (unsigned ch=0;ch<channels;++ch) {
        double gain=fmin(1,fmin(i/(.01*rate),(frames-1-i)/(.01*rate)));
        double hz=channels==1 ? 440 : (ch==0 ? 330 : 660);
        int sample=(int)(26000*gain*sin(6.2831853071795864769*hz*i/rate));
        le16(b+44+((size_t)i*channels+ch)*2,(unsigned)sample);
    }
    sb_wav_result_t result=sb_audio_wav_load(a,b,size,id); free(b); return result;
}
#define REQUIRE(e) do { if (!(e)) { fprintf(stderr,"WAV diagnostic failed: %s\n",#e); sb_audio_destroy(a); return 1; } } while (0)
int main(void) {
    sb_audio_t *a=sb_audio_create(); if (!a) return 1;
    sb_audio_session_begin(a);
    sb_wav_handle_t mono,stereo,full,id,other;
    REQUIRE(load_tone(a,22050,1,&mono)==SB_WAV_OK);
    REQUIRE(load_tone(a,44100,2,&stereo)==SB_WAV_OK);
    REQUIRE(load_tone(a,48000,1,&full)==SB_WAV_OK);
    puts("22.05 kHz mono WAV: centered 440 Hz; natural finish.");
    REQUIRE(sb_audio_wav_play(a,mono,1,0,&id)==SB_WAV_OK);
    SDL_Delay(2150); REQUIRE(!sb_audio_wav_playing(a,id));
    puts("44.1 kHz stereo WAV: low left / high right, then left and right balance.");
    REQUIRE(sb_audio_wav_play(a,stereo,1,0,&id)==SB_WAV_OK);
    SDL_Delay(500); REQUIRE(sb_audio_wav_controls(a,id,1,-1));
    SDL_Delay(500); REQUIRE(sb_audio_wav_controls(a,id,1,1));
    SDL_Delay(500); REQUIRE(sb_audio_wav_stop(a,id)); SDL_Delay(100);
    puts("48 kHz mono WAV: pan left to right, quieter, then stop.");
    REQUIRE(sb_audio_wav_play(a,full,1,-1,&id)==SB_WAV_OK);
    SDL_Delay(400); REQUIRE(sb_audio_wav_controls(a,id,1,1));
    SDL_Delay(400); REQUIRE(sb_audio_wav_controls(a,id,.3,0));
    SDL_Delay(400); REQUIRE(sb_audio_wav_stop(a,id)); SDL_Delay(100);
    puts("Two overlapping WAVs with a 220 Hz synth; pause, resume, then failure silence.");
    REQUIRE(sb_audio_voice_configure(a,1,.01,0,1,.1));
    REQUIRE(sb_audio_voice_note_on(a,1,220));
    REQUIRE(sb_audio_wav_play(a,mono,.7,-1,&id)==SB_WAV_OK);
    REQUIRE(sb_audio_wav_play(a,stereo,.7,0,&other)==SB_WAV_OK);
    SDL_Delay(400); sb_audio_session_pause(a,true); SDL_Delay(700);
    sb_audio_session_pause(a,false); SDL_Delay(500);
    sb_audio_session_fail(a); SDL_Delay(300);
    REQUIRE(!sb_audio_wav_playing(a,id) && !sb_audio_wav_playing(a,other));
    sb_audio_session_end(a);
    sb_audio_session_begin(a);
    REQUIRE(sb_audio_wav_play(a,mono,1,0,&id)==SB_WAV_BAD_HANDLE);
    puts("Fresh session: short WAV, then paused shutdown remains silent.");
    REQUIRE(load_tone(a,48000,1,&full)==SB_WAV_OK);
    REQUIRE(sb_audio_wav_play(a,full,1,0,&id)==SB_WAV_OK);
    SDL_Delay(400); sb_audio_session_pause(a,true); sb_audio_session_prepare_shutdown(a);
    REQUIRE(sb_audio_wav_play(a,full,1,0,&other)==SB_WAV_OK);
    SDL_Delay(300); sb_audio_session_end(a);
    sb_audio_destroy(a);
    puts("WAV diagnostic complete; confirm playback and transitions by listening.");
    return 0;
}
