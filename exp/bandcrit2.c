/* bandcrit2.c — 干净版: SizeMin 选到 m 最小的变量, 但 m=min 的变量往往有多个
 * 核心问题: 在这些并列变量中, 如何选? 这就是 band 版 crit 的位置
 * 对比(都在 m=min 集合内, 只差"选哪个变量/哪个配置"):
 *   H0 First    = 枚举序第一个(当前实现, 等价 tdoku 的 tie-break)
 *   H1 CritMax  = 选 (变量,配置) 使 Σcrit(3格) 最大  [band 版 crit 类比]
 *   H2 Random   = 随机选(下界: 检验"选哪个"到底有没有价值)
 */
#define OUR_SOLVER_LIB
#include "consolidate.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct{ int n; int c[9]; } Cols;
static Cols rowcols(int r,int d){ Cols s; s.n=0;
    for(int c=0;c<9;c++) if(curval[r*9+c]==d){ s.c[s.n++]=c; return s; }
    unsigned bit=1u<<(d-1);
    for(int c=0;c<9;c++) if(cand[r*9+c]&bit) s.c[s.n++]=c; return s; }
static Cols colrows(int c,int d){ Cols s; s.n=0;
    for(int r=0;r<9;r++) if(curval[r*9+c]==d){ s.c[s.n++]=r; return s; }
    unsigned bit=1u<<(d-1);
    for(int r=0;r<9;r++) if(cand[r*9+c]&bit) s.c[s.n++]=r; return s; }
static int enum_rowband(int b,int d,int out[][3]){
    Cols A=rowcols(3*b+0,d),B=rowcols(3*b+1,d),C=rowcols(3*b+2,d); int n=0;
    for(int i=0;i<A.n;i++){int b0=A.c[i]/3;
      for(int j=0;j<B.n;j++){int b1=B.c[j]/3; if(b1==b0)continue;
        for(int k=0;k<C.n;k++){int b2=C.c[k]/3; if(b2==b0||b2==b1)continue;
          out[n][0]=A.c[i];out[n][1]=B.c[j];out[n][2]=C.c[k];n++; if(n>=64)return n;}}}
    return n; }
static int enum_colband(int b,int d,int out[][3]){
    Cols A=colrows(3*b+0,d),B=colrows(3*b+1,d),C=colrows(3*b+2,d); int n=0;
    for(int i=0;i<A.n;i++){int b0=A.c[i]/3;
      for(int j=0;j<B.n;j++){int b1=B.c[j]/3; if(b1==b0)continue;
        for(int k=0;k<C.n;k++){int b2=C.c[k]/3; if(b2==b0||b2==b1)continue;
          out[n][0]=A.c[i];out[n][1]=B.c[j];out[n][2]=C.c[k];n++; if(n>=64)return n;}}}
    return n; }
static int band_d_done(int v,int b,int d){
    for(int i=0;i<3;i++){ int f=0;
        if(!v){int r=3*b+i; for(int c=0;c<9;c++) if(curval[r*9+c]==d){f=1;break;}}
        else  {int c=3*b+i; for(int r=0;r<9;r++) if(curval[r*9+c]==d){f=1;break;}}
        if(!f) return 0; } return 1; }
static int cfg_crit(int v,int b,int cfg[3]){
    int s=0; for(int i=0;i<3;i++){ int idx=v?cfg[i]*9+(3*b+i):(3*b+i)*9+cfg[i]; s+=crit[idx]; } return s; }
static int apply_cfg(int v,int b,int d,int cfg[3],long*cells){
    for(int i=0;i<3;i++){ int idx=v?cfg[i]*9+(3*b+i):(3*b+i)*9+cfg[i];
        if(curval[idx]==d) continue;
        if(!assign(idx,d)) return 0; if(cells)(*cells)++; } return 1; }

static int HEUR=0; static unsigned RST=12345;
static long g_minm[80];      /* 每个局面 minm 的分布 */
static long g_ncand=0;       /* m=min 的变量数累加 */
static long g_branch_cells=0;
static long g_spread_sum=0,g_spread_n=0,g_cmax_sum=0,g_cmin_sum=0;
static int dfs_band(void){
    if(!g_prop_clean){ if(propagate_locked()<0) return 0; g_prop_clean=1; }
    int any=0; for(int i=0;i<81;i++) if(cand[i]){any=1;break;}
    if(!any){ if(verify_sol()){ g_nsols++; if(g_nsols==1){g_ok=1;
            for(int z=0;z<81;z++)g_snap[z]=curval[z]; g_hassnap=1;}
        if(g_nsols>=SOLVE_LIMIT) return 1; return 0; } return 0; }
    compute_crit();
    int cfg[64][3];
    int cands[64][4]; int nc=0,minm=1000;
    for(int v=0;v<2;v++) for(int b=0;b<3;b++) for(int d=1;d<=9;d++){
        if(band_d_done(v,b,d)) continue;
        int n = v? enum_colband(b,d,cfg) : enum_rowband(b,d,cfg);
        if(n==0) return 0;
        if(n<minm){ minm=n; nc=0; }
        if(n==minm && nc<64){ cands[nc][0]=v;cands[nc][1]=b;cands[nc][2]=d;cands[nc][3]=n; nc++; }
    }
    if(nc==0) return verify_sol()?1:0;
    if(minm<80) g_minm[minm]++;
    g_ncand+=nc;
    /* 诊断: 并列变量的 Σcrit 区分度 */
    if(1){ int cmin=1000000000,cmax=-1;
        for(int i=0;i<nc;i++){ int v=cands[i][0],b=cands[i][1],d=cands[i][2];
            int loc[64][3]; int m=v?enum_colband(b,d,loc):enum_rowband(b,d,loc);
            if(m!=cands[i][3]) m=cands[i][3];
            int bcfg=-1000000000;
            for(int k=0;k<m&&k<64;k++){ int s2=cfg_crit(v,b,loc[k]); if(s2>bcfg)bcfg=s2; }
            if(bcfg<cmin)cmin=bcfg; if(bcfg>cmax)cmax=bcfg; }
        if(cmax>=0){ g_spread_sum+=cmax-cmin; g_cmax_sum+=cmax; g_cmin_sum+=cmin; g_spread_n++; } }
    int ci=0, bestk=0;
    if(HEUR==1){                     /* 在并列变量中选 Σcrit 最大的(变量,配置) */
        int bs=-1000000000;
        for(int i=0;i<nc;i++){ int v=cands[i][0],b=cands[i][1],d=cands[i][2];
            int loc[64][3]; int m=v?enum_colband(b,d,loc):enum_rowband(b,d,loc);
            if(m!=cands[i][3]) m=cands[i][3];
            for(int k=0;k<m&&k<64;k++){ int s=cfg_crit(v,b,loc[k]);
                int better = (HEUR==1)?(s>bs):(s<bs);
                if(better){bs=s;ci=i;bestk=k;} } }
    }else if(HEUR==3){               /* CritMin: 反向对照, 检验方向是否错误 */
        int bs=1000000000;
        for(int i=0;i<nc;i++){ int v=cands[i][0],b=cands[i][1],d=cands[i][2];
            int loc[64][3]; int m=v?enum_colband(b,d,loc):enum_rowband(b,d,loc);
            if(m!=cands[i][3]) m=cands[i][3];
            for(int k=0;k<m&&k<64;k++){ int s=cfg_crit(v,b,loc[k]);
                if(s<bs){bs=s;ci=i;bestk=k;} } }
    }else if(HEUR==4){               /* First 变量 + crit 配置序: 只改值序, 不改变量选择 */
        ci=0; { int v=cands[0][0],b=cands[0][1],d=cands[0][2];
            int loc[64][3]; int m=v?enum_colband(b,d,loc):enum_rowband(b,d,loc);
            int bs=-1000000000;
            for(int k=0;k<m&&k<64;k++){int s2=cfg_crit(v,b,loc[k]); if(s2>bs){bs=s2;bestk=k;}} }
    }else if(HEUR==2){               /* 随机选一个并列变量 */
        RST=RST*1103515245u+12345u; ci=(RST>>16)%nc;
    }
    int bv=cands[ci][0],bb=cands[ci][1],bd=cands[ci][2];
    int n = bv? enum_colband(bb,bd,cfg) : enum_rowband(bb,bd,cfg);
    if(n<=0) return 0;
    if(n==1){                        /* m=1: 免费 assign(band 级传播) */
        int mark=tlen;
        if(!apply_cfg(bv,bb,bd,cfg[0],&g_branch_cells)){undo_to(mark);return 0;}
        if(tlen==mark){undo_to(mark);return 0;}
        if(propagate_locked()<0){undo_to(mark);return 0;}
        g_prop_clean=1; int r=dfs_band(); undo_to(mark); g_prop_clean=0; return r;
    }
    for(int k=0;k<n;k++){
        if(guesses>=GCAP){ CAP_HIT++; return -1; }
        /* 坑64: 把 bestk 提到首位时必须重排, 否则 cfg[bestk]试两次而 cfg[0]漏掉 */
        int kk = (HEUR==1||HEUR==4) ? ((k==0)?bestk:((k<=bestk)?(k-1):k)) : k;
        if(kk<0||kk>=n) kk=k;
        int mark=tlen;
        int ok=apply_cfg(bv,bb,bd,cfg[kk],&g_branch_cells);
        int filled = ok? propagate_locked():-1;
        if(filled<0){ undo_to(mark); continue; }
        g_prop_clean=1; guesses++;
        int r=dfs_band(); undo_to(mark); g_prop_clean=0;
        if(r==1)return 1; if(r==-1)return -1;
    }
    return 0;
}
static long band_solve(const char*p,int*solved,int*fill){
    our_init();
    for(int i=0;i<81;i++){ char c=p[i]; g_grid[i]=(c>='1'&&c<='9')?(c-'0'):0; }
    S_rounds=0;S_fill=0;guesses=0;g_ok=0;g_hassnap=0;tlen=0;g_nsols=0;
    g_prop_clean=0; g_branch_cells=0;
    reset_state(); wdeg_reset();
    long saved=GCAP; GCAP=3000000L; dfs_band(); GCAP=saved;
    if(solved)*solved=g_hassnap?1:0; if(fill)*fill=S_fill; return guesses;
}
int main(int argc,char**argv){
    const char*fn=argc>1?argv[1]:"sample5000.txt";
    int maxq=argc>2?atoi(argv[2]):150;
    FILE*f=fopen(fn,"r"); if(!f){fprintf(stderr,"cannot open %s\n",fn);return 1;}
    static char P[200000][84]; int N=0; char buf[512];
    while(fgets(buf,sizeof buf,f)&&N<maxq){
        if(strlen(buf)<81)continue;
        int L=(int)strlen(buf); while(L>0&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0;
        if(L>=81){ memcpy(P[N],buf,81); P[N][81]=0; N++; } }
    fclose(f);
    our_init();
    int keys[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,keys,1); our_set_limit(1); our_set_locked(1); our_set_splevel(2);
    long tcell=0; int scell=0;
    for(int q=0;q<N;q++){ OurResult r=our_solve(P[q]); tcell+=r.guesses; scell+=r.solved; }
    printf("cell 分支(本文 L2): solved=%d/%d  guesses=%.2f\n\n",scell,N,(double)tcell/N);
    const char*nm[5]={"H0 First(枚举序)","H1 CritMax(变量+配置)","H2 Random(随机)","H3 CritMin(反向对照)","H4 First变量+crit配置序"};
    printf("%-30s %9s %10s %10s\n","启发式","解出","guesses","BCC(w=3)");
    for(int h=0;h<5;h++){
        HEUR=h; RST=12345; g_ncand=0; g_branch_cells=0;
        long MM[80]; memset(MM,0,sizeof MM);
        long tg=0; int sv=0;
        for(int q=0;q<N;q++){
            memset(g_minm,0,sizeof g_minm);
            int s=0,fl=0; long g=band_solve(P[q],&s,&fl); tg+=g; sv+=s;
            for(int z=0;z<80;z++) MM[z]+=g_minm[z];
        }
        printf("%-30s %5d/%3d %10.2f %10.2f\n",nm[h],sv,N,(double)tg/N,(double)tg/N*3.0);
        if(h==0){
            long tot=0; for(int z=0;z<80;z++) tot+=MM[z];
            printf("\n  minm(被选变量的配置数)分布: ");
            for(int z=0;z<=6;z++) if(MM[z]) printf("m=%d:%.2f%%  ",z,100.0*MM[z]/(tot?tot:1));
            printf("\n  并列变量数(m=min的变量个数)/局面 = %.2f\n",(double)g_ncand/(tot?tot:1));
            printf("  => 若该值≈1 则并列无选择空间; 若>>1 则\"选哪个\"有价值\n");
            printf("  并列变量 Σcrit: 最大均值=%.2f 最小均值=%.2f 极差均值=%.2f (n=%ld)\n\n",
                   (double)g_cmax_sum/(g_spread_n?g_spread_n:1),(double)g_cmin_sum/(g_spread_n?g_spread_n:1),
                   (double)g_spread_sum/(g_spread_n?g_spread_n:1),g_spread_n);
        }
    }
    return 0;
}
