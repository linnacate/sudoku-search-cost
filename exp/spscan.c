/* spscan.c — 各强传播技术的耗时拆解
 * splevel: 0=singles 1=+locked 2=+naked 3=+hidden 4=+xwing
 * 用法: ./spscan <puzzles> <N>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
extern "C" {
  typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
  void our_init(void);
  OurResult our_solve(const char*puzzle);
  void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
  void our_set_limit(int lim);
  void our_set_locked(int on);
  void our_set_splevel(int lv);
}
static double now_ms(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1000.0+ts.tv_nsec/1e6;}
int main(int argc,char**argv){
    const char*fn=argc>1?argv[1]:"h2h/tdoku-master/data/puzzles5_forum_hardest_1905_11+";
    int N=argc>2?atoi(argv[2]):2000;
    FILE*f=fopen(fn,"r"); if(!f){printf("打不开\n");return 1;}
    char(*P)[128]=(char(*)[128])malloc(sizeof(char[128])*N);
    int n=0; char buf[512];
    while(fgets(buf,sizeof buf,f)&&n<N){int L=(int)strlen(buf);
        while(L>0&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0;
        if(L<81)continue; memcpy(P[n],buf,81);P[n][81]=0;n++;}
    fclose(f); N=n;
    our_init();
    int keys[5]={0,1,2,3,4};
    printf("%-8s %10s %10s %10s %10s\n","级别","us/题","avg_guess","geo","增量us");
    double prev=0;
    const char*nm[5]={"L0 singles","L1 +locked","L2 +naked","L3 +hidden","L4 +xwing"};
    for(int lv=0;lv<=4;lv++){
        our_set_strategy(10,1,5,keys,1);
        our_set_limit(1); our_set_locked(lv>=1?1:0); our_set_splevel(lv);
        OurResult d=our_solve(P[0]); (void)d;
        long tg=0; double sl=0; int ts=0;
        double t0=now_ms();
        for(int q=0;q<N;q++){OurResult R=our_solve(P[q]); tg+=R.guesses; if(R.solved){ts++;sl+=(double)(R.guesses+1);}}
        double el=now_ms()-t0;
        printf("%-8s %10.1f %10.2f %10.2f %10s\n",nm[lv],el*1000.0/N,(double)tg/N,sl/(ts?ts:1)-1,
               lv? (el*1000.0/N-prev>0?"+":""):"");
        if(lv)printf("           (相对上一级别 %+.1f us)\n", el*1000.0/N-prev);
        prev=el*1000.0/N;
    }
    return 0;
}
