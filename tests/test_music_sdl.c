/* Real production callback, manually driven with the dummy device suspended. */
#include "../src/sb_audio_sdl.c"
#include "wav_fixture.h"
#include <math.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Music SDL line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static const sb_music_instrument_t instrument={SB_AUDIO_VOICE_TRIANGLE,{0,0,1,.01},.4,-.5,.5,2000,true};
static const sb_music_event_t events[]={{0,1,SB_MUSIC_NOTE_ON,220},{4000,1,SB_MUSIC_NOTE_OFF,0}};
static const sb_music_song_t song={48000,1,5000,&instrument,events,2};
static const unsigned map[]={3};
static float out[4096];
static void render(sb_audio_t *a,unsigned frames) { audio_output(a,(Uint8 *)out,(int)(frames*2*sizeof(float))); }
static bool silent(unsigned frames) {
    for (unsigned i=0;i<frames*2;++i) if (out[i]!=0) return false;
    return true;
}
static bool effect(sb_audio_t *a) {
    return sb_audio_voice_configure(a,9,0,0,1,.01) && sb_audio_voice_pan(a,9,1) &&
        sb_audio_voice_saw_on(a,9,660);
}
int main(void) {
    CHECK(!sb_audio_music_start(NULL,&song,map,1,true));
    CHECK(!sb_audio_music_stop(NULL) && !sb_audio_music_reset(NULL));
    CHECK(!sb_audio_music_gain(NULL,1) && !sb_audio_music_reserved(NULL,1));
    CHECK(sb_audio_music_state(NULL)==SB_MUSIC_IDLE);
    sb_audio_t *a=sb_audio_create(); CHECK(a); SDL_PauseAudioDevice(a->device,1);
    CHECK(!sb_audio_music_start(a,&song,map,1,true)); /* Diagnostics aren't sessions. */
    sb_audio_session_begin(a);
    CHECK(effect(a));
    sb_music_shared_t before=a->music;
    unsigned busy[]={9},invalid[]={0};
    CHECK(!sb_audio_music_start(a,&song,busy,1,true));
    CHECK(!sb_audio_music_start(a,&song,invalid,1,true));
    CHECK(!memcmp(&before,&a->music,sizeof before));
    CHECK(sb_audio_music_start(a,&song,map,1,true));
    CHECK(sb_audio_music_reserved(a,3) && !sb_audio_music_reserved(a,9));
    before=a->music;
    CHECK(!sb_audio_music_start(a,&song,map,1,true));
    CHECK(!sb_audio_music_gain(a,NAN));
    CHECK(!sb_audio_voice_configure(a,3,0,0,1,0));
    CHECK(!sb_audio_voice_note_on(a,3,440));
    CHECK(!sb_audio_voice_noise_on(a,3)); CHECK(!sb_audio_voice_saw_on(a,3,440));
    CHECK(!sb_audio_voice_pulse_on(a,3,440)); CHECK(!sb_audio_voice_waveform(a,3,SB_AUDIO_VOICE_NOISE));
    CHECK(!sb_audio_voice_start_selected(a,3,440)); CHECK(!sb_audio_voice_start_selected_noise(a,3));
    CHECK(!sb_audio_voice_pitch(a,3,440)); CHECK(!sb_audio_voice_volume(a,3,1));
    CHECK(!sb_audio_voice_pan(a,3,0)); CHECK(!sb_audio_voice_pulse_width(a,3,.5));
    CHECK(!sb_audio_voice_seed(a,3,1)); CHECK(!sb_audio_voice_filter_cutoff(a,3,500));
    CHECK(!sb_audio_voice_filter_bypass(a,3,true)); CHECK(!sb_audio_voice_note_off(a,3));
    CHECK(!sb_audio_voice_stop(a,3));
    sb_audio_voice_settings_t settings=sb_audio_voice_defaults();
    CHECK(!sb_audio_voice_apply_settings(a,3,&settings));
    CHECK(!memcmp(&before,&a->music,sizeof before));
    size_t length; unsigned char *bytes=fixture(48000,1,10000,&length); CHECK(bytes);
    sb_wav_handle_t asset,id;
    CHECK(sb_audio_wav_load(a,bytes,length,&asset)==SB_WAV_OK); free(bytes);
    CHECK(sb_audio_wav_play_loop(a,asset,1,0,&id)==SB_WAV_OK);
    render(a,120);
    CHECK(sb_audio_voice_active(a,3));
    /* Independent sequence + WAV reference, analytic ONE fade across the mix. */
    sb_music_sequence_t sequence=a->music.sequence;
    sb_wav_pool_t wav=a->wav;
    sb_audio_session_pause(a,true);
    render(a,480);
    for (unsigned i=0;i<480;++i) {
        float synth[2],sample[2];
        CHECK(sb_music_sequence_render(&sequence,synth,1)); sb_wav_render(&wav,sample,1);
        for (unsigned ch=0;ch<2;++ch) {
            float want=(float)((synth[ch]*bank_diagnostic_gain+sample[ch])*(1-i/480.0));
            CHECK(out[2*i+ch]==want);
        }
    }
    CHECK(!memcmp(&sequence,&a->music.sequence,sizeof sequence));
    CHECK(!memcmp(&wav,&a->wav,sizeof wav));
    before=a->music; render(a,2048); CHECK(silent(2048));
    CHECK(!memcmp(&before,&a->music,sizeof before) && !memcmp(&wav,&a->wav,sizeof wav));
    sb_audio_session_pause(a,false); render(a,480);
    for (unsigned i=0;i<480;++i) {
        float synth[2],sample[2];
        CHECK(sb_music_sequence_render(&sequence,synth,1)); sb_wav_render(&wav,sample,1);
        for (unsigned ch=0;ch<2;++ch)
            CHECK(out[2*i+ch]==(float)((synth[ch]*bank_diagnostic_gain+sample[ch])*(i/480.0)));
    }
    CHECK(sb_audio_music_gain(a,0)); render(a,480);
    CHECK(a->music.sequence.gain==0 && sb_audio_voice_active(a,9) && sb_audio_wav_playing(a,id));
    /* Partition invariance across gain, pause reversal and loop boundary. */
    CHECK(sb_audio_music_gain(a,.8)); sb_audio_session_pause(a,true); render(a,113);
    sb_audio_session_pause(a,false);
    sb_audio_t copy=*a; float whole[20000],parts[20000];
    audio_output(a,(Uint8 *)whole,sizeof whole);
    for (unsigned i=0;i<10000;i+=37) {
        unsigned n=10000-i<37?10000-i:37;
        audio_output(&copy,(Uint8 *)(parts+2*i),(int)(n*2*sizeof(float)));
    }
    CHECK(!memcmp(whole,parts,sizeof whole));
    CHECK(!memcmp(&a->music,&copy.music,sizeof a->music));
    CHECK(!memcmp(&a->wav,&copy.wav,sizeof a->wav));
    CHECK(sb_audio_music_stop(a)); render(a,480);
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE && !sb_audio_music_reserved(a,3));
    CHECK(sb_audio_voice_active(a,9) && sb_audio_wav_playing(a,id));
    /* Start during fade holds music only, effects/WAV continue until silence. */
    sb_audio_session_pause(a,true); render(a,30);
    CHECK(sb_audio_music_start(a,&song,map,1,true));
    sb_audio_bank_t bank=a->music.sequence.bank; wav=a->wav;
    render(a,450);
    CHECK(a->music.sequence.frame==0 && a->music.sequence.next_event==0);
    float scratch[900]; CHECK(sb_audio_bank_render_unreserved(&bank,1u<<2,scratch,450));
    sb_wav_render(&wav,scratch,450);
    CHECK(!memcmp(&bank,&a->music.sequence.bank,sizeof bank));
    CHECK(!memcmp(&wav,&a->wav,sizeof wav));
    CHECK(sb_audio_music_stop(a)); CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE);
    sb_audio_session_pause(a,false); render(a,480);
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE && sb_audio_voice_active(a,9));
    CHECK(sb_audio_music_start(a,&song,map,1,true)); render(a,90);
    sb_audio_session_pause(a,true); render(a,20);
    CHECK(sb_audio_music_stop(a)); render(a,460);
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE);
    sb_audio_session_pause(a,false); render(a,480);
    CHECK(sb_audio_music_start(a,&song,map,1,true)); render(a,20);
    sb_audio_bank_t effects=a->music.sequence.bank;
    sb_music_transport_t transport=a->music.transport; wav=a->wav;
    CHECK(sb_audio_music_reset(a));
    CHECK(!memcmp(&effects.saw[8],&a->music.sequence.bank.saw[8],sizeof effects.saw[8]));
    CHECK(!memcmp(&transport,&a->music.transport,sizeof transport));
    CHECK(!memcmp(&wav,&a->wav,sizeof wav));
    /* Failure cancels scheduling, clears all sources after fade, stays failed. */
    CHECK(sb_audio_music_start(a,&song,map,1,true)); render(a,20);
    sb_audio_session_fail(a);
    CHECK(!sb_audio_music_start(a,&song,map,1,true) && !sb_audio_music_gain(a,1));
    CHECK(!sb_audio_music_stop(a) && !sb_audio_music_reset(a));
    CHECK(!sb_audio_voice_note_on(a,9,440));
    render(a,480);
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE && !a->music.sequence.song);
    CHECK(!bank_active(a) && !sb_audio_wav_playing(a,id));
    sb_audio_session_pause(a,false); render(a,2048); CHECK(silent(2048));
    sb_audio_session_begin(a); CHECK(!a->wav.bytes && !a->music.sequence.reserved);
    CHECK(sb_audio_music_start(a,&song,map,1,true));
    sb_audio_session_pause(a,true); render(a,480);
    sb_audio_session_fail(a); CHECK(!a->music.sequence.song && !bank_active(a));
    sb_audio_session_begin(a); CHECK(sb_audio_music_start(a,&song,map,1,true));
    sb_audio_session_pause(a,true); render(a,20); sb_audio_session_prepare_shutdown(a);
    render(a,2048); CHECK(silent(2048));
    sb_audio_session_end(a); CHECK(!a->music.sequence.song && !a->music.sequence.reserved);
    CHECK(!sb_audio_music_start(a,&song,map,1,true));
    /* Fresh session defaults and natural completion release the borrow. */
    sb_audio_session_begin(a); CHECK(effect(a));
    CHECK(sb_audio_music_start(a,&song,map,1,false));
    for (unsigned i=0;i<6;++i) render(a,1000);
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE && !a->music.sequence.song);
    CHECK(sb_audio_voice_note_on(a,3,440) && sb_audio_voice_active(a,9));
    sb_audio_session_end(a); sb_audio_session_begin(a);
    CHECK(sb_audio_music_start(a,&song,map,1,true)); /* Destroy while borrowed. */
    sb_audio_destroy(a);
    puts("Production music/synth/WAV session checks passed");
    return 0;
}
