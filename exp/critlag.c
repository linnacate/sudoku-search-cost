/* crit 时效性衰减曲线 driver
 * 用法: ./critlag <题库> <N> [maxlag]
 * 每题对 LAG=0..maxlag 各求解一次, 输出 guesses 曲线 + 逐题配对符号检验
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
extern void our_init(void);
extern OurResult our_solve(const char*);
extern void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
extern void our_set_locked(int on);
extern void our_set_splevel(int lv);
extern void our_set_limit(int lim);
extern void our_set_lag(int k);
extern void our_set_lagmin(int m);
extern void our_lag_diag(long*use,long*raw);
extern void our_lag_diag_reset(void);

#define MAXL 8
int main(int argc,char**argv){
    if(argc<3){ fprintf(stderr,"用法: %s <题库> <N> [maxlag]\n",argv[0]); return 1; }
    int N=atoi(argv[2]);
    int ML=argc>4?atoi(argv[4]):0; (void)ML;
    int maxlag=argc>3?atoi(argv[3]):6;
    int lagmin=argc>4?atoi(argv[4]):0;
    our_set_lagmin(lagmin);
    if(maxlag>MAXL-1)maxlag=MAXL-1;

    FILE*f=fopen(argv[1],"r"); if(!f){perror(argv[1]);return 1;}
    char**rows=malloc(sizeof(char*)*(N+10)); int nq=0; char line[256];
    while(fgets(line,sizeof line,f) && nq<N){
        int l=(int)strcspn(line,"\r\n"); line[l]=0;
        if(l<81) continue;
        rows[nq++]=strdup(line);
    }
    fclose(f);
    if(N>nq)N=nq;

    int full[5]={0,1,2,3,4};
    our_init(); our_set_limit(1); our_set_locked(1); our_set_splevel(2);
    our_set_strategy(10,1,5,full,1);
    if(lagmin>0) our_set_lagmin(lagmin);

    static long G[MAXL][200000];
    static double LG[MAXL][200000];
    long T[MAXL]={0}; double SL[MAXL]={0}; int solved=0;
    static long DU[MAXL],DR[MAXL];
    for(int k=0;k<=maxlag;k++){
        our_set_lag(k);
        our_lag_diag_reset();
        long cnt=0;
        for(int q=0;q<N;q++){
            OurResult R=our_solve(rows[q]);
            if(!R.solved){ G[k][q]=-1; continue; }
            G[k][q]=R.guesses; LG[k][q]=log((double)R.guesses+1.0);
            T[k]+=R.guesses; SL[k]+=log((double)R.guesses+1.0);
            cnt++;
        }
        if(k==0) solved=(int)cnt;
        our_lag_diag(&DU[k],&DR[k]);
        fprintf(stderr,"  lag=%d done\n",k);
    }
    printf("\n[solved %d/%d]  完整配方 5键+FILT+la0opt, locked=1, sp=2, limit=1\n",solved,N);
    printf("[lagmin=%d]\n",lagmin);
    printf("\n%-8s %12s %12s %10s %14s\n","LAG","avg_guesses","geo_mean","相对LAG0","配对 net(w-l)");
    for(int k=0;k<=maxlag;k++){
        if(solved==0)break;
        double am=(double)T[k]/solved, gm=exp(SL[k]/solved)-1.0;
        long w=0,l=0,t=0;
        if(k>0){ for(int q=0;q<N;q++){
            if(G[k][q]<0||G[0][q]<0) continue;
            if(G[k][q]>G[0][q]) l++; else if(G[k][q]<G[0][q]) w++; else t++; } }
        printf("LAG=%-4d %12.4f %12.4f %9.4f",k,am,gm,am/((double)T[0]/solved));
        if(k>0){
            double net=(double)(w-l);
            double sd=sqrt((double)(w+l));
            printf("   %+ld (w%ld/l%ld/t%ld) z=%.2f",(long)net,w,l,t,sd>0?net/sd:0.0);
        }
        { if(k>0) printf("   [stale层 %ld / raw层 %ld]",DU[k],DR[k]); }
        printf("\n");
    }
    printf("\n注: LAG=k 表示第 d 层选格时用第 d-k 层传播后的 crit 快照.\n");
    printf("    LAG=0 = 标准(每层重算). z 为配对符号检验正态近似.\n");
    return 0;
}
