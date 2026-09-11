/* bandbranch.c — 对齐 tdoku 的 band-digit 分支粒度实验
 * 一次 guess = 数字 d 在一个 band(行band或列band) 中的一个完整配置 = 3 格
 * 目的: (1) 与 cell 分支(1格) 在 BCC 口径下比较
 *       (2) 与 tdoku 原生 guess 直接比较(同粒度)
 *       (3) 检验 band 级"配置唯一"是否是一个可吸收的新传播技术
 */
#define OUR_SOLVER_LIB
#include "consolidate.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int BAND_PROP=1;          /* 1=配置数==1 时免费 assign (band 级传播) */
static long g_band_prop_hits=0;
static long g_branch_cells=0;      /* 分支决策实际填入的格数(精确BCC) */
static long g_mdist[80];           /* best_m 分布 */

typedef struct{ int n; int c[9]; } Cols;

static Cols rowcols(int r,int d){
    Cols s; s.n=0;
    for(int c=0;c<9;c++) if(curval[r*9+c]==d){ s.c[s.n++]=c; return s; }
    unsigned bit=1u<<(d-1);
    for(int c=0;c<9;c++) if(cand[r*9+c]&bit) s.c[s.n++]=c;
    return s;
}
static Cols colrows(int c,int d){
    Cols s; s.n=0;
    for(int r=0;r<9;r++) if(curval[r*9+c]==d){ s.c[s.n++]=r; return s; }
    unsigned bit=1u<<(d-1);
    for(int r=0;r<9;r++) if(cand[r*9+c]&bit) s.c[s.n++]=r;
    return s;
}
static int enum_rowband(int b,int d,int out[][3]){
    Cols A=rowcols(3*b+0,d), B=rowcols(3*b+1,d), C=rowcols(3*b+2,d);
    int n=0;
    for(int i=0;i<A.n;i++){ int b0=A.c[i]/3;
      for(int j=0;j<B.n;j++){ int b1=B.c[j]/3; if(b1==b0)continue;
        for(int k=0;k<C.n;k++){ int b2=C.c[k]/3; if(b2==b0||b2==b1)continue;
          out[n][0]=A.c[i];out[n][1]=B.c[j];out[n][2]=C.c[k];n++; if(n>=64)return n; }}}
    return n;
}
static int enum_colband(int b,int d,int out[][3]){
    Cols A=colrows(3*b+0,d), B=colrows(3*b+1,d), C=colrows(3*b+2,d);
    int n=0;
    for(int i=0;i<A.n;i++){ int b0=A.c[i]/3;
      for(int j=0;j<B.n;j++){ int b1=B.c[j]/3; if(b1==b0)continue;
        for(int k=0;k<C.n;k++){ int b2=C.c[k]/3; if(b2==b0||b2==b1)continue;
          out[n][0]=A.c[i];out[n][1]=B.c[j];out[n][2]=C.c[k];n++; if(n>=64)return n; }}}
    return n;
}

/* (v,b,d) 是否已完全确定: 该 band 的 3 条线中每条都已填入 d */
static int band_d_done(int v,int b,int d){
    for(int i=0;i<3;i++){
        int found=0;
        if(!v){ int r=3*b+i; for(int c=0;c<9;c++) if(curval[r*9+c]==d){found=1;break;} }
        else  { int c=3*b+i; for(int r=0;r<9;r++) if(curval[r*9+c]==d){found=1;break;} }
        if(!found) return 0;
    }
    return 1;
}

static int apply_cfg(int v,int b,int d,int cfg[3]){
    for(int i=0;i<3;i++){
        int idx = v ? cfg[i]*9+(3*b+i) : (3*b+i)*9+cfg[i];
        if(curval[idx]==d) continue;
        if(!assign(idx,d)) return 0;
        g_branch_cells++;
    }
    return 1;
}
static int dfs_band(void){
    if(!g_prop_clean){ if(propagate_locked()<0) return 0; g_prop_clean=1; }
    int any=0; for(int i=0;i<81;i++) if(cand[i]){any=1;break;}
    if(!any){ if(verify_sol()){ g_nsols++; if(g_nsols==1){g_ok=1;
            for(int z=0;z<81;z++)g_snap[z]=curval[z]; g_hassnap=1;}
        if(g_nsols>=SOLVE_LIMIT) return 1; return 0; } return 0; }
    int cfg[64][3];
    int best_m=1000,bb=-1,bd=-1,bv=-1;
    for(int v=0;v<2;v++) for(int b=0;b<3;b++) for(int d=1;d<=9;d++){
        if(band_d_done(v,b,d)) continue;          /* 已完全确定, 不参与分支 */
        int n = v? enum_colband(b,d,cfg) : enum_rowband(b,d,cfg);
        if(n==0) return 0;
        if(n<best_m){ best_m=n; bb=b; bd=d; bv=v; }
    }
    if(bb<0){ return verify_sol()?1:0; }
    if(best_m>=0&&best_m<80) g_mdist[best_m]++;
    int n = bv? enum_colband(bb,bd,cfg) : enum_rowband(bb,bd,cfg);
    if(BAND_PROP && n<=1){
        if(n!=1) return 0;
        int mark=tlen;
        if(!apply_cfg(bv,bb,bd,cfg[0])){ undo_to(mark); return 0; }
        if(tlen==mark){ undo_to(mark); return 0; }   /* 无推进 => 防无限递归 */
        if(propagate_locked()<0){ undo_to(mark); return 0; }
        g_prop_clean=1; g_band_prop_hits++;
        int r=dfs_band();
        undo_to(mark); g_prop_clean=0;
        return r;
    }
    for(int k=0;k<n;k++){
        if(guesses>=GCAP){ CAP_HIT++; return -1; }
        int mark=tlen;
        int ok=apply_cfg(bv,bb,bd,cfg[k]);
        int filled = ok? propagate_locked():-1;
        if(filled<0){ undo_to(mark); continue; }
        g_prop_clean=1; guesses++;
        int r=dfs_band();
        undo_to(mark); g_prop_clean=0;
        if(r==1)return 1;
        if(r==-1)return -1;
    }
    return 0;
}
/* 返回 guesses; *solved, *fill, *prophits 出参 */
static long band_solve(const char*p,int*solved,int*fill,long*ph){
    our_init();
    for(int i=0;i<81;i++){ char c=p[i]; g_grid[i]=(c>='1'&&c<='9')?(c-'0'):0; }
    S_rounds=0;S_fill=0;guesses=0;g_ok=0;g_hassnap=0;tlen=0;g_nsols=0;
    g_prop_clean=0; g_band_prop_hits=0; g_branch_cells=0;
    reset_state(); wdeg_reset();
    long saved=GCAP; GCAP=5000000L;
    dfs_band();
    GCAP=saved;
    if(solved)*solved=g_hassnap?1:0;
    if(fill)*fill=S_fill;
    if(ph)*ph=g_band_prop_hits;
    return guesses;
}

static long MD[80];
int main(int argc,char**argv){
    const char*fn = argc>1?argv[1]:"sample5000.txt";
    int maxq = argc>2?atoi(argv[2]):200;
    FILE*f=fopen(fn,"r"); if(!f){fprintf(stderr,"cannot open %s\n",fn);return 1;}
    static char P[200000][84]; int N=0; char buf[512];
    while(fgets(buf,sizeof buf,f)&&N<maxq){
        if(strlen(buf)<81)continue;
        int L=(int)strlen(buf); while(L>0&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0;
        if(L>=81){ memcpy(P[N],buf,81); P[N][81]=0; N++; }
    }
    fclose(f);
    fprintf(stderr,"N=%d file=%s\n",N,fn);
    our_init();
    int keys[5]={0,1,2,3,4};
    long tc1=0,tc0=0,tb1=0,tb0=0; int sc1=0,sc0=0,sb1=0,sb0=0; long ph=0;
    long bcells=0;
    int diffcnt=0;
    char refsol[84],bsol[84];
    for(int q=0;q<N;q++){
        fprintf(stderr,"q=%d cell1...\n",q);
        /* cell 分支, la0opt=1 */
        our_set_strategy(10,1,5,keys,1); our_set_limit(1); our_set_locked(1); our_set_splevel(2);
        OurResult r=our_solve(P[q]); tc1+=r.guesses; sc1+=r.solved;
        our_last_sol(refsol); refsol[81]=0;
        fprintf(stderr,"  cell0...\n");
        our_set_strategy(10,1,5,keys,0);
        OurResult r0=our_solve(P[q]); tc0+=r0.guesses; sc0+=r0.solved;
        fprintf(stderr,"  band1...\n");
        our_set_strategy(10,1,5,keys,0);
        BAND_PROP=1; int sv=0,fl=0; long h=0;
        long g1=band_solve(P[q],&sv,&fl,&h); tb1+=g1; sb1+=sv; ph+=h; bcells+=g_branch_cells;
        for(int z=0;z<80;z++) MD[z]+=g_mdist[z];
        our_last_sol(bsol); bsol[81]=0;
        if(sv && strncmp(refsol,bsol,81)!=0) diffcnt++;
        /* band 分支, 无 band 级传播 */
        fprintf(stderr,"  band0...\n"); BAND_PROP=0; sv=0;
        long g0=band_solve(P[q],&sv,&fl,&h); tb0+=g0; sb0+=sv;
    }
    printf("N=%d  solved: cell(la=1)=%d cell(la=0)=%d band(prop)=%d band(noprop)=%d\n",
           N,sc1,sc0,sb1,sb0);
    printf("解不一致(band vs cell) = %d\n",diffcnt);
    printf("band 级传播命中总数 = %ld\n",ph);
    printf("\n%-28s %10s %10s\n","配置","总guesses","均值");
    printf("%-28s %10ld %10.2f\n","cell 分支 la=1 (标准L2)",tc1,(double)tc1/N);
    printf("%-28s %10ld %10.2f\n","cell 分支 la=0",tc0,(double)tc0/N);
    printf("%-28s %10ld %10.2f\n","band 分支 +band传播",tb1,(double)tb1/N);
    printf("%-28s %10ld %10.2f\n","band 分支 无band传播",tb0,(double)tb0/N);
    printf("\nbest_m(最少配置数)分布: ");
    for(int z=0;z<=8;z++) if(MD[z]) printf("[m=%d]%ld ",z,MD[z]);
    printf("\n分支决策实际填入格数(band) = %ld  => 精确BCC=%.2f/题  (每次guess平均定%.2f格)\n",
           bcells,(double)bcells/N,(double)bcells/(tb1?tb1:1));
    printf("\n[BCC 归一化]  cell: w=1   band: w=3 (一次配置定3格)\n");
    printf("%-28s %10s\n","配置","平均BCC");
    printf("%-28s %10.2f\n","cell 分支 la=1",(double)tc1/N);
    printf("%-28s %10.2f\n","cell 分支 la=0",(double)tc0/N);
    printf("%-28s %10.2f\n","band 分支 +band传播",(double)tb1/N*3.0);
    printf("%-28s %10.2f\n","band 分支 无band传播",(double)tb0/N*3.0);
    return 0;
}
