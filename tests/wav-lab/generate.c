/* Deterministic, original test sounds. Generated files stay in the build tree. */
#include "../wav_fixture.h"
#include <math.h>
#include <stdio.h>
static int save(const char *dir,const char *name,unsigned rate,unsigned channels,int loop) {
    size_t n; unsigned frames=rate*2;
    unsigned char *b=fixture(rate,channels,frames,&n);
    if (!b) return 1;
    for (unsigned i=0;i<frames;++i) for (unsigned ch=0;ch<channels;++ch) {
        double envelope=loop ? .6 : fmin(1,i/(.01*rate))*(1-(double)i/frames);
        double hz=channels==1 ? 440 : (ch ? 660 : 330);
        int sample=(int)(26000*envelope*sin(6.283185307179586*hz*i/rate));
        put16(b+44+((size_t)i*channels+ch)*2,(unsigned)sample);
    }
    char path[4096]; int length=snprintf(path,sizeof path,"%s/%s",dir,name);
    if (length<0 || (size_t)length>=sizeof path) { free(b); return 1; }
    FILE *f=fopen(path,"wb");
    if (!f) { free(b); return 1; }
    int ok=fwrite(b,1,n,f)==n;
    if (fclose(f)!=0) ok=0;
    free(b); return ok ? 0 : 1;
}
int main(int argc,char **argv) {
    if (argc!=2) return 1;
    return save(argv[1],"mono.wav",22050,1,0) || save(argv[1],"stereo.wav",44100,2,0) ||
        save(argv[1],"loop.wav",48000,1,1);
}
