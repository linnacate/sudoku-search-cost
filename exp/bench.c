/* bench.c — 墙钟时间基准: 我们 vs tdoku
 * 用法: ./bench <puzzle_file> <N>
 * 输出: 各自的 总ms / 每题us / guesses, 并交叉验证解一致
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

extern "C" {
  size_t TdokuSolverDpllTriadSimd(const char*, size_t, uint32_t, char*, size_t*);
}
extern "C" {
  typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
  void our_init(void);
  OurResult our_solve(const char*puzzle);
  void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
  void our_set_limit(int lim);
  void our_set_locked(int on);
  void our_set_splevel(int lv);
  void our_last_sol(char*out);
}

static double now_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

int main(int argc,char**argv){
    const char* fn = argc>1?argv[1]:"h2h/tdoku-master/data/puzzles5_forum_hardest_1905_11+";
    int N = argc>2?atoi(argv[2]):2000;
    FILE*f=fopen(fn,"r"); if(!f){printf("打不开 %s\n",fn);return 1;}
    char (*P)[128]= (char(*)[128])malloc(sizeof(char[128])*N);
    int n=0; char buf[512];
    while(fgets(buf,sizeof buf,f)&&n<N){
        int L=(int)strlen(buf); while(L>0&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0;
        if(L<81)continue;
        memcpy(P[n],buf,81); P[n][81]=0; n++;
    }
    fclose(f);
    N=n;
    printf("数据集: %s  N=%d\n\n",fn,N);

    our_init();
    int keys[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,keys,1);
    our_set_limit(1);
    our_set_locked(1);
    our_set_splevel(2);          /* L2 = singles + locked + naked */
    OurResult dummy = our_solve(P[0]); (void)dummy;

    /* ---- 我们 ---- */
    long tg=0; int ts=0; double sl=0;
    char *oursol = (char*)malloc((size_t)N*82);
    double t0=now_ms();
    for(int q=0;q<N;q++){
        OurResult R=our_solve(P[q]);
        if(R.solved){ts++; sl+=log((double)R.guesses+1.0);}
        tg+=R.guesses;
        if(R.solved){ our_last_sol(oursol+q*82); oursol[q*82+81]=0; }
        else oursol[q*82]=0;
    }
    double t1=now_ms();
    double our_ms=t1-t0;

    /* ---- tdoku ---- */
    long dg=0; int ds=0; double dl=0; int mismatch=0;
    char sol[128];
    double t2=now_ms();
    for(int q=0;q<N;q++){
        size_t ng=0;
        size_t c=TdokuSolverDpllTriadSimd(P[q],1,0,sol,&ng);
        if(c>0){ds++; dl+=log((double)ng+1.0);
            if(oursol[q*82] && memcmp(sol,oursol+q*82,81)!=0) mismatch++;}
        dg+=(long)ng;
    }
    double t3=now_ms();
    double td_ms=t3-t2;

    printf("%-10s %10s %10s %10s %10s %8s\n","","总ms","us/题","avg_guess","geo_guess","解出");
    printf("%-10s %10.1f %10.1f %10.2f %10.2f %8d\n","我们(L2)",
        our_ms, our_ms*1000.0/N, (double)tg/N, exp(sl/(ts?ts:1))-1, ts);
    printf("%-10s %10.1f %10.1f %10.2f %10.2f %8d\n","tdoku",
        td_ms, td_ms*1000.0/N, (double)dg/N, exp(dl/(ds?ds:1))-1, ds);
    printf("\n速度比 (我们/tdoku) = %.2fx   解不一致 = %d\n", our_ms/td_ms, mismatch);
    return 0;
}
