/* hcost.c — 把机器代价换算成【人类决策步骤】
 *
 * 问题: 当前最佳策略 ~29 次假设(机器口径), 距"手算"还差多远?
 *
 * 关键: 人的原子操作与机器不同
 *   - 机器"假设"是 O(1) 的内存写入
 *   - 人"试填+传播+发现矛盾+擦除"是一整套昂贵流程
 *   - la0opt 的"前瞻剪枝"对人来说【等同于一次正式假设】(2.18 已指出)
 *     -> 人的试填次数 = counted_guesses + pruned_lookaheads
 *
 * 统计:
 *   1. 传播轮数 R (人的"全盘扫描"次数) —— 最贵的部分
 *   2. naked / hidden single 分别触发多少次
 *   3. 试填总次数 (含被剪)
 *   4. 擦除格数 (回溯代价)
 *   5. 最大/平均 搜索深度
 *   6. 候选盘的"总维护量" = 每次删除候选的次数
 *
 * 编译: gcc -O3 -march=native -o hcost hcost.c -lm
 * 用法: ./hcost puzzles.txt [N] [策略] [LA]
 *   策略: 0=新组合(默认) 1=MRV 2=critmrv
 *   LA:   0=无剪枝 1=la0opt(默认)
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
static const int COLOF[81]={0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8};
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
static unsigned cand[81],rowm[9],colm[9],boxm[9];
typedef struct{short idx;unsigned oldcand;short oldr,oldc,oldb;char iscell;}Trail;
static Trail trail[MAXT]; static int tlen=0; static int g_grid[81];
static void reset_state(void){
    tlen=0; memset(rowm,0,sizeof(rowm));memset(colm,0,sizeof(colm));memset(boxm,0,sizeof(boxm));
    for(int i=0;i<81;i++)if(g_grid[i]){unsigned b=1u<<(g_grid[i]-1);cand[i]=0;
        rowm[ROWOF[i]]|=b;colm[COLOF[i]]|=b;boxm[BOXOF[i]]|=b;}
    for(int i=0;i<81;i++)if(!g_grid[i])cand[i]=ALL&~(rowm[ROWOF[i]]|colm[COLOF[i]]|boxm[BOXOF[i]]);
}
/* ===== 统计 ===== */
static long S_rounds, S_naked, S_hidden, S_tryfill, S_erase_cells;
static long S_maxcasc;
static long S_del_cand, S_deadend, S_guess, S_pruned, S_erase_guesses;
static long S_maxdepth, S_sumdepth, S_nodes;
static int cur_depth;

static inline int undo_to(int mark){
    int ncells=0, ndels=0;
    while(tlen>mark){Trail*t=&trail[--tlen];cand[t->idx]=t->oldcand;
        if(t->iscell){rowm[ROWOF[t->idx]]=t->oldr;colm[COLOF[t->idx]]=t->oldc;boxm[BOXOF[t->idx]]=t->oldb;ncells++;}
        else ndels++;}
    S_erase_cells+=ncells; S_del_cand+=ndels;
    return ncells;
}
static inline int assign(int i,int d){
    unsigned bit=1u<<(d-1); Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];t->iscell=1;
    cand[i]=0; rowm[ROWOF[i]]|=bit;colm[COLOF[i]]|=bit;boxm[BOXOF[i]]|=bit;
    for(int k=0;k<NPEER[i];k++){int j=PEERS[i][k];
        if(cand[j]&bit){Trail*u=&trail[tlen++];u->idx=j;u->oldcand=cand[j];u->iscell=0;
            cand[j]&=~bit; if(cand[j]==0)return 0;}}
    return 1;
}
static int propagate(void){
    int progress=1,filled=0,myf=0;
    while(progress){progress=0;
        S_rounds++;   /* 人的一次"全盘扫描" */
        for(int i=0;i<81;i++){unsigned c=cand[i];
            if(c&&(c&(c-1))==0){S_naked++; if(!assign(i,__builtin_ctz(c)+1))return -1;filled++;myf++;progress=1;}}
        for(int uid=0;uid<27;uid++){
            unsigned once=0,twice=0,all=0;
            for(int k=0;k<9;k++){unsigned c=cand[UNITS[uid][k]];if(!c)continue;twice|=once&c;once|=c;all|=c;}
            unsigned placed=uid<9?rowm[uid]:(uid<18?colm[uid-9]:boxm[uid-18]);
            if((all|placed)!=ALL)return -1;
            unsigned hid=once&~twice&~placed;
            while(hid){unsigned bit=hid&(~hid+1);hid^=bit;
                int tgt=-1,cnt=0;
                for(int k=0;k<9;k++){int i=UNITS[uid][k];if(cand[i]&bit){tgt=i;cnt++;}}
                if(cnt==1){S_hidden++; if(!assign(tgt,__builtin_ctz(bit)+1))return -1;filled++;progress=1;}}
        }
    }
    if(myf>S_maxcasc)S_maxcasc=myf;
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
    int u0=ROWOF[i], u1=COLOF[i]+9, u2=BOXOF[i]+18;
    switch(k){
        case 0: return (double)np;
        case 1: return (double)crit[i];
        case 7: { int n=0; for(int k2=0;k2<NPEER[i];k2++) if(cand[PEERS[i][k2]])n++; return (double)n; }
        case 5: { double p=1; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    p*= (freed[u0][d]>0?freed[u0][d]:1);
                    p*= (freed[u1][d]>0?freed[u1][d]:1);
                    p*= (freed[u2][d]>0?freed[u2][d]:1); }
                  return p; }
        case 17:{ double s=0; for(int k2=0;k2<NPEER[i];k2++){int j=PEERS[i][k2];
                    if(cand[j])s+=crit[j];} return s; }
    }
    return 0;
}
static int FILT, NKEY, KSEL[MAXK]; static double KSIGN[MAXK]; static double KV[81][MAXK];
static void compute_all(int i){ for(int k=0;k<NKEY;k++) KV[i][k]=KSIGN[k]*keyval(i,KSEL[k]); }
static int pick(void){
    compute_freed(); compute_crit();
    int pool[81],pn=0;
    for(int i=0;i<81;i++){
        if(!cand[i])continue;
        if(FILT==1 && crit[i]<=0) continue;
        pool[pn++]=i;
    }
    if(pn==0){ for(int i=0;i<81;i++) if(cand[i]) pool[pn++]=i; if(pn==0)return -1; }
    for(int k=0;k<pn;k++) compute_all(pool[k]);
    int best=pool[0];
    for(int k=1;k<pn;k++){
        int i=pool[k]; int better=0;
        for(int q=0;q<NKEY;q++){
            if(KV[i][q]>KV[best][q]){better=1;break;}
            if(KV[i][q]<KV[best][q]){better=0;break;}
        }
        if(better) best=i;
    }
    return best;
}
static void order_vals(int i,int*ds,int nd){
    if(nd<=1)return; int w[9];
    for(int k=0;k<nd;k++){
        unsigned bit=1u<<(ds[k]-1); int c=0;
        for(int p=0;p<NPEER[i];p++)if(cand[PEERS[i][p]]&bit)c++;
        w[k]=c;
    }
    for(int a=1;a<nd;a++){int kd=ds[a],kw=w[a],b=a-1;
        while(b>=0&&w[b]<kw){ds[b+1]=ds[b];w[b+1]=w[b];b--;} ds[b+1]=kd;w[b+1]=kw;}
}
static long guesses; static int LA;
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
    S_nodes++; cur_depth++;
    if(cur_depth>S_maxdepth)S_maxdepth=cur_depth;
    S_sumdepth+=cur_depth;
    int i=pick(); if(i<0){cur_depth--;return 1;}
    unsigned cc=cand[i]; int ds[9],nd=0;
    for(int d=1;d<=9;d++)if(cc&(1u<<(d-1)))ds[nd++]=d;
    order_vals(i,ds,nd);
    for(int k=0;k<nd;k++){
        if(guesses>=GCAP){ cur_depth--; return -1; }
        int mark0=tlen;
        int ok, filled;
        S_tryfill++;                       /* 人的一次"试填" */
        ok=assign(i,ds[k]);
        filled=ok?propagate():-1;
        if(filled<0){
            S_pruned++;
            S_deadend++;
            undo_to(mark0); continue;
        }
        guesses++;
        int r=dfs();
        undo_to(mark0);
        if(r==1){cur_depth--; return 1;}
        if(r==-1){cur_depth--; return -1;}
    }
    cur_depth--;
    return 0;
}
static const struct{int filt;int n;int ks[5];double sg[5];const char*nm;}SD[3]={
 {1,5,{1,0,7,5,17},{-1,-1,1,-1,-1},"新组合"},
 {0,1,{0},         {-1},           "MRV"},
 {1,1,{0},         {-1},           "critmrv"},
};
int main(int argc,char**argv){
    if(argc<2){fprintf(stderr,"用法: %s file [N] [策略 0/1/2] [LA 0/1]\n",argv[0]);return 1;}
    int NQ=argc>2?atoi(argv[2]):300;
    int STRAT=argc>3?atoi(argv[3]):0;
    LA=argc>4?atoi(argv[4]):1;
    init_tables();
    FILE*f=fopen(argv[1],"r"); if(!f){return 1;}
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
    FILT=SD[STRAT].filt; NKEY=SD[STRAT].n;
    for(int i=0;i<NKEY;i++){KSEL[i]=SD[STRAT].ks[i];KSIGN[i]=SD[STRAT].sg[i];}
    long TR=0,TS=0,TN=0,TH=0,TT=0,TP=0,TE=0,TD=0,TG=0,TSUMD=0,TMAXD=0,TD2=0,TMC=0;
    int solved=0;
    FILE*csv=(argc>5)?fopen(argv[5],"w"):NULL;
    if(csv)fprintf(csv,"q,rounds,naked,hidden,tryfill,pruned,guess,erase_cells,maxdepth,maxcasc,ok\n");
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
        reset_state();
        S_rounds=S_naked=S_hidden=S_tryfill=S_erase_cells=0;
        S_deadend=S_guess=S_pruned=S_maxdepth=S_sumdepth=S_nodes=0; S_del_cand=0; S_maxcasc=0;
        guesses=0; cur_depth=0; g_verify_ok=-1;
        srand(20260904u+q);
        dfs();
        if(g_verify_ok==1)solved++;
        TR+=S_rounds; TS+=S_naked; TN+=S_tryfill; TH+=S_hidden;
        TP+=S_pruned; TE+=S_erase_cells; TD+=S_deadend; TG+=guesses; TD2+=S_del_cand;
        TSUMD+=S_sumdepth; if(S_maxdepth>TMAXD)TMAXD=S_maxdepth;
        if(csv)fprintf(csv,"%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d\n",
            q,S_rounds,S_naked,S_hidden,S_tryfill,S_pruned,guesses,S_erase_cells,S_maxdepth,S_maxcasc,g_verify_ok==1);
    }
    double N=(double)nq;
    printf("\n=== 人类决策步骤核算: 策略=%s, N=%d (解出 %d) ===\n",
        SD[STRAT].nm, nq, solved);
    printf("  (注: 对人而言 la0opt 恒等于\"开\" —— 人试填后必然传播到底才知道矛盾与否)\n\n");
    printf("  %-26s %12s\n","指标","每题平均");
    printf("  %-26s %12.1f\n","机器口径: 假设次数 (guesses)",TG/N);
    printf("  %-26s %12.1f\n","★ 人的试填次数 (含被剪的)",TN/N);
    printf("  %-26s %12.1f\n","  ├─ 成为正式假设",TG/N);
    printf("  %-26s %12.1f\n","  └─ 前瞻后被剪掉(死胡同)",TP/N);
    printf("  %-26s %12.1f\n","★ 传播轮数 (=全盘扫描次数)",TR/N);
    printf("  %-26s %12.1f\n","  ├─ naked single 触发次数",TS/N);
    printf("  %-26s %12.1f\n","  └─ hidden single 触发次数",TH/N);
    printf("  %-26s %12.1f\n","★ 擦除: 已填格数(回溯深度)",TE/N);
    printf("  %-26s %12.1f\n","  擦除: 候选删除数",TD2/N);
        printf("  %-26s %12.1f\n","  平均搜索深度",TSUMD/N/fmax(TG/N,1.0));
    printf("  %-26s %12ld\n","  最大搜索深度",TMAXD);
    printf("  %-26s %12.1f\n","最大单次级联(格)",TMC/N);
    printf("\n");
    /* 人类时间模型 */
    printf("  === 换算成人的时间 (估算) ===\n");
    double t_scan=45.0;    /* 一轮全盘扫描(27unit x 9digit) 秒 */
    double t_try=8.0;      /* 一次试填(选格+选值+写入) 秒 */
    double t_erase=12.0;   /* 一次擦除并恢复 秒 */
    double T = (TR/N)*t_scan + (TN/N)*t_try + (TE/N)*t_erase;
    printf("    假设: 全盘扫描 %.0fs/轮, 试填 %.0fs, 擦除 %.0fs/格\n",t_scan,t_try,t_erase);
    printf("    全盘扫描 %.1f 轮 x %.0fs = %7.0f s (%.1f h)\n",TR/N,t_scan,(TR/N)*t_scan,(TR/N)*t_scan/3600);
    printf("    试填     %.1f 次 x %.0fs = %7.0f s (%.1f h)\n",TN/N,t_try,(TN/N)*t_try,(TN/N)*t_try/3600);
    printf("    擦除     %.1f 格 x %.0fs = %7.0f s (%.1f h)\n",TE/N,t_erase,(TE/N)*t_erase,(TE/N)*t_erase/3600);
    printf("    ----------------------------------------\n");
    printf("    合计                      = %7.0f s = %.1f 小时\n\n",T,T/3600);
    /* 乐观/悲观区间 */
    printf("    乐观 (熟练, 扫描20s/试填5s/擦除8s): %.1f 小时\n",
        ((TR/N)*20+(TN/N)*5+(TE/N)*8)/3600);
    printf("    悲观 (仔细, 扫描90s/试填15s/擦除20s): %.1f 小时\n",
        ((TR/N)*90+(TN/N)*15+(TE/N)*20)/3600);
    return 0;
}
