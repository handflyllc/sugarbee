/* Listening host for prepared songs. No cartridge or production SDL backend. */
#include "../src/sb_music_sequence.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct { sb_music_sequence_t sequence; bool failed; uint64_t remaining; bool fade_at_limit; } host_t;
static void audio_callback(void *userdata, Uint8 *stream, int length) {
    host_t *host=userdata;
    memset(stream,0,(size_t)length);
    size_t frames=(size_t)length/(2*sizeof(float));
    if(host->remaining && frames>host->remaining) frames=(size_t)host->remaining;
    if (length % (int)(2*sizeof(float)) != 0 ||
        !sb_music_sequence_render(&host->sequence,(float *)stream,
                                 frames)) {
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
}

static void chord(sb_music_event_t *events, size_t *count, uint64_t start,
                  uint64_t hold, const double *hz, unsigned voices) {
    for(unsigned v=1;v<=voices;++v)
        events[(*count)++]=(sb_music_event_t){start,v,SB_MUSIC_NOTE_ON,hz[v-1]};
    for(unsigned v=1;v<=voices;++v)
        events[(*count)++]=(sb_music_event_t){start+hold,v,SB_MUSIC_NOTE_OFF,0};
}

static bool play(SDL_AudioDeviceID device, host_t *host, const sb_music_song_t *song,
                 Uint32 timeout, unsigned iterations, uint64_t stop_after) {
    /* Device remains paused while caller-owned fixture data is prepared. */
    host->fade_at_limit=stop_after!=0;
    host->remaining=stop_after?stop_after:(iterations>1?song->end_frame*iterations:0);
    if(iterations>1) {
        if(!sb_music_sequence_start_loop(&host->sequence,song)) return false;
    } else if(!sb_music_sequence_start(&host->sequence,song)) return false;
    SDL_PauseAudioDevice(device,0);
    Uint32 started=SDL_GetTicks();
    bool done=false, failed=false;
    while(SDL_GetTicks()-started<timeout) {
        SDL_LockAudioDevice(device);
        done=host->sequence.state==SB_MUSIC_IDLE;
        failed=host->failed;
        SDL_UnlockAudioDevice(device);
        if(done || failed) break;
        SDL_Delay(5);
    }
    SDL_PauseAudioDevice(device,1);
    return done && !failed;
}

int main(int argc, char **argv) {
    bool stopping=argc==2 && (strcmp(argv[1],"--stop")==0 || strcmp(argv[1],"--stop-smoke")==0);
    bool looping=argc==2 && (strcmp(argv[1],"--loop")==0 || strcmp(argv[1],"--loop-smoke")==0);
    bool smoke=argc==2 && (strcmp(argv[1],"--smoke")==0 || strcmp(argv[1],"--loop-smoke")==0 || strcmp(argv[1],"--stop-smoke")==0);
    if(argc!=1 && !smoke && !looping && !stopping) {
        fprintf(stderr,"Usage: %s [--smoke | --loop | --loop-smoke | --stop | --stop-smoke]\n",argv[0]); return 1;
    }
    if(SDL_Init(SDL_INIT_AUDIO)!=0) {
        fprintf(stderr,"SDL audio: %s\n",SDL_GetError()); return 1;
    }
    host_t host={0};
    SDL_AudioSpec want={0}, got={0};
    want.freq=48000; want.format=AUDIO_F32SYS; want.channels=2;
    want.samples=512; want.callback=audio_callback; want.userdata=&host;
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(NULL,0,&want,&got,SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if(!device) {
        fprintf(stderr,"Open audio: %s\n",SDL_GetError()); SDL_Quit(); return 1;
    }
    puts("Prepared music listening test: start with low speaker volume.");
    bool ok=true;
    for(unsigned pass=0;ok && pass<(smoke?1u:stopping?3u:looping?1u:5u);++pass) {
        sb_music_instrument_t ins[3];
        for(unsigned v=0;v<3;++v) {
            sb_audio_voice_kind_t wave=SB_AUDIO_VOICE_TRIANGLE;
            if(pass==1) wave=SB_AUDIO_VOICE_SAW;
            if(pass==2) wave=SB_AUDIO_VOICE_PULSE;
            if(pass>=3 || looping || stopping) wave=v==0?SB_AUDIO_VOICE_TRIANGLE:v==1?SB_AUDIO_VOICE_SAW:SB_AUDIO_VOICE_PULSE;
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
        } else if(looping) {
            puts("Four identical mixed-instrument loops; authored releases and gaps, no added boundary delay.");
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
                looping || (stopping && pass!=1)?4:1,
                stopping?(smoke?480:pass==0?24000:pass==1?86400:91200):0);
        /* Pausing above excludes callback access before local data expires. */
        sb_music_sequence_reset(&host.sequence);
        if(stopping && !smoke) SDL_Delay(250);
    }
    SDL_CloseAudioDevice(device); SDL_Quit();
    puts(ok?"Sequence complete; musical quality requires listening acceptance.":
            "Sequence failed validation, rendering, or timed out.");
    return ok?0:1;
}
