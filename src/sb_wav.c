#include "sb_wav_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint16_t u16(const unsigned char *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }
static uint32_t u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static double pcm(const unsigned char *p) {
    uint16_t raw=u16(p);
    return (raw>=32768 ? (int32_t)raw-65536 : (int32_t)raw)/32768.0;
}
void sb_wav_free(sb_wav_data_t *d) {
    if (d) { free(d->samples); *d=(sb_wav_data_t){0}; }
}
sb_wav_result_t sb_wav_decode(const void *bytes, size_t length, sb_wav_data_t *out) {
    if (!out || out->samples || out->frames || out->channels || !bytes) return SB_WAV_INVALID;
    if (length>SB_WAV_SOURCE_LIMIT) return SB_WAV_TOO_LARGE;
    if (length<12) return SB_WAV_INVALID;
    const unsigned char *b=bytes, *data=NULL;
    if (memcmp(b,"RIFF",4) || memcmp(b+8,"WAVE",4)) return SB_WAV_UNSUPPORTED;
    if ((uint64_t)u32(b+4)+8!=length) return SB_WAV_INVALID;
    unsigned channels=0, rate=0, chunks=0;
    size_t data_bytes=0;
    bool format=false;
    for (size_t pos=12;pos<length;) {
        if (++chunks>1024 || length-pos<8) return SB_WAV_INVALID;
        const unsigned char *tag=b+pos;
        size_t size=u32(b+pos+4); pos+=8;
        if (size>length-pos || (size&1u)>length-pos-size) return SB_WAV_INVALID;
        if (!memcmp(tag,"fmt ",4)) {
            if (format || size<16) return SB_WAV_INVALID;
            if (u16(b+pos)!=1 || u16(b+pos+14)!=16) return SB_WAV_UNSUPPORTED;
            if (!(size==16 || (size==18 && u16(b+pos+16)==0))) return SB_WAV_UNSUPPORTED;
            channels=u16(b+pos+2); rate=u32(b+pos+4);
            if ((channels!=1 && channels!=2) || (rate!=22050 && rate!=44100 && rate!=48000)) return SB_WAV_UNSUPPORTED;
            if (u16(b+pos+12)!=channels*2 || u32(b+pos+8)!=rate*channels*2) return SB_WAV_INVALID;
            format=true;
        } else if (!memcmp(tag,"data",4)) {
            if (!format || data || !size || size%(channels*2)) return SB_WAV_INVALID;
            data=b+pos; data_bytes=size;
        }
        pos+=size+(size&1u);
    }
    if (!format || !data) return SB_WAV_INVALID;
    size_t source_frames=data_bytes/(channels*2);
    uint64_t frames=((uint64_t)source_frames*SB_WAV_RATE+rate-1)/rate;
    if (frames>SB_WAV_ASSET_LIMIT/(channels*sizeof(float))) return SB_WAV_TOO_LARGE;
    float *samples=malloc((size_t)frames*channels*sizeof(float));
    if (!samples) return SB_WAV_NO_MEMORY;
    /* Linear interpolation, last source frame held at the right boundary.
     * Integer phase arithmetic avoids cumulative resampling drift. No extra
     * conversion buffer; interpolation stays within the PCM source bounds. */
    for (size_t i=0;i<(size_t)frames;++i) {
        uint64_t phase=(uint64_t)i*rate;
        size_t first=(size_t)(phase/SB_WAV_RATE);
        size_t next=first+1<source_frames ? first+1 : first;
        double fraction=(double)(phase%SB_WAV_RATE)/SB_WAV_RATE;
        for (unsigned ch=0;ch<channels;++ch) {
            double a=pcm(data+(first*channels+ch)*2);
            double z=pcm(data+(next*channels+ch)*2);
            samples[i*channels+ch]=(float)(a+(z-a)*fraction);
        }
    }
    *out=(sb_wav_data_t){samples,(size_t)frames,channels};
    return SB_WAV_OK;
}
const char *sb_wav_result_string(sb_wav_result_t r) {
    static const char *const names[]={"ok","invalid_input","unsupported_wav","wav_too_large",
        "out_of_memory","audio_budget","assets_full","voices_full","invalid_handle",
        "asset_in_use","audio_unavailable","audio_rejected","handles_exhausted"};
    return (unsigned)r<sizeof names/sizeof *names ? names[r] : "unknown_error";
}
void sb_wav_stop_all(sb_wav_pool_t *p) { memset(p->playbacks,0,sizeof p->playbacks); }
void sb_wav_pool_reset(sb_wav_pool_t *p) {
    sb_wav_stop_all(p);
    for (unsigned i=0;i<SB_WAV_ASSETS;++i) { sb_wav_free(&p->assets[i].data); p->assets[i].id=0; }
    p->bytes=0;
}
static size_t data_size(const sb_wav_data_t *d) { return d->frames*d->channels*sizeof(float); }
sb_wav_result_t sb_wav_insert(sb_wav_pool_t *p, sb_wav_data_t *d, sb_wav_handle_t *id) {
    *id=0;
    if (data_size(d)>SB_WAV_TOTAL_LIMIT-p->bytes) return SB_WAV_BUDGET;
    unsigned slot=0;
    while (slot<SB_WAV_ASSETS && p->assets[slot].id) ++slot;
    if (slot==SB_WAV_ASSETS) return SB_WAV_ASSETS_FULL;
    if (p->serial==UINT64_MAX) return SB_WAV_IDS_EXHAUSTED;
    p->assets[slot]=(sb_wav_asset_t){++p->serial,*d};
    p->bytes+=data_size(d); *d=(sb_wav_data_t){0};
    *id=p->assets[slot].id;
    return SB_WAV_OK;
}
sb_wav_result_t sb_wav_unload(sb_wav_pool_t *p, sb_wav_handle_t id) {
    if (!id) return SB_WAV_BAD_HANDLE;
    for (unsigned i=0;i<SB_WAV_ASSETS;++i) if (p->assets[i].id==id) {
        for (unsigned v=0;v<SB_WAV_PLAYBACKS;++v)
            if (p->playbacks[v].active && p->playbacks[v].asset==i) return SB_WAV_IN_USE;
        p->bytes-=data_size(&p->assets[i].data);
        sb_wav_free(&p->assets[i].data); p->assets[i].id=0;
        return SB_WAV_OK;
    }
    return SB_WAV_BAD_HANDLE;
}
static bool valid_controls(double volume,double pan) {
    return isfinite(volume) && volume>=0 && volume<=1 && isfinite(pan) && pan>=-1 && pan<=1;
}
static void gains(unsigned channels,double volume,double pan,double out[2]) {
    if (channels==1) {
        double angle=(pan+1)*0.7853981633974483096;
        out[0]=pan==1 ? 0 : volume*cos(angle);
        out[1]=pan==-1 ? 0 : volume*sin(angle);
    } else { out[0]=volume*(pan>0 ? 1-pan : 1); out[1]=volume*(pan<0 ? 1+pan : 1); }
}
sb_wav_result_t sb_wav_play(sb_wav_pool_t *p,sb_wav_handle_t asset,double volume,double pan,bool loop,sb_wav_handle_t *id) {
    *id=0;
    if (!valid_controls(volume,pan)) return SB_WAV_INVALID;
    unsigned a=0;
    while (a<SB_WAV_ASSETS && (!asset || p->assets[a].id!=asset)) ++a;
    if (a==SB_WAV_ASSETS) return SB_WAV_BAD_HANDLE;
    unsigned v=0;
    while (v<SB_WAV_PLAYBACKS && p->playbacks[v].active) ++v;
    if (v==SB_WAV_PLAYBACKS) return SB_WAV_VOICES_FULL;
    if (p->serial==UINT64_MAX) return SB_WAV_IDS_EXHAUSTED;
    sb_wav_playback_t next={0};
    next.id=++p->serial; next.asset=a; next.active=true;
    next.volume=volume; next.pan=pan; next.loop=loop;
    gains(p->assets[a].data.channels,volume,pan,next.gain);
    memcpy(next.target,next.gain,sizeof next.gain);
    p->playbacks[v]=next; *id=next.id;
    return SB_WAV_OK;
}
static sb_wav_playback_t *playback(sb_wav_pool_t *p,sb_wav_handle_t id) {
    for (unsigned v=0;v<SB_WAV_PLAYBACKS;++v)
        if (id && p->playbacks[v].id==id && p->playbacks[v].active) return &p->playbacks[v];
    return NULL;
}
bool sb_wav_playing(const sb_wav_pool_t *p,sb_wav_handle_t id) {
    for (unsigned v=0;v<SB_WAV_PLAYBACKS;++v)
        if (id && p->playbacks[v].id==id && p->playbacks[v].active) return true;
    return false;
}
bool sb_wav_controls(sb_wav_pool_t *p,sb_wav_handle_t id,double volume,double pan) {
    sb_wav_playback_t *v=playback(p,id);
    if (!v || v->stopping || !valid_controls(volume,pan)) return false;
    if (v->volume==volume && v->pan==pan) return true;
    v->volume=volume; v->pan=pan;
    memcpy(v->from,v->gain,sizeof v->gain);
    gains(p->assets[v->asset].data.channels,volume,pan,v->target); v->ramp=480;
    return true;
}
bool sb_wav_stop(sb_wav_pool_t *p,sb_wav_handle_t id) {
    sb_wav_playback_t *v=playback(p,id);
    if (!v || v->stopping) return false;
    v->stopping=true; v->ramp=480;
    memcpy(v->from,v->gain,sizeof v->gain); v->target[0]=v->target[1]=0;
    return true;
}
void sb_wav_render(sb_wav_pool_t *p,float *out,size_t frames) {
    for (size_t i=0;i<frames;++i) {
        double sum[2]={0};
        for (unsigned slot=0;slot<SB_WAV_PLAYBACKS;++slot) {
            sb_wav_playback_t *v=&p->playbacks[slot];
            if (!v->active) continue;
            const sb_wav_data_t *d=&p->assets[v->asset].data;
            for (unsigned ch=0;ch<2;++ch)
                sum[ch]+=d->samples[v->position*d->channels+(d->channels==1 ? 0 : ch)]*v->gain[ch];
            if (v->ramp) {
                --v->ramp;
                for (unsigned ch=0;ch<2;++ch)
                    v->gain[ch]=v->ramp ? v->from[ch]+(v->target[ch]-v->from[ch])*(480-v->ramp)/480 : v->target[ch];
                if (!v->ramp && v->stopping) v->active=false;
            }
            if (++v->position==d->frames) {
                /* Wrap while fading too; never reactivate a finished stop. */
                if (v->loop) v->position=0;
                else v->active=false;
            }
        }
        out[2*i]=(float)(sum[0]*.025); out[2*i+1]=(float)(sum[1]*.025);
    }
}
