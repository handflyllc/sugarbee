#ifndef MUSIC_ASSET_FIXTURE_H
#define MUSIC_ASSET_FIXTURE_H
#include <stdio.h>
#include <stddef.h>
/* Generated paired events let tests reach storage limits with valid music. */
static size_t music_json(char *out,size_t capacity,unsigned events) {
    int n=snprintf(out,capacity,
        "{\"format\":\"sugarbee.music\",\"version\":1,\"timeline_rate\":48000,"
        "\"end_frame\":100000,\"tracks\":[{\"instrument\":{\"waveform\":\"triangle\","
        "\"attack\":0,\"decay\":0,\"sustain\":1,\"release\":0.01,\"volume\":0.5,"
        "\"pan\":0,\"filter\":{\"cutoff\":2000,\"bypass\":true}}}],\"events\":[");
    if (n<0 || (size_t)n>=capacity) return 0;
    size_t used=(size_t)n;
    for (unsigned i=0;i<events;++i) {
        n=snprintf(out+used,capacity-used,"%s{\"frame\":%u,\"track\":1,\"type\":\"%s\"%s}",
            i?",":"",i*20,i%2?"note_off":"note_on",i%2?"":",\"hz\":440");
        if (n<0 || (size_t)n>=capacity-used) return 0;
        used+=(size_t)n;
    }
    if (capacity-used<3) return 0;
    out[used++]=']'; out[used++]='}'; out[used]=0;
    return used;
}
#endif
