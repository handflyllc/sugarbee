#include "sb_stats.h"
#include <math.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define NEAR(a,b) (fabs((a)-(b)) < 1e-8)
int main(void) {
    sb_stats_t s;
    sb_stats_reset(&s);
    for(int i=0;i<30;++i) sb_stats_add(&s,1.0/60,0.002,0.003,0.01,0);
    CHECK(!s.view.ready);
    for(int i=0;i<30;++i) sb_stats_add(&s,1.0/60,0.002,0.003,0.01,0);
    CHECK(s.view.ready && NEAR(s.view.fps,60));
    CHECK(NEAR(s.view.frame_average,1.0/60) && NEAR(s.view.frame_worst,1.0/60));
    CHECK(NEAR(s.view.update,0.002) && NEAR(s.view.draw,0.003) && NEAR(s.view.present,0.01));
    sb_stats_add(&s,0.1,0.02,0.03,0.04,0.017);
    CHECK(NEAR(s.view.frame_worst,1.0/60)); /* Refresh is throttled. */
    sb_stats_add(&s,0.4,0.01,0.02,0.03,0.317);
    CHECK(NEAR(s.view.frame_worst,0.4) && NEAR(s.view.dropped,0.334));
    for(int i=0;i<90;++i) sb_stats_add(&s,1.0/60,0,0,0,0);
    CHECK(NEAR(s.view.dropped,0) && NEAR(s.view.frame_worst,1.0/60));
    double now=s.now;
    sb_stats_add(&s,NAN,0,0,0,0); sb_stats_add(&s,INFINITY,0,0,0,0);
    sb_stats_add(&s,0,0,0,0,0); sb_stats_add(&s,1,-1,0,0,0);
    CHECK(s.now==now);
    sb_stats_reset(&s);
    CHECK(!s.view.ready && s.count==0 && s.now==0);
    for(int i=0;i<10000;++i) sb_stats_add(&s,0.0001,0,0,0,0);
    CHECK(s.count==SB_STATS_CAPACITY && s.view.limited);
    for(int i=0;i<120;++i) sb_stats_add(&s,1.0/60,0,0,0,0);
    CHECK(!s.view.limited && s.view.ready && NEAR(s.view.fps,60));
    sb_stats_reset(NULL); sb_stats_add(NULL,1,0,0,0,0);
    return 0;
}
