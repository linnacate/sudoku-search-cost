/* hcost2.c — 测试两种"少擦除"策略对人类步骤的影响
 *   A. 修正痕迹 (TrailSaving / Conflict-Driven Backjumping 的简化版):
 *      矛盾时回退到【实际冲突相关的层】, 而不是只回退一层
 *   B. 记忆化剪枝: 记住已知矛盾的 (格,值), 跳过
 * 编译: gcc -O3 -march=native -o hcost2 hcost2.c -lm
 * 用法: ./hcost2 file [N] [MODE]   MODE 0=基线 1=记忆化 2=记忆化+回跳
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define ALL 0x1FFu
#define MAXT 4000000
#define MAXK 8
static long GCAP=200000L;
static const int ROWOF[81]={0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8};
static const int COLOF[81]={0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8};
static const int BOXOF[81]={0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,3,3,3,4,4,4,5,5,5,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,6,6,6,7,7,7,8,8,8,6,6,6,7,7,7,8,8,8};
static int UNITS[27][9], PEERS[81][20], NPEER[81];
static void init_tables(void){
    for(int r=0;r<9;r++)for(int c=0;c<9;c++)UNITS[r][c]=r*9+c;
    for(int c=0;c<9;c++)for(int r=0;r<9;r++)UNITS[9+c][r]=r*9+c;
    for(int b=0;b<9;b++){int k=0;for(int i=0;i<81;i++)if(BOXOF[i]==b)UNITS[18+b][k++]=i;}
    for(int i=0;i<81;i++){char seen[81]={0};int n=0;
        for(int u=0;u<27;u++){int inU=0;for(int k=0;k<9;k++)if(UNITS[u][k]==i){inU=1;break;}
            if(!inU)continue;
            for(int k=0;k<9;k++){int j=UNITS[u][k];if(j!=i&&!seen[j]){seen[j]=1;PEERS[i][n++]=j;}}}
        NPEER[i]=n;}
}
static long S_rounds,S_naked,S_hidden,S_tryfill,S_erase_cells,S_del_cand;
static long S_deadend,S_guess,S_pruned,S_maxdepth,S_sumdepth;
static int cur_depth;
static unsigned cand[81],rowm[9],colm[9],boxm[9];
typedef struct{short idx;unsigned oldcand;short oldr,oldc,oldb;char iscell;}Trail;
static Trail trail[MAXT]; static int tlen=0; static int g_grid[81];
static void reset_state(void){
    tlen=0; memset(rowm,0,sizeof(rowm));memset(colm,0,sizeof(colm));memset(boxm,0,sizeof(boxm));
    for(int i=0;i<81;i++)if(g_grid[i]){unsigned b=1u<<(g_grid[i]-1);cand[i]=0;
        rowm[ROWOF[i]]|=b;colm[COLOF[i]]|=b;boxm[BOXOF[i]]|=b;}
    for(int i=0;i<81;i++)if(!g_grid[i])cand[i]=ALL&~(rowm[ROWOF[i]]|colm[COLOF[i]]|boxm[BOXOF[i]]);
}
static inline int undo_to(int mark){
    int ncells=0,ndels=0;
    while(tlen>mark){Trail*t=&trail[--tlen];cand[t->idx]=t->oldcand;
        if(t->iscell){rowm[ROWOF[t->idx]]=t->oldr;colm[COLOF[t->idx]]=t->oldc;boxm[BOXOF[t->idx]]=t->oldb;ncells++;}
        else ndels++;}
    S_erase_cells+=ncells; S_del_cand+=ndels;
    return ncells;
}
static short conflict_cell;  /* 矛盾发生的格 (-1=无) */
static inline int assign(int i,int d){
    unsigned bit=1u<<(d-1); Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];t->iscell=1;
    cand[i]=0; rowm[ROWOF[i]]|=bit;colm[COLOF[i]]|=bit;boxm[BOXOF[i]]|=bit;
    for(int k=0;k<NPEER[i];k++){int j=PEERS[i][k];
        if(cand[j]&bit){Trail*u=&trail[tlen++];u->idx=j;u->oldcand=cand[j];u->iscell=0;
            cand[j]&=~bit; if(cand[j]==0){conflict_cell=j; return 0;}}}
    return 1;
}
static int propagate(void){
    int progress=1,filled=0;
    while(progress){progress=0;
        S_rounds++;
        for(int i=0;i<81;i++){unsigned c=cand[i];
            if(c&&(c&(c-1))==0){S_naked++; if(!assign(i,__builtin_ctz(c)+1))return -1;filled++;progress=1;}}
        for(int uid=0;uid<27;uid++){unsigned once=0,twice=0,all=0;
            for(int k=0;k<9;k++){unsigned c=cand[UNITS[uid][k]];if(!c)continue;twice|=once&c;once|=c;all|=c;}
            unsigned placed=uid<9?rowm[uid]:(uid<18?colm[uid-9]:boxm[uid-18]);
            if((all|placed)!=ALL)return -1;
            unsigned hid=once&~twice&~placed;
            while(hid){unsigned bit=hid&(~hid+1);hid^=bit;
                int tgt=-1,cnt=0;
                for(int k=0;k<9;k++){int i=UNITS[uid][k];if(cand[i]&bit){tgt=i;cnt++;}}
                if(cnt==1){S_hidden++; if(!assign(tgt,__builtin_ctz(bit)+1))return -1;filled++;progress=1;}}}}
    return filled;
}
static short freed[27][9]; static int crit[81];
static void compute_freed(void){
    for(int uid=0;uid<27;uid++){unsigned base=uid<9?rowm[uid]:(uid<18?colm[uid-9]:boxm[uid-18]);
        for(int d=0;d<9;d++){if(base&(1u<<d)){freed[uid][d]=-1;continue;}
            unsigned bit=1u<<d;int c=0;
            for(int k=0;k<9;k++)if(cand[UNITS[uid][k]]&bit)c++;
            freed[uid][d]=(short)c;}}
}
static void compute_crit(void){
    for(int i=0;i<81;i++)crit[i]=0;
    for(int uid=0;uid<27;uid++)for(int d=0;d<9;d++){
        if(freed[uid][d]!=2)continue; unsigned bit=1u<<d;
        for(int k=0;k<9;k++){int i=UNITS[uid][k];if(cand[i]&bit)crit[i]++;}}
}
static double keyval(int i,int k){
    unsigned cc=cand[i]; int np=__builtin_popcount(cc);
    int u0=ROWOF[i],u1=COLOF[i]+9,u2=BOXOF[i]+18;
    switch(k){
        case 0: return (double)np;
        case 1: return (double)crit[i];
        case 7: { int n=0; for(int k2=0;k2<NPEER[i];k2++) if(cand[PEERS[i][k2]])n++; return (double)n; }
        case 5: { double p=1; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    p*=(freed[u0][d]>0?freed[u0][d]:1); p*=(freed[u1][d]>0?freed[u1][d]:1);
                    p*=(freed[u2][d]>0?freed[u2][d]:1);} return p; }
        case 17:{ double s=0; for(int k2=0;k2<NPEER[i];k2++){int j=PEERS[i][k2]; if(cand[j])s+=crit[j];} return s; }
    }
    return 0;
}
static int FILT,NKEY,KSEL[MAXK]; static double KSIGN[MAXK],KV[81][MAXK];
static void compute_all(int i){ for(int k=0;k<NKEY;k++) KV[i][k]=KSIGN[k]*keyval(i,KSEL[k]); }
static int pick(void){
    compute_freed(); compute_crit();
    int pool[81],pn=0;
    for(int i=0;i<81;i++){ if(!cand[i])continue; if(FILT==1&&crit[i]<=0)continue; pool[pn++]=i; }
    if(pn==0){ for(int i=0;i<81;i++) if(cand[i]) pool[pn++]=i; if(pn==0)return -1; }
    for(int k=0;k<pn;k++) compute_all(pool[k]);
    int best=pool[0];
    for(int k=1;k<pn;k++){ int i=pool[k]; int better=0;
        for(int q=0;q<NKEY;q++){ if(KV[i][q]>KV[best][q]){better=1;break;} if(KV[i][q]<KV[best][q]){better=0;break;} }
        if(better)best=i; }
    return best;
}
static void order_vals(int i,int*ds,int nd){
    if(nd<=1)return; int w[9];
    for(int k=0;k<nd;k++){ unsigned bit=1u<<(ds[k]-1); int c=0;
        for(int p=0;p<NPEER[i];p++)if(cand[PEERS[i][p]]&bit)c++; w[k]=c; }
    for(int a=1;a<nd;a++){int kd=ds[a],kw=w[a],b=a-1;
        while(b>=0&&w[b]<kw){ds[b+1]=ds[b];w[b+1]=w[b];b--;} ds[b+1]=kd;w[b+1]=kw;}
}
static long guesses;
static int MODE;
static unsigned char dead[81][10];   /* 记忆化: 已知矛盾的 (格,值) */
static int gcell[256], gval[256], gmark[256];  /* 每层: 猜测的格/值/trail标记 */
static int verify_sol(void){
    for(int i=0;i<81;i++) if(cand[i]) return 0;
    for(int u=0;u<27;u++){ unsigned p=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]); if(p!=ALL) return 0; }
    return 1;
}
static int g_verify_ok;
static int dfs(void){
    if(propagate()<0)return 0;
    int any=0; for(int i=0;i<81;i++)if(cand[i]){any=1;break;}
    if(!any){ g_verify_ok=verify_sol(); return 1; }
    cur_depth++; if(cur_depth>S_maxdepth)S_maxdepth=cur_depth; S_sumdepth+=cur_depth;
    int i=pick(); if(i<0){cur_depth--;return 1;}
    unsigned cc=cand[i]; int ds[9],nd=0;
    for(int d=1;d<=9;d++)if(cc&(1u<<(d-1)))ds[nd++]=d;
    order_vals(i,ds,nd);
    int real=-1;   /* 本次递归最终成功的分支 */
    for(int k=0;k<nd;k++){
        if(guesses>=GCAP){cur_depth--;return -1;}
        if(MODE>=1 && dead[i][ds[k]]) continue;   /* 记忆化跳过 */
        int mark0=tlen;
        S_tryfill++; conflict_cell=-1;
        int ok=assign(i,ds[k]);
        int filled=ok?propagate():-1;
        if(filled<0){
            S_pruned++; S_deadend++;
            if(MODE>=1) dead[i][ds[k]]=1;   /* 记住 */
            undo_to(mark0);
            continue;
        }
        guesses++;
        gcell[cur_depth]=i; gval[cur_depth]=ds[k]; gmark[cur_depth]=mark0;
        int r=dfs();
        undo_to(mark0);
        if(r==1){cur_depth--; return 1;}
        if(r==-1){cur_depth--; return -1;}
        if(MODE>=1) dead[i][ds[k]]=1;
    }
    cur_depth--;
    return 0;
}
static const struct{int filt;int n;int ks[5];double sg[5];}SD[1]={
 {1,5,{1,0,7,5,17},{-1,-1,1,-1,-1}},
};
int main(int argc,char**argv){
    if(argc<2){return 1;}
    int NQ=argc>2?atoi(argv[2]):300;
    MODE=argc>3?atoi(argv[3]):0;
    init_tables();
    FILE*f=fopen(argv[1],"r"); if(!f)return 1;
    int (*grids)[81]=malloc(sizeof(int[81])*(NQ+10));
    int nq=0; char line[512];
    while(fgets(line,sizeof(line),f)&&nq<NQ){
        int n=0;
        for(char*p=line;*p&&n<81;p++){
            if(*p>='1'&&*p<='9')grids[nq][n++]=*p-'0';
            else if(*p=='.'||*p=='0')grids[nq][n++]=0;}
        if(n==81)nq++;
    }
    fclose(f);
    FILT=SD[0].filt; NKEY=SD[0].n;
    for(int i=0;i<NKEY;i++){KSEL[i]=SD[0].ks[i];KSIGN[i]=SD[0].sg[i];}
    long TR=0,TS=0,TH=0,TF=0,TE=0,TD2=0,TP=0,TG=0,TSUMD=0; int TMAXD=0;
    int solved=0;
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
        reset_state();
        S_rounds=S_naked=S_hidden=S_tryfill=S_erase_cells=S_del_cand=0;
        S_deadend=S_guess=S_pruned=S_maxdepth=S_sumdepth=0;
        guesses=0; cur_depth=0; g_verify_ok=-1;
        memset(dead,0,sizeof(dead));
        srand(20260904u+q);
        dfs();
        if(g_verify_ok==1)solved++;
        TR+=S_rounds;TS+=S_naked;TH+=S_hidden;TF+=S_tryfill;TE+=S_erase_cells;
        TD2+=S_del_cand;TP+=S_pruned;TG+=guesses;TSUMD+=S_sumdepth;
        if(S_maxdepth>TMAXD)TMAXD=S_maxdepth;
    }
    double N=(double)nq;
    printf("MODE=%d N=%d 解出=%d | 假设=%.1f 人试填=%.1f (剪%.1f) 传播轮=%.1f 擦除格=%.1f 候选删=%.1f 均深=%.1f 最深=%d | 时间=%.2fh\n",
        MODE,nq,solved,TG/N,TF/N,TP/N,TR/N,TE/N,TD2/N,TSUMD/N/fmax(TG/N,1.0),TMAXD,
        ((TR/N)*45+(TF/N)*8+(TE/N)*12)/3600);
    return 0;
}
