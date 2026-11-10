#include "sb_music_host.h"
#include <string.h>
void sb_music_host_reset(sb_music_host_t *h) {
    if (!h) return;
    memset(h,0,sizeof(*h));
    sb_music_transport_init(&h->transport);
}
bool sb_music_host_start(sb_music_host_t *h,const sb_music_song_t *song,bool loop) {
    if (!h) return false;
    bool ok=loop ? sb_music_sequence_start_loop(&h->sequence,song) :
                   sb_music_sequence_start(&h->sequence,song);
    if (ok) sb_music_transport_started(&h->transport);
    return ok;
}
bool sb_music_host_pause(sb_music_host_t *h,bool paused) {
    return h && sb_music_transport_pause(&h->transport,paused);
}
bool sb_music_host_stop(sb_music_host_t *h) {
    return h && sb_music_transport_stop(&h->transport,&h->sequence);
}
bool sb_music_host_render(sb_music_host_t *h,float *output,size_t frames) {
    return h && sb_music_transport_render(&h->transport,&h->sequence,output,frames);
}
