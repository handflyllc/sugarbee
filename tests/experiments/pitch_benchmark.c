/* Offline session render workload. No SDL device; compile via pitch_benchmark.py. */
#define _POSIX_C_SOURCE 200809L
#include "sb_music_transport.h"
#include "sb_wav_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"benchmark line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int measuring;
static unsigned allocations;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n) { if (measuring) ++allocations; return __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t m) { if (measuring) ++allocations; return __real_calloc(n,m); }
void *__wrap_realloc(void *p,size_t n) { if (measuring) ++allocations; return __real_realloc(p,n); }
void __wrap_free(void *p) { if (measuring) ++allocations; __real_free(p); }
static double now(void) {
    struct timespec ts;
    CHECK(clock_gettime(CLOCK_MONOTONIC,&ts)==0);
    return ts.tv_sec+ts.tv_nsec*1e-9;
}
static int compare(const void *a,const void *b) {
    double x=*(const double *)a,y=*(const double *)b;
    return (x>y)-(x<y);
}
static void run(unsigned frames,unsigned blocks,int wide,int mode) {
    sb_music_sequence_t sequence={0};
    sb_music_transport_t transport;
    sb_music_transport_init(&transport);
    sb_music_instrument_t instruments[9];
    sb_music_event_t events[18];
    const double old_hz[9]={110,220,440,660,880,110.125,330,550,879.75};
    const double new_hz[9]={27.5,55,90,110,440,880,1760,3520,4186.009044809578};
    for (unsigned v=0;v<9;++v) {
        sb_audio_voice_kind_t kind=SB_AUDIO_VOICE_PULSE;
        if (mode==1) kind=v%3==0?SB_AUDIO_VOICE_TRIANGLE:v%3==1?SB_AUDIO_VOICE_SAW:SB_AUDIO_VOICE_PULSE;
        instruments[v]=(sb_music_instrument_t){.waveform=kind,.envelope={0,0,1,.1},
            .volume=1,.pan=(v%3-1.0),.pulse_width=v%2?.95:.05,
            .filter_cutoff=v%2?20:2000,.filter_bypass=false};
        events[v]=(sb_music_event_t){0,v+1,SB_MUSIC_NOTE_ON,wide?new_hz[v]:old_hz[v]};
    }
    for (unsigned v=0;v<9;++v) events[9+v]=(sb_music_event_t){UINT64_MAX,v+1,SB_MUSIC_NOTE_OFF,0};
    sb_music_song_t song={48000,9,UINT64_MAX,instruments,events,18};
    CHECK(sb_music_sequence_start(&sequence,&song));
    sb_wav_pool_t wav={0};
    sb_wav_handle_t ids[8];
    for (unsigned j=0;j<8;++j) {
        sb_wav_data_t data={.frames=16384,.channels=2};
        data.samples=malloc(data.frames*2*sizeof(float)); CHECK(data.samples);
        for (size_t i=0;i<data.frames;++i) {
            data.samples[2*i]=(float)(.8*sin(6.283185307179586*(i+j)/137));
            data.samples[2*i+1]=(float)(.8*cos(6.283185307179586*(i+j)/173));
        }
        sb_wav_handle_t asset;
        CHECK(sb_wav_insert(&wav,&data,&asset)==SB_WAV_OK);
        CHECK(sb_wav_play(&wav,asset,1,0,true,&ids[j])==SB_WAV_OK);
    }
    float *output=malloc(2*frames*sizeof(float)); CHECK(output);
    double *times=malloc(blocks*sizeof(double)); CHECK(times);
    double sum=0,peak=0,energy=0; unsigned missed=0;
    const double deadline=frames/48000.0;
    /* Warm up 32 blocks; time control updates too in transition workload. */
    for (unsigned block=0;block<blocks+32;++block) {
        measuring=block>=32;
        double start=now();
        memset(output,0,frames*2*sizeof(float));
        if (mode==2 && block>0) {
            for (unsigned v=1;v<=9;++v) {
                /* Wait for each previous 480-frame transition to finish. */
                if (!sequence.bank.switch_left[v-1])
                    CHECK(sb_audio_bank_waveform(&sequence.bank,v,
                        sequence.bank.kind[v-1]==SB_AUDIO_VOICE_PULSE?SB_AUDIO_VOICE_SAW:SB_AUDIO_VOICE_PULSE));
                CHECK(sb_audio_bank_pitch(&sequence.bank,v,wide?new_hz[(v+block)%9]:old_hz[(v+block)%9]));
                CHECK(sb_audio_bank_pulse_width(&sequence.bank,v,block%2?.95:.05));
                CHECK(sb_audio_bank_filter_cutoff(&sequence.bank,v,block%2?20:20000));
                CHECK(sb_audio_bank_filter_bypass(&sequence.bank,v,block%2!=0));
            }
        }
        for (unsigned i=0;i<frames;++i) {
            float synth[2],sample[2];
            CHECK(sb_music_transport_render_frame(&transport,&sequence,synth));
            sb_wav_render(&wav,sample,1);
            output[2*i]=(float)((synth[0]*0.790569415f+sample[0])*transport.fade.value);
            output[2*i+1]=(float)((synth[1]*0.790569415f+sample[1])*transport.fade.value);
            sb_music_transport_advance_frame(&transport,&sequence);
        }
        double elapsed=now()-start;
        measuring=0;
        if (block>=32) {
            times[block-32]=elapsed; sum+=elapsed;
            if (elapsed>=deadline) ++missed;
        }
        for (unsigned i=0;i<2*frames;++i) {
            CHECK(isfinite(output[i]) && fabs(output[i])<=1);
            if (fabs(output[i])>peak) peak=fabs(output[i]);
            energy+=output[i]*output[i];
        }
    }
    CHECK(!allocations && energy>0);
    for (unsigned v=1;v<=9;++v) CHECK(sb_audio_bank_active(&sequence.bank,v));
    for (unsigned v=0;v<8;++v) CHECK(sb_wav_playing(&wav,ids[v]));
    qsort(times,blocks,sizeof(double),compare);
    printf("%s mean_ms=%.4f p99_ms=%.4f max_ms=%.4f p99_deadline_pct=%.2f max_deadline_pct=%.2f misses=%u heap_calls=%u peak=%.6f\n",
        mode==0?"pulse":mode==1?"mixed":"transitions",sum/blocks*1000,
        times[(blocks*99+99)/100-1]*1000,times[blocks-1]*1000,
        100*times[(blocks*99+99)/100-1]/deadline,100*times[blocks-1]/deadline,missed,allocations,peak);
    free(times); free(output); sb_wav_pool_reset(&wav);
}
int main(int argc,char **argv) {
    CHECK(argc==4);
    unsigned frames=(unsigned)strtoul(argv[1],NULL,10),blocks=(unsigned)strtoul(argv[2],NULL,10);
    int wide=atoi(argv[3]);
    CHECK(frames>=64 && frames<=4096 && blocks>=100 && blocks<=10000);
    printf("frames=%u blocks=%u deadline_ms=%.4f range=%s\n",frames,blocks,1000.0*frames/48000,wide?"A0-C8":"110-880");
    for (int mode=0;mode<3;++mode) run(frames,blocks,wide,mode);
    return 0;
}
