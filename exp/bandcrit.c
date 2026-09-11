/* bandcrit.c — band 分支的启发式对照实验
 * 问题: crit 的价值在于"二值开关"级联, 那 band 是否也有对应物?
 * 关键: cell crit[i] 与填哪个值无关(纯选格量);
 *       band 的配置一旦选定就定3格, crit 类比必然依赖配置 => 退化为值序问题
 * 启发式:
 *   H0 SizeMin      = tdoku 原生(配置数最少)
 *   H1 CritMax      = 选 (变量,配置) 使 Σcrit(3格) 最大 (纯 crit 类比)
 *   H2 SizeMinCrit  = m 最小的变量内, 配置按 Σcrit 降序
 *   H3 Random       = 随机(下界, 检验配置数是否有区分度)
 */
#define OUR_SOLVER_LIB
#include "consolidate.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    Cols A=rowcols(3*b+0,d),B=rowcols(3*b+1,d),C=rowcols(3*b+2,d); int n=0;
    for(int i=0;i<A.n;i++){int b0=A.c[i]/3;
      for(int j=0;j<B.n;j++){int b1=B.c[j]/3; if(b1==b0)continue;
        for(int k=0;k<C.n;k++){int b2=C.c[k]/3; if(b2==b0||b2==b1)continue;
          out[n][0]=A.c[i];out[n][1]=B.c[j];out[n][2]=C.c[k];n++; if(n>=64)return n;}}}
    return n;
}
static int enum_colband(int b,int d,int out[][3]){
    Cols A=colrows(3*b+0,d),B=colrows(3*b+1,d),C=colrows(3*b+2,d); int n=0;
    for(int i=0;i<A.n;i++){int b0=A.c[i]/3;
      for(int j=0;j<B.n;j++){int b1=B.c[j]/3; if(b1==b0)continue;
        for(int k=0;k<C.n;k++){int b2=C.c[k]/3; if(b2==b0||b2==b1)continue;
          out[n][0]=A.c[i];out[n][1]=B.c[j];out[n][2]=C.c[k];n++; if(n>=64)return n;}}}
    return n;
}
static int band_d_done(int v,int b,int d){
    for(int i=0;i<3;i++){ int found=0;
        if(!v){int r=3*b+i; for(int c=0;c<9;c++) if(curval[r*9+c]==d){found=1;break;}}
        else  {int c=3*b+i; for(int r=0;r<9;r++) if(curval[r*9+c]==d){found=1;break;}}
        if(!found) return 0; }
    return 1;
}
static int cfg_crit(int v,int b,int cfg[3]){
    int s=0;
    for(int i=0;i<3;i++){ int idx = v? cfg[i]*9+(3*b+i) : (3*b+i)*9+cfg[i]; s+=crit[idx]; }
    return s;
}
static int apply_cfg(int v,int b,int d,int cfg[3],long*cells){
    for(int i=0;i<3;i++){
        int idx = v? cfg[i]*9+(3*b+i) : (3*b+i)*9+cfg[i];
        if(curval[idx]==d) continue;
        if(!assign(idx,d)) return 0;
        if(cells)(*cells)++;
    }
    return 1;
}

static int HEUR=0;
static unsigned RST=12345;
static long g_mall[80];      /* 所有(v,b,d)的m分布 */
static long g_mbest[80];     /* 被选中变量的m分布 */
static long g_nvar=0, g_branch_cells=0;

static int dfs_band(void){
    if(!g_prop_clean){ if(propagate_locked()<0) return 0; g_prop_clean=1; }
    int any=0; for(int i=0;i<81;i++) if(cand[i]){any=1;break;}
    if(!any){ if(verify_sol()){ g_nsols++; if(g_nsols==1){g_ok=1;
            for(int z=0;z<81;z++)g_snap[z]=curval[z]; g_hassnap=1;}
        if(g_nsols>=SOLVE_LIMIT) return 1; return 0; } return 0; }
    compute_crit();
    int cfg[64][3];
    int best_m=1000,bb=-1,bd=-1,bv=-1,bestk=0;
    int bestscore=-1000000000;
    /* 收集候选 */
    int nv=0;
    for(int v=0;v<2;v++) for(int b=0;b<3;b++) for(int d=1;d<=9;d++){
        if(band_d_done(v,b,d)) continue;
        int n = v? enum_colband(b,d,cfg) : enum_rowband(b,d,cfg);
        if(n==0) return 0;
        if(n<80) g_mall[n]++;
        nv++;
        if(HEUR==0){                       /* SizeMin */
            if(n<best_m){best_m=n;bb=b;bd=d;bv=v;}
        }else if(HEUR==1){                 /* CritMax: 全局Σcrit最大 */
            int loc[64][3]; int m=v?enum_colband(b,d,loc):enum_rowband(b,d,loc);
            for(int k=0;k<m;k++){ int s=cfg_crit(v,b,loc[k]);
                if(s>bestscore){bestscore=s;bb=b;bd=d;bv=v;bestk=k;best_m=m;} }
        }else if(HEUR==2){                 /* SizeMin 优先, 平局取Σcrit最大 */
            int loc[64][3]; int m=v?enum_colband(b,d,loc):enum_rowband(b,d,loc);
            if(m>best_m) continue;
            for(int k=0;k<m;k++){ int s=cfg_crit(v,b,loc[k]);
                if(m<best_m || s>bestscore){ if(m<best_m)bestscore=-1000000000;
                    if(s>bestscore||m<best_m){bestscore=s;bb=b;bd=d;bv=v;bestk=k;best_m=m;} } }
        }else{                             /* Random */
            RST=RST*1103515245u+12345u; if((RST>>16)%nv==0){bb=b;bd=d;bv=v;best_m=n;}
        }
    }
    g_nvar+=nv;
    if(bb<0){ return verify_sol()?1:0; }
    if(best_m>=0&&best_m<80) g_mbest[best_m]++;
    int n = bv? enum_colband(bb,bd,cfg) : enum_rowband(bb,bd,cfg);
    if(HEUR==3){ bestk=0; }
    for(int k=0;k<n;k++){
        if(guesses>=GCAP){ CAP_HIT++; return -1; }
        int kk = (HEUR==1||HEUR==2) ? ((k==0)?bestk:k) : k;
        int mark=tlen;
        int ok=apply_cfg(bv,bb,bd,cfg[kk],&g_branch_cells);
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
static long band_solve(const char*p,int*solved,int*fill){
    our_init();
    for(int i=0;i<81;i++){ char c=p[i]; g_grid[i]=(c>='1'&&c<='9')?(c-'0'):0; }
    S_rounds=0;S_fill=0;guesses=0;g_ok=0;g_hassnap=0;tlen=0;g_nsols=0;
    g_prop_clean=0; g_branch_cells=0;
    reset_state(); wdeg_reset();
    long saved=GCAP; GCAP=3000000L;
    dfs_band();
    GCAP=saved;
    if(solved)*solved=g_hassnap?1:0;
    if(fill)*fill=S_fill;
    return guesses;
}
static long MA[80],MB[80];
int main(int argc,char**argv){
    const char*fn=argc>1?argv[1]:"sample5000.txt";
    int maxq=argc>2?atoi(argv[2]):200;
    FILE*f=fopen(fn,"r"); if(!f){fprintf(stderr,"cannot open %s\n",fn);return 1;}
    static char P[200000][84]; int N=0; char buf[512];
    while(fgets(buf,sizeof buf,f)&&N<maxq){
        if(strlen(buf)<81)continue;
        int L=(int)strlen(buf); while(L>0&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0;
        if(L>=81){ memcpy(P[N],buf,81); P[N][81]=0; N++; }
    }
    fclose(f);
    our_init();
    int keys[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,keys,1); our_set_limit(1); our_set_locked(1); our_set_splevel(2);
    long tcell=0; int scell=0;
    for(int q=0;q<N;q++){ OurResult r=our_solve(P[q]); tcell+=r.guesses; scell+=r.solved; }
    printf("=== 基线 cell 分支 (本文标准 L2) ===\n");
    printf("  solved=%d/%d  guesses均值=%.2f\n\n",scell,N,(double)tcell/N);

    const char*nm[4]={"H0 SizeMin(tdoku)","H1 CritMax","H2 SizeMin+Crit","H3 Random"};
    printf("%-22s %8s %8s %10s %10s\n","启发式","解出","guesses","BCC(w=3)","定格/guess");
    for(int h=0;h<4;h++){
        HEUR=h; RST=12345; g_nvar=0; g_branch_cells=0;
        memset(MA,0,sizeof MA); memset(MB,0,sizeof MB);
        long tg=0; int sv_tot=0; long cells=0;
        for(int q=0;q<N;q++){
            int sv=0,fl=0;
            long g=band_solve(P[q],&sv,&fl);
            tg+=g; sv_tot+=sv; cells+=g_branch_cells;
            for(int z=0;z<80;z++){MA[z]+=g_mall[z];MB[z]+=g_mbest[z];}
        }
        printf("%-22s %5d/%3d %8.2f %10.2f %10.2f\n",nm[h],sv_tot,N,(double)tg/N,
               (double)tg/N*3.0,(double)cells/(tg?tg:1));
        if(h==0){
            printf("\n  [配置数m分布] 全部(v,b,d):  ");
            for(int z=0;z<=8;z++) if(MA[z]) printf("m=%d:%.1f%%  ",z,100.0*MA[z]/(g_nvar?g_nvar:1));
            printf("\n  [被选中变量的m]            ");
            long tot=0; for(int z=0;z<80;z++) tot+=MB[z];
            for(int z=0;z<=8;z++) if(MB[z]) printf("m=%d:%.1f%%  ",z,100.0*MB[z]/(tot?tot:1));
            printf("\n  (平均候选变量数/局面 = %.1f)\n\n",(double)g_nvar/N);
        }
    }
    return 0;
}
