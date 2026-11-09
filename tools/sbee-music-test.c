/* Listening host for prepared songs. No cartridge or production SDL backend. */
#include "../src/sb_music_sequence.h"
#include "../src/sb_music_host.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    sb_music_sequence_t sequence;
    sb_music_host_t music_host;
    bool pause_mode, failed;
    uint64_t remaining;
    bool fade_at_limit;
    float *capture;
    size_t capture_frames, capture_capacity;
    size_t transition_frames[9];
} host_t;
static void audio_callback(void *userdata, Uint8 *stream, int length) {
    host_t *host=userdata;
    if (length <= 0) return;
    memset(stream,0,(size_t)length);
    size_t frames=(size_t)length/(2*sizeof(float));
    if(host->remaining && frames>host->remaining) frames=(size_t)host->remaining;
    if (length % (int)(2*sizeof(float)) != 0 ||
        !(host->pause_mode ? sb_music_host_render(&host->music_host,(float *)stream,frames) :
          sb_music_sequence_render(&host->sequence,(float *)stream,frames))) {
        host->failed=true;
        return;
    }
    if(host->remaining) {
        host->remaining-=frames;
        /* Diagnostic countdown: stop within this buffer, or clean up a fixed
         * number of completed eligible loop iterations. */
        if(!host->remaining) {
            if(host->fade_at_limit) {
                if(!sb_music_sequence_stop(&host->sequence) ||
                   !sb_music_sequence_render(&host->sequence,(float *)stream+2*frames,
                       (size_t)length/(2*sizeof(float))-frames)) host->failed=true;
            } else sb_music_sequence_reset(&host->sequence);
        }
    }
    /* Match the current SDL bank attenuation; bank already applies 1/9. */
    float *samples=(float *)stream;
    for(size_t n=0;n<(size_t)length/sizeof(float);++n)
        samples[n]*=0.790569415f;
    if (host->capture) {
        size_t count=(size_t)length/(2*sizeof(float));
        if (count>host->capture_capacity-host->capture_frames) host->failed=true;
        else {
            memcpy(host->capture+2*host->capture_frames,stream,count*2*sizeof(float));
            host->capture_frames+=count;
        }
    }
}

static void chord(sb_music_event_t *events, size_t *count, uint64_t start,
                  uint64_t hold, const double *hz, unsigned voices) {
    for(unsigned v=1;v<=voices;++v)
        events[(*count)++]=(sb_music_event_t){start,v,SB_MUSIC_NOTE_ON,hz[v-1]};
    for(unsigned v=1;v<=voices;++v)
        events[(*count)++]=(sb_music_event_t){start+hold,v,SB_MUSIC_NOTE_OFF,0};
}

static bool play(SDL_AudioDeviceID device, host_t *host, const sb_music_song_t *song,
                 Uint32 timeout, unsigned iterations, uint64_t stop_after, Uint32 gain_step_ms) {
    /* Device remains paused while caller-owned fixture data is prepared. */
    host->fade_at_limit=stop_after!=0;
    host->remaining=stop_after?stop_after:(iterations>1?song->end_frame*iterations:0);
    if(iterations>1) {
        if(!sb_music_sequence_start_loop(&host->sequence,song)) return false;
    } else if(!sb_music_sequence_start(&host->sequence,song)) return false;
    SDL_PauseAudioDevice(device,0);
    Uint32 started=SDL_GetTicks();
    bool done=false, failed=false;
    unsigned gain_step=0;
    const unsigned gain_times[]={1,2,4};
    const double gain_values[]={.25,0,1};
    while(SDL_GetTicks()-started<timeout) {
        SDL_LockAudioDevice(device);
        if(gain_step_ms && gain_step<3 &&
           SDL_GetTicks()-started >= gain_step_ms*gain_times[gain_step]) {
            if(!sb_music_sequence_set_gain(&host->sequence,gain_values[gain_step]))
                host->failed=true;
            ++gain_step;
        }
        done=host->sequence.state==SB_MUSIC_IDLE;
        failed=host->failed;
        SDL_UnlockAudioDevice(device);
        if(done || failed) break;
        SDL_Delay(5);
    }
    SDL_PauseAudioDevice(device,1);
    return done && !failed;
}

/* Control thread only; fixtures outlive the callback. Native tests establish
 * exact sample boundaries; this script exercises the same adapter audibly. */
static bool play_pause(SDL_AudioDeviceID device, host_t *host, bool smoke) {
    sb_music_instrument_t ins[3];
    for (unsigned v=0;v<3;++v)
        ins[v]=(sb_music_instrument_t){SB_AUDIO_VOICE_TRIANGLE,{.01,.1,.7,.12},.6,0,.5,2000,true};
    sb_music_event_t events[16]; size_t count=0;
    const double major[]={220,277.1826309768721,329.6275569128699};
    const double minor[]={220,261.6255653005986,329.6275569128699};
    chord(events,&count,0,36000,major,3);
    chord(events,&count,48000,36000,minor,3);
    sb_music_song_t song={48000,3,96000,ins,events,count};
    host->pause_mode=true;
    sb_music_host_t *h=&host->music_host;
    sb_music_host_reset(h);
    bool ok=sb_music_host_start(h,&song,true);
    if (ok) SDL_PauseAudioDevice(device,0);
    const char *messages[]={
        "Pause: fade out, then hold silent.",
        "Resume: continue the frozen song.",
        "Stop during pause fade: stay stopped on resume.",
        "Resume after stop: silence.",
        "Start while paused: hold opening events at frame zero.",
        "Cancel held start, prepare again, resume: opening chord fades in.",
        "Pause again: wait until fully silent.",
        "Stop fully paused, then resume: silence.",
        "Paused-stop checks complete."};
    for (unsigned step=0;ok && step<9;++step) {
        SDL_Delay(smoke?60:1000);
        puts(messages[step]); fflush(stdout);
        SDL_LockAudioDevice(device);
        host->transition_frames[step]=host->capture_frames;
        switch (step) {
        case 0: case 6: ok=sb_music_host_pause(h,true); break;
        case 1: ok=sb_music_host_pause(h,false); break;
        case 2: ok=sb_music_host_pause(h,true) && sb_music_host_stop(h); break;
        case 3: ok=h->sequence.state==SB_MUSIC_IDLE && sb_music_host_pause(h,false); break;
        case 4: ok=h->sequence.state==SB_MUSIC_IDLE && sb_music_host_pause(h,true) &&
                   sb_music_host_start(h,&song,true); break;
        case 5: ok=h->sequence.frame==0 && h->sequence.next_event==0 && h->held_start &&
                   sb_music_host_stop(h) && sb_music_host_start(h,&song,true) &&
                   sb_music_host_pause(h,false); break;
        case 7: ok=h->paused && !h->fade.left && sb_music_host_stop(h) &&
                   h->sequence.state==SB_MUSIC_IDLE && sb_music_host_pause(h,false); break;
        case 8: ok=h->sequence.state==SB_MUSIC_IDLE; break;
        }
        ok=ok && !host->failed;
        SDL_UnlockAudioDevice(device);
    }
    SDL_PauseAudioDevice(device,1);
    sb_music_host_reset(h); /* Exclude callback before borrowed fixtures expire. */
    return ok;
}

/* File I/O only after the device closes. Preserve the exact float samples
 * supplied to SDL, including attenuation, rather than quantizing to PCM16. */
static bool write_u32(SDL_RWops *file, Uint32 value) {
    value=SDL_SwapLE32(value);
    return SDL_RWwrite(file,&value,sizeof(value),1)==1;
}
static bool save_capture(const char *path, const host_t *host) {
    SDL_RWops *file=SDL_RWFromFile(path,"wb");
    if (!file) { fprintf(stderr,"Capture open: %s\n",SDL_GetError()); return false; }
    Uint32 bytes=(Uint32)(host->capture_frames*2*sizeof(float));
    /* IEEE float stereo, 48 kHz, fmt + fact + data; all fields little-endian. */
    bool ok=SDL_RWwrite(file,"RIFF",4,1)==1 && write_u32(file,bytes+48) &&
        SDL_RWwrite(file,"WAVEfmt ",8,1)==1 && write_u32(file,16) &&
        write_u32(file,0x00020003) && write_u32(file,48000) &&
        write_u32(file,48000*8) && write_u32(file,0x00200008) &&
        SDL_RWwrite(file,"fact",4,1)==1 && write_u32(file,4) &&
        write_u32(file,(Uint32)host->capture_frames) &&
        SDL_RWwrite(file,"data",4,1)==1 && write_u32(file,bytes);
    for (size_t n=0;ok && n<host->capture_frames*2;++n) {
        Uint32 bits;
        memcpy(&bits,&host->capture[n],sizeof(bits));
        ok=write_u32(file,bits);
    }
    if (SDL_RWclose(file)!=0) ok=false;
    if (!ok) fprintf(stderr,"Capture write failed: %s\n",path);
    else {
        printf("Captured %zu frames to %s\n",host->capture_frames,path);
        for (unsigned n=0;n<9;++n)
            printf("Transition %u: frame %zu (%.6f seconds)\n",n,
                   host->transition_frames[n],host->transition_frames[n]/48000.0);
        puts("Transition 2 is stop during pause fade. Capture is before SDL/hardware conversion.");
    }
    return ok;
}

int main(int argc, char **argv) {
    const char *capture_path=argc==3 && strcmp(argv[1],"--pause-capture")==0 ? argv[2] : NULL;
    bool pausing=capture_path || (argc==2 && (strcmp(argv[1],"--pause")==0 || strcmp(argv[1],"--pause-smoke")==0));
    bool gaining=argc==2 && (strcmp(argv[1],"--gain")==0 || strcmp(argv[1],"--gain-smoke")==0);
    bool stopping=argc==2 && (strcmp(argv[1],"--stop")==0 || strcmp(argv[1],"--stop-smoke")==0);
    bool looping=argc==2 && (strcmp(argv[1],"--loop")==0 || strcmp(argv[1],"--loop-smoke")==0);
    bool smoke=argc==2 && (strcmp(argv[1],"--smoke")==0 || strcmp(argv[1],"--loop-smoke")==0 || strcmp(argv[1],"--stop-smoke")==0 || strcmp(argv[1],"--gain-smoke")==0 || strcmp(argv[1],"--pause-smoke")==0);
    if(argc!=1 && !smoke && !looping && !stopping && !gaining && !pausing) {
        fprintf(stderr,"Usage: %s [--smoke | --loop | --loop-smoke | --stop | --stop-smoke | --gain | --gain-smoke | --pause | --pause-smoke | --pause-capture output.wav]\n",argv[0]); return 1;
    }
    if(SDL_Init(SDL_INIT_AUDIO)!=0) {
        fprintf(stderr,"SDL audio: %s\n",SDL_GetError()); return 1;
    }
    host_t host={0};
    if (capture_path) {
        host.capture_capacity=48000u*20u;
        host.capture=SDL_malloc(host.capture_capacity*2*sizeof(float));
        if (!host.capture) { SDL_Quit(); return 1; }
        /* Touch storage before playback; callback does only a bounded copy. */
        memset(host.capture,0,host.capture_capacity*2*sizeof(float));
    }
    SDL_AudioSpec want={0}, got={0};
    want.freq=48000; want.format=AUDIO_F32SYS; want.channels=2;
    want.samples=512; want.callback=audio_callback; want.userdata=&host;
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(NULL,0,&want,&got,SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if(!device) {
        fprintf(stderr,"Open audio: %s\n",SDL_GetError()); SDL_free(host.capture); SDL_Quit(); return 1;
    }
    if (capture_path) printf("SDL driver: %s; callback: %d Hz, %u channels, %u frames\n",
        SDL_GetCurrentAudioDriver(),got.freq,(unsigned)got.channels,(unsigned)got.samples);
    puts("Prepared music listening test: start with low speaker volume.");
    bool ok=pausing ? play_pause(device,&host,smoke) : true;
    for(unsigned pass=0;!pausing && ok && pass<(smoke?1u:gaining?2u:stopping?3u:looping?1u:5u);++pass) {
        sb_music_instrument_t ins[3];
        for(unsigned v=0;v<3;++v) {
            sb_audio_voice_kind_t wave=SB_AUDIO_VOICE_TRIANGLE;
            if(pass==1) wave=SB_AUDIO_VOICE_SAW;
            if(pass==2) wave=SB_AUDIO_VOICE_PULSE;
            if(pass>=3 || looping || stopping || gaining) wave=v==0?SB_AUDIO_VOICE_TRIANGLE:v==1?SB_AUDIO_VOICE_SAW:SB_AUDIO_VOICE_PULSE;
            ins[v]=(sb_music_instrument_t){wave,{.01,.1,.7,.12},.6,0,.5,2000,true};
        }
        sb_music_event_t events[128]; size_t count=0;
        uint64_t end;
        const double major[]={220,277.1826309768721,329.6275569128699};
        const double minor[]={220,261.6255653005986,329.6275569128699};
        if(stopping) {
            puts(pass==0?"Stop a held looping chord; then replay.":
                 pass==1?"Stop during natural release tails.":"Stop in an authored gap; no next iteration.");
            chord(events,&count,0,84000,major,3);
            end=pass==1?84000:96000;
        } else if(looping || gaining) {
            puts(gaining?(pass==0?"Gain: full, quarter, mute across loop boundary, then full again.":
                 "Replay: full gain restored without another gain command."):
                 "Four identical mixed-instrument loops; authored releases and gaps, no added boundary delay.");
            chord(events,&count,0,smoke?480:24000,major,3);
            if(!smoke) chord(events,&count,48000,36000,minor,3);
            end=smoke?7200:96000;
        } else if(smoke) {
            chord(events,&count,0,480,major,3); end=4800;
        } else if(pass==0) {
            puts("Triangle: unison, octave, fifth, major third; then major/minor triads.");
            const double upper[]={220,440,329.6275569128699,277.1826309768721};
            for(unsigned n=0;n<4;++n) {
                double pair[]={220,upper[n]}; chord(events,&count,n*72000u,48000,pair,2);
            }
            chord(events,&count,288000,48000,major,3);
            chord(events,&count,360000,48000,minor,3); end=432000;
        } else if(pass<4) {
            puts(pass==1?"Saw: major/minor, repeated chord, then adjacent chord change.":
                 pass==2?"Pulse: major/minor, repeated chord, then adjacent chord change.":
                         "Mixed triangle/saw/pulse: same chord sequence.");
            chord(events,&count,0,48000,major,3);
            chord(events,&count,72000,48000,minor,3);
            chord(events,&count,144000,24000,major,3);
            chord(events,&count,180000,24000,major,3);
            chord(events,&count,204000,48000,minor,3); end=276000;
        } else {
            puts("Mixed: short original melody over two held accompaniment voices.");
            const double melody[]={440,493.8833012561241,523.2511306011972,659.2551138257398};
            events[count++]=(sb_music_event_t){0,1,SB_MUSIC_NOTE_ON,melody[0]};
            events[count++]=(sb_music_event_t){0,2,SB_MUSIC_NOTE_ON,220};
            events[count++]=(sb_music_event_t){0,3,SB_MUSIC_NOTE_ON,329.6275569128699};
            for(unsigned n=0;n<4;++n) {
                if(n) events[count++]=(sb_music_event_t){n*24000u,1,SB_MUSIC_NOTE_ON,melody[n]};
                events[count++]=(sb_music_event_t){n*24000u+18000,1,SB_MUSIC_NOTE_OFF,0};
            }
            events[count++]=(sb_music_event_t){96000,2,SB_MUSIC_NOTE_OFF,0};
            events[count++]=(sb_music_event_t){96000,3,SB_MUSIC_NOTE_OFF,0}; end=120000;
        }
        fflush(stdout);
        sb_music_song_t song={48000,3,end,ins,events,count};
        ok=play(device,&host,&song,smoke?3000:15000,
                looping || (gaining && pass==0) || (stopping && pass!=1)?4:1,
                stopping?(smoke?480:pass==0?24000:pass==1?86400:91200):0,
                gaining && pass==0?(smoke?20:1000):0);
        /* Pausing above excludes callback access before local data expires. */
        sb_music_sequence_reset(&host.sequence);
        if(stopping && !smoke) SDL_Delay(250);
    }
    SDL_CloseAudioDevice(device);
    if (capture_path && ok) ok=save_capture(capture_path,&host);
    SDL_free(host.capture);
    SDL_Quit();
    puts(ok?"Sequence complete; musical quality requires listening acceptance.":
            "Sequence failed validation, rendering, or timed out.");
    return ok?0:1;
}
