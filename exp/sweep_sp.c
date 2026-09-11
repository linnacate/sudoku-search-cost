/* sweep_sp.c — 分层计时: 各传播级别的 墙钟 / guesses
 * 目的: 定位墙钟热点在哪一层 (locked? naked?), 并给出 guesses 对照
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

extern void our_init(void);
typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
OurResult our_solve(const char*puzzle);
void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
void our_set_limit(int lim);
void our_set_locked(int on);
void our_set_splevel(int lv);

static double now_ms(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec*1000.0+ts.tv_nsec/1e6;}

int main(int argc,char**argv){
    const char* fn=argc>1?argv[1]:"h2h/tdoku-master/data/puzzles5_forum_hardest_1905_11+";
    int N=argc>2?atoi(argv[2]):2000;
    FILE*f=fopen(fn,"r"); if(!f){printf("cannot open %s\n",fn);return 1;}
    char (*P)[128]=(char(*)[128])malloc(sizeof(char[128])*N);
    int n=0; char buf[512];
    while(fgets(buf,sizeof buf,f)&&n<N){
        int L=(int)strlen(buf); while(L>0&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0;
        if(L<81)continue; memcpy(P[n],buf,81); P[n][81]=0; n++;
    }
    fclose(f); N=n;
    printf("dataset=%s N=%d\n\n",fn,N);
    printf("%-6s %-6s %10s %10s %10s %8s\n","locked","sp","总ms","us/题","avg_gue","geo_gue");

    int cfgs[][2]={{0,0},{1,0},{1,2},{1,3},{1,4}};
    const char*nm[]={"L0","L1","L2","L3","L4"};
    for(int c=0;c<5;c++){
        our_init();
        int keys[5]={0,1,2,3,4};
        our_set_strategy(10,1,5,keys,1);
        our_set_limit(1);
        our_set_locked(cfgs[c][0]);
        our_set_splevel(cfgs[c][1]);
        OurResult d=our_solve(P[0]); (void)d;
        long tg=0; int ts=0; double sl=0;
        double t0=now_ms();
        for(int q=0;q<N;q++){
            OurResult R=our_solve(P[q]);
            tg+=R.guesses; if(R.solved){ts++; sl+=log((double)R.guesses+1.0);}
        }
        double t1=now_ms();
        printf("%-6d %-6d %10.1f %10.1f %10.2f %8.2f   %s\n",
            cfgs[c][0],cfgs[c][1],t1-t0,(t1-t0)*1000.0/N,
            (double)tg/N, exp(sl/(ts?ts:1))-1, nm[c]);
    }
    return 0;
}
