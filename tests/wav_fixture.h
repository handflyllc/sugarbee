#ifndef WAV_FIXTURE_H
#define WAV_FIXTURE_H
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static void put16(unsigned char *b,unsigned v) { b[0]=(unsigned char)v; b[1]=(unsigned char)(v>>8); }
static void put32(unsigned char *b,uint32_t v) {
    for (unsigned i=0;i<4;++i) b[i]=(unsigned char)(v>>(8*i));
}
static unsigned char *fixture(unsigned rate,unsigned channels,size_t frames,size_t *length) {
    *length=44+frames*channels*2;
    unsigned char *b=calloc(1,*length);
    if (!b) return NULL;
    memcpy(b,"RIFF",4); put32(b+4,(uint32_t)*length-8); memcpy(b+8,"WAVEfmt ",8);
    put32(b+16,16); put16(b+20,1); put16(b+22,channels); put32(b+24,rate);
    put32(b+28,rate*channels*2); put16(b+32,channels*2); put16(b+34,16);
    memcpy(b+36,"data",4); put32(b+40,(uint32_t)(frames*channels*2));
    for (size_t i=0;i<frames*channels;++i) put16(b+44+i*2,32767);
    return b;
}
#endif
