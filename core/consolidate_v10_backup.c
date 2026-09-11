/* consolidate.c — 成果夯实: 消融 + 独立基线 + 多数据集 统一口径
 *
 * 目标(来自调研报告第 7.1 / 9 节建议):
 *   A. 消融: 逐个去掉 K1~K5, 区分"crit 是关键"还是"运气好的 tie-breaker"
 *   B. 独立基线: MRV / MRV+degree / dom-wdeg / 随机重启 / Norvig 风格
 *   C. 统一口径: 每题 verify_sol, 报告 guesses/nodes/fill/rounds/CV
 *
 * 编译: gcc -O3 -march=native -o consolidate consolidate.c -lm
 * 用法: ./consolidate puzzles.txt [maxq] [子集]
 *        子集 0=基线 1=消融 2=全部
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define ALL 0x1FFu
#define MAXT 4000000
static long GCAP=20000L;
static const int ROWOF[81]={0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8};
static const int COLOF[81]={0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8};
static const int BOXOF[81]={0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,3,3,3,4,4,4,5,5,5,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,6,6,6,7,7,7,8,8,8,6,6,6,7,7,7,8,8,8};
static int UNITS[27][9], PEERS[81][20], NPEER[81], UOF[81][3];
static int POSIN[81][3];      /* 格 i 在其第 z 个 unit 中的位置 0..8 */
static unsigned UBOXMASK[27][9]; /* unit u 与 box b 的交集, 按 box 位置编号的 9-bit 掩码 */
static unsigned UBITS[81];       /* 格 i 所属的 3 个 unit 的位掩码, 供脏 unit 传播用 */
static void init_tables(void){
    for(int r=0;r<9;r++)for(int c=0;c<9;c++)UNITS[r][c]=r*9+c;
    for(int c=0;c<9;c++)for(int r=0;r<9;r++)UNITS[9+c][r]=r*9+c;
    for(int b=0;b<9;b++){int k=0;for(int i=0;i<81;i++)if(BOXOF[i]==b)UNITS[18+b][k++]=i;}
    for(int i=0;i<81;i++){int n=0;
        for(int u=0;u<27;u++){int in=0;for(int k=0;k<9;k++)if(UNITS[u][k]==i){in=1;break;}
            if(in) UOF[i][n++]=u;}
        char seen[81]={0};int m=0;
        for(int u=0;u<27;u++){int in=0;for(int k=0;k<9;k++)if(UNITS[u][k]==i){in=1;break;}
            if(!in)continue;
            for(int k=0;k<9;k++){int j=UNITS[u][k];if(j!=i&&!seen[j]){seen[j]=1;PEERS[i][m++]=j;}}}
        NPEER[i]=m;}
    for(int i=0;i<81;i++)for(int z=0;z<3;z++){int u=UOF[i][z];
        for(int k=0;k<9;k++)if(UNITS[u][k]==i){POSIN[i][z]=k;break;}}
    for(int u=0;u<27;u++)for(int b=0;b<9;b++){unsigned m=0;
        for(int k=0;k<9;k++){int i=UNITS[u][k]; if(BOXOF[i]!=b)continue;
            for(int q=0;q<9;q++)if(UNITS[18+b][q]==i){m|=1u<<q;break;}}
        UBOXMASK[u][b]=m;}
    for(int i=0;i<81;i++) UBITS[i]=(1u<<UOF[i][0])|(1u<<UOF[i][1])|(1u<<UOF[i][2]);
}
static unsigned cand[81],rowm[9],colm[9],boxm[9];
/* upos[u][d] : unit u 中数字 d 仍可放置的位置掩码 (bit p = UNITS[u][p])
 * 不变式: upos[u][d] 的第 p 位为 1  <=>  cand[UNITS[u][p]] & (1<<d)
 * 增量维护于 assign / kill_cand / undo_to;  使 hidden single 检测从
 * "扫 9 个格" 降为 "一次 popcount"
 */
static unsigned upos[27][9];
/* nk1: 位 i=1 <=> cand[i] 恰含单个候选(裸单待填)。
 * 使 propagate 的裸单查找从"每轮全扫 81 格"降为 O(1) 位图取值。
 * 增量维护于 assign / kill_cand / undo_to; reset_state 全量重建。
 * 不变式: nk1 位 i  <=>  cand[i]!=0 且 popcount(cand[i])==1 */
static unsigned nk1[3];
#define NK1_SET(i) do{ int _i=(i); unsigned _c=cand[_i]; \
    if(_c && !(_c&(_c-1))) nk1[_i>>5] |=  (1u<<(_i&31)); \
    else                   nk1[_i>>5] &= ~(1u<<(_i&31)); }while(0)
static void build_nk1(void){ nk1[0]=nk1[1]=nk1[2]=0;
    for(int i=0;i<81;i++) NK1_SET(i); }
static void build_upos(void){
    for(int u=0;u<27;u++)for(int d=0;d<9;d++)upos[u][d]=0;
    for(int i=0;i<81;i++){
        unsigned c=cand[i]; if(!c)continue;
        const int u0=UOF[i][0],u1=UOF[i][1],u2=UOF[i][2];
        const unsigned b0=1u<<POSIN[i][0],b1=1u<<POSIN[i][1],b2=1u<<POSIN[i][2];
        while(c){ int d=__builtin_ctz(c); c&=c-1;
            upos[u0][d]|=b0; upos[u1][d]|=b1; upos[u2][d]|=b2; }
    }
}
#ifdef UPOS_CHECK
/* 校验模式: 每处使用前断言 upos 与 cand 一致 */
static void upos_assert(void){
    unsigned chk[27][9]; for(int u=0;u<27;u++)for(int d=0;d<9;d++)chk[u][d]=0;
    for(int i=0;i<81;i++){ unsigned c=cand[i]; if(!c)continue;
        for(int z=0;z<3;z++){ int u=UOF[i][z]; unsigned b=1u<<POSIN[i][z];
            while(c){int d=__builtin_ctz(c);c&=c-1;chk[u][d]|=b;} c=cand[i]; } }
    for(int u=0;u<27;u++)for(int d=0;d<9;d++)
        if(chk[u][d]!=upos[u][d]){ fprintf(stderr,"UPOS MISMATCH u=%d d=%d (%u vs %u)\n",u,d,upos[u][d],chk[u][d]); abort(); }
}
#else
#define upos_assert() ((void)0)
#endif
static int g_grid[81];
static int curval[81];   /* 当前赋值(含搜索填入), verify_sol 必须读它, 不能读 g_grid */
static int g_snap[81];   /* 找到解时的快照(供外部读取) */
static int g_hassnap=0;
typedef struct{short idx;unsigned oldcand;short oldr,oldc,oldb;char iscell;}Trail;
static Trail trail[MAXT]; static int tlen=0;
static void reset_state(void){
    tlen=0; memset(rowm,0,sizeof(rowm));memset(colm,0,sizeof(colm));memset(boxm,0,sizeof(boxm));
    for(int i=0;i<81;i++){ curval[i]=g_grid[i];
        if(g_grid[i]){unsigned b=1u<<(g_grid[i]-1);cand[i]=0;
        rowm[ROWOF[i]]|=b;colm[COLOF[i]]|=b;boxm[BOXOF[i]]|=b;} }
    for(int i=0;i<81;i++)if(!g_grid[i])cand[i]=ALL&~(rowm[ROWOF[i]]|colm[COLOF[i]]|boxm[BOXOF[i]]);
    build_upos(); build_nk1();
}
static inline void undo_to(int mark){
    while(tlen>mark){Trail*t=&trail[--tlen];
        int i=t->idx;
        unsigned prevc=cand[i];
        cand[i]=t->oldcand;
        {   /* 回滚 upos: 恢复本格重新获得的候选位 */
            unsigned add = t->oldcand & ~prevc;
            if(add){ const int u0=UOF[i][0],u1=UOF[i][1],u2=UOF[i][2];
                const unsigned b0=1u<<POSIN[i][0],b1=1u<<POSIN[i][1],b2=1u<<POSIN[i][2];
                while(add){ int d=__builtin_ctz(add); add&=add-1;
                    upos[u0][d]|=b0; upos[u1][d]|=b1; upos[u2][d]|=b2; } }
        }
        NK1_SET(i);
        if(t->iscell){curval[i]=0;
            rowm[ROWOF[i]]=t->oldr;colm[COLOF[i]]=t->oldc;boxm[BOXOF[i]]=t->oldb;}}
}
static inline int assign(int i,int d){
    unsigned bit=1u<<(d-1); Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];t->iscell=1;
    cand[i]=0; curval[i]=d; rowm[ROWOF[i]]|=bit;colm[COLOF[i]]|=bit;boxm[BOXOF[i]]|=bit;
    nk1[i>>5] &= ~(1u<<(i&31));
    {   /* cand[i]=0 => 该格在所有 3 个 unit 中不再是任何数字的位置 */
        const int u0=UOF[i][0],u1=UOF[i][1],u2=UOF[i][2];
        const unsigned n0=~(1u<<POSIN[i][0]),n1=~(1u<<POSIN[i][1]),n2=~(1u<<POSIN[i][2]);
        for(int z=0;z<9;z++){ upos[u0][z]&=n0; upos[u1][z]&=n1; upos[u2][z]&=n2; }
    }
    for(int k=0;k<NPEER[i];k++){int j=PEERS[i][k];
        if(cand[j]&bit){Trail*u=&trail[tlen++];u->idx=j;u->oldcand=cand[j];u->iscell=0;
            cand[j]&=~bit;
            {   /* 该格失去数字 d => 3 个 unit 中 d 的位置掩码去掉它 */
                const int dd=d-1;
                upos[UOF[j][0]][dd]&=~(1u<<POSIN[j][0]);
                upos[UOF[j][1]][dd]&=~(1u<<POSIN[j][1]);
                upos[UOF[j][2]][dd]&=~(1u<<POSIN[j][2]);
            }
            NK1_SET(j);
            if(cand[j]==0)return 0;}}
    return 1;
}
static long S_rounds, S_fill;
static int propagate(void){
    S_rounds++; long f=0;
    upos_assert();
    /* 增量传播: 裸单用待处理队列(不再每轮重扫 81 格),
     * hidden single 只扫"脏 unit"(不再每轮扫全部 27 个 unit) */
    /* 注: 裸单用"全盘 81 格线性扫描"反而比队列快 —— 顺序访问对缓存与分支
     * 预测友好, 而队列的随机访问 + 入/出队开销超过收益 (实测 544.9 vs 499.2us/题)。
     * 真正的收益来自 hidden single 用 upos 掩码取代"扫 9 格"。 */
    int changed=1;
    while(changed){
        changed=0;
        /* nk1 位图: 每次取值 O(1), assign 内部增量维护, 取代全扫 81 格 */
        for(;;){
            int w; unsigned m;
            if(nk1[0]){w=0;m=nk1[0];} else if(nk1[1]){w=1;m=nk1[1];}
            else if(nk1[2]){w=2;m=nk1[2];} else break;
            int b=__builtin_ctz(m);
            nk1[w] &= ~(1u<<b);            /* 先清位, 防死循环 */
            int i=w*32+b;
            if(i>=81) continue;            /* 越界位(不应出现) */
            unsigned c=cand[i];
            if(!c||(c&(c-1))) continue;    /* 已填或非单候选 */
            int d=__builtin_ctz(c)+1; int mk=tlen;
            if(!assign(i,d)){undo_to(mk);return -1;} f++; changed=1;
        }
        /* 不再 "有裸单就跳过 hidden 重扫": 同一轮内继续做 hidden single,
         * 减少外层轮数(每轮都要全扫 81 格 + 27 unit)。传播不动点唯一,
         * 故填格总数不变, 仅达成次序不同 => 轮数下降。 */
        for(int u=0;u<27;u++){
            unsigned base = u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
            unsigned av = ALL & ~base;              /* 只遍历本 unit 尚未使用的数字 */
            while(av){
                int d=__builtin_ctz(av); av&=av-1;
                unsigned m=upos[u][d];             /* O(1) 取代扫 9 格 */
                if(m==0)return -1;                  /* 无处可放 => 矛盾 */
                if((m&(m-1))==0){                   /* hidden single */
                    int last=UNITS[u][__builtin_ctz(m)];
                    int mk=tlen;
                    if(!assign(last,d+1)){undo_to(mk);return -1;}
                    f++; changed=1; }
            }
        }
    }
    upos_assert();
    S_fill+=f; return (int)f;
}
static int verify_sol(void){
    for(int u=0;u<27;u++){unsigned m=0;
        for(int k=0;k<9;k++){int i=UNITS[u][k];
            int d=curval[i]; if(d<1||d>9)return 0; m|=1u<<(d-1);}
        if(m!=ALL)return 0;}
    return 1;
}
/* ---------- crit 与各键 ---------- */
static int crit[81];
static void compute_crit(void){
    upos_assert();
    for(int i=0;i<81;i++)crit[i]=0;
    for(int u=0;u<27;u++){
        unsigned base=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
        for(int d=0;d<9;d++){
            if(base&(1u<<d))continue;
            unsigned m=upos[u][d];
            if(m==0)continue;
            unsigned m1=m&(m-1);
            if(m1==0)continue;                 /* popcount==1 */
            if(m1&(m1-1))continue;             /* popcount>=3 */
            crit[UNITS[u][__builtin_ctz(m)]]++;
            crit[UNITS[u][__builtin_ctz(m1)]]++;
        }
    }
}
static int nEmptyPeer(int i){int n=0;for(int k=0;k<NPEER[i];k++)if(cand[PEERS[i][k]])n++;return n;}
static int freed[27][9];
static void compute_freed(void){
    upos_assert();
    for(int u=0;u<27;u++){unsigned base=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
        for(int d=0;d<9;d++){ if(base&(1u<<d)){freed[u][d]=0;continue;}
            freed[u][d]=__builtin_popcount(upos[u][d]); }}
}
/* ---------- 引信演绎 (fuse deduction) ----------
 * 纯逻辑: 格 i 的引信数字 d' (在某 unit u 中 freedom==2, 另一位置为 j_u)
 *   填入 i=d' 矛盾 => i != d' => u 中 d' 只能放 j_u => sol[j_u] = d'
 *   这是确定性的, 与统计无关
 */
static int FUSE_DEDUCT=0;
static int FUSE_TRIPLE_ONLY=0;   /* 只对 triple 格适用 */
static int PRIORITY_TRIPLE=0;    /* 选格时优先 multi-fuse 格 */
static int PT_DEPTH=-1;          /* >=0: 只在递归深度 < PT_DEPTH 时优先 triple */
static int g_depth=0;            /* 当前递归深度 */
static long PT_HIT=0, PT_MISS=0;
static long TRIPLE_N=0;          /* 找到的 triple 格次数 */
static long CAP_HIT=0;
static int SOLVE_LIMIT=1;   /* 1=找第一个解; 2=验证唯一性(数到2个解) */
static int FUSE_ORACLE=0;   /* 作弊: 只填与真解一致的格子 (测上界) */
static long FUSE_OPP=0, FUSE_CELLS=0, FUSE_OK=0, FUSE_TRIES=0;
static int *G_SOL=NULL;   /* 非 NULL 时验证填入的正确性 */
/* 是否存在数字 d' 使 d' 在 i 的 >=2 个 unit 中 freedom==2 (triple/双引信) */
static int is_multi_fuse(int i,int*dp_out){
    compute_freed();
    unsigned cc=cand[i];
    for(int d=1;d<=9;d++){
        if(!(cc&(1u<<(d-1))))continue;
        int cnt=0;
        for(int z=0;z<3;z++){int u=UOF[i][z]; if(freed[u][d-1]==2)cnt++;}
        if(cnt>=2){ if(dp_out)*dp_out=d; return cnt; }
    }
    return 0;
}
static int fuse_deduct(int i,int d){
    if(!FUSE_DEDUCT) return 0;
    compute_freed();
    unsigned bit=1u<<(d-1);
    int js[3], nj=0;
    for(int z=0;z<3;z++){
        int u=UOF[i][z];
        if(freed[u][d-1]!=2) continue;
        int j=-1;
        for(int k=0;k<9;k++){
            int x=UNITS[u][k];
            if(x!=i && (cand[x]&bit)){ j=x; break; }
        }
        if(j<0) continue;
        if(FUSE_ORACLE && G_SOL && G_SOL[j]!=d) continue;   /* 作弊: 只保留正确的 */
        int dup=0; for(int t=0;t<nj;t++) if(js[t]==j) dup=1;
        if(!dup) js[nj++]=j;
    }
    if(nj==0) return 0;
    FUSE_OPP++;
    for(int t=0;t<nj;t++){
        int j=js[t];
        if(!(cand[j]&bit)) continue;
        FUSE_TRIES++;
        int mark=tlen;
        if(!assign(j,d)){ undo_to(mark); return 1; }   /* 矛盾 => 整个节点无解 */
        if(propagate()<0){ undo_to(mark); return 1; }   /* 矛盾 => 整个节点无解 */
        FUSE_CELLS++;
        if(G_SOL && curval[j]==G_SOL[j]) FUSE_OK++;
    }
    return 0;
}

static double prodF(int i){ double p=1; unsigned cc=cand[i];
    for(int d=1;d<=9;d++){ if(!(cc&(1u<<(d-1))))continue;
        int mn=99; for(int z=0;z<3;z++){int u=UOF[i][z];if(freed[u][d-1]<mn)mn=freed[u][d-1];}
        p*= (mn>0?mn:1); }
    return p; }
static int critPeer(int i){int s=0;for(int k=0;k<NPEER[i];k++)s+=crit[PEERS[i][k]];return s;}

/* ---------- 策略定义 ---------- */
/* 键编号: 0=-crit 1=-pc 2=+nEmptyPeer 3=-prodF 4=-critPeer */
static int NK; static int KEYS[8];   /* 当前打分键序列 */
static int FILT;                      /* 0=all 1=crit>0 */
#define MAXQ 60000
static int strat;                     /* 策略号 */
static long guesses; static int g_ok; static int g_nsols=0;

static inline double keyval(int i,int z){
    switch(KEYS[z]){
        case 0: return -(double)crit[i];
        case 1: return -(double)__builtin_popcount(cand[i]);
        case 2: return  (double)nEmptyPeer(i);
        case 3: return -prodF(i);
        case 4: return -(double)critPeer(i);
        default: return 0;
    }
}
static void build_keys(int i,double*kv,int*nk){
    *nk=NK;
    for(int z=0;z<NK;z++) kv[z]=keyval(i,z);
}
/* dom/wdeg */
static double wcon[27];
static void wdeg_reset(void){for(int u=0;u<27;u++)wcon[u]=1.0;}
static double wdeg_of(int i){ double s=0;
    for(int z=0;z<3;z++){int u=UOF[i][z]; int un=0;
        for(int k=0;k<9;k++)if(cand[UNITS[u][k]])un++;
        if(un>=2) s+=wcon[u]; }
    return s; }
int (*g_pick_hook)(void)=NULL;
static int FORCE_CELL=-1;   /* 强制选格 (端到端验证) */
static int FORCE_LEFT=-1;
static int g_prop_clean=0;   /* 1=当前候选集已传播到不动点, dfs 开头可跳过传播 */
static int pick_orig_impl(void);
static int pick(void){
    if(g_pick_hook){ int r=g_pick_hook(); if(r>=0) return r; }
    return pick_orig_impl();
}
static int pick_orig_impl(void){
    if(strat==1||strat==2||strat==3){   /* MRV / MRV+deg / dom-wdeg 不走这里 */
    }
    compute_crit(); compute_freed();
    int pool[81],pn=0;
    for(int i=0;i<81;i++){ if(!cand[i])continue;
        if(FILT && crit[i]<=0) continue; pool[pn++]=i; }
    if(pn==0){ if(!FILT) return -1;
        for(int i=0;i<81;i++) if(cand[i]) pool[pn++]=i;
        if(pn==0) return -1; }
    if(strat==0){  /* 纯 MRV 基线 */
        int b=pool[0]; for(int k=1;k<pn;k++) if(__builtin_popcount(cand[pool[k]])<__builtin_popcount(cand[b])) b=pool[k];
        return b; }
    if(strat==1){  /* MRV + degree (tie: 未赋值 peer 多) */
        int b=pool[0];
        for(int k=1;k<pn;k++){int i=pool[k];int better=0;
            int pi=__builtin_popcount(cand[i]),pb=__builtin_popcount(cand[b]);
            if(pi<pb)better=1; else if(pi>pb)better=0;
            else { int di=nEmptyPeer(i),db=nEmptyPeer(b); if(di>db)better=1; }
            if(better)b=i; }
        return b; }
    if(strat==2){  /* dom/wdeg */
        int b=pool[0]; double bw=-1;
        for(int k=0;k<pn;k++){int i=pool[k];
            double r=(double)__builtin_popcount(cand[i])/wdeg_of(i);
            if(bw<0||r<bw){bw=r;b=i;} }
        return b; }
    /* 优先: multi-fuse 格 (重数最高优先, 平局取索引小) */
    if(FORCE_CELL>=0 && FORCE_LEFT>0){ FORCE_LEFT--; return FORCE_CELL; }
    if(PRIORITY_TRIPLE && (PT_DEPTH<0 || g_depth<PT_DEPTH)){
        int bt=-1, bc=0;
        for(int k=0;k<pn;k++){
            int dp=0; int c=is_multi_fuse(pool[k],&dp);
            if(c>bc){ bc=c; bt=pool[k]; }
        }
        if(bt>=0){ PT_HIT++; return bt; }
        PT_MISS++;
    }
    /* strat>=10: 多键字典序 */
    /* 惰性求值: 字典序比较在第一个出现差异的键处即终止,
     * 因此多数候选只需算 key0/key1, 无需算 prodF / critPeer */
    int best=pool[0];
    /* 缓存 best 的键值: 原版在每次比较中都重算 keyval(best,z),
     * 而 prodF/nEmptyPeer/critPeer 都较贵, pn 大时重复计算量可观.
     * 语义不变: 仍是字典序, 遇到第一个差异的键即终止. */
    double bkv[8]; for(int z=0;z<NK;z++) bkv[z]=keyval(best,z);
    for(int k=1;k<pn;k++){ int i=pool[k]; int better=0;
        for(int z=0;z<NK;z++){
            double a=keyval(i,z), b=bkv[z];
            if(a>b){better=1;break;}
            if(a<b){better=0;break;}
        }
        if(better){ best=i; for(int z=0;z<NK;z++) bkv[z]=keyval(best,z); } }
    return best;
}
/* 值序: vo1 = 波及面降序 */
static int ORDER_DP=0;   /* 值序: 优先试 multi-fuse 的引信数字 */
static int ORACLE_VAL=0; /* 作弊: 把真解值排到第一位 (测值序上界) */
static int OV_MAXDEPTH=1000000; /* 只在前 OV_MAXDEPTH 层作弊 */
static int VORD=0;              /* 值序模式 0=默认 */
static int g_oracle_sol[81];
static void order_vals(int i,int*ds,int nd){
    if(nd<=1)return;
    if(ORACLE_VAL && g_depth<=OV_MAXDEPTH){
        int tv=g_oracle_sol[i];
        if(tv>=1&&tv<=9){
            for(int k=0;k<nd;k++) if(ds[k]==tv){ int t=ds[k]; ds[k]=ds[0]; ds[0]=t; break; }
        }
        return;
    }
    /* VORD: 0=默认(peer计数降序) 1=升序 2=随机 3=最小约束值(peer计数升序,同1)
       4=crit值优先 5=prodF最大优先 */
    if(VORD>=1&&VORD<=5){
        if(VORD==2){ /* 随机: 用确定性hash打乱 */
            unsigned h=(unsigned)(i*2654435761u+(unsigned)guesses*40503u);
            for(int k=nd-1;k>0;k--){ int j=(int)((h>>(k*3))%(k+1)); int t=ds[k];ds[k]=ds[j];ds[j]=t; }
            return;
        }
        double w[9];
        for(int k=0;k<nd;k++){
            int d=ds[k]; unsigned bit=1u<<(d-1);
            if(VORD==1||VORD==3){
                int c=0; for(int z=0;z<NPEER[i];z++) if(cand[PEERS[i][z]]&bit) c++;
                w[k]=(double)c;
            } else if(VORD==4){
                /* 该值在格 i 的 3 个 unit 中 freed==2 的次数 (引信值优先) */
                int c=0;
                for(int z=0;z<3;z++){ int u=UOF[i][z]; if(freed[u][d]==2) c++; }
                w[k]=(double)c;
            } else {
                /* 该值在 3 个 unit 中的 freed 乘积取对数 (越小越"紧") */
                double p=1; for(int z=0;z<3;z++){ int u=UOF[i][z]; p*=(freed[u][d]?freed[u][d]:1); }
                w[k]=-log(p);
            }
        }
        int desc = (VORD==1||VORD==3) ? 0 : 1;  /* 1/3 升序, 4/5 降序 */
        for(int a=1;a<nd;a++){int kd=ds[a];double kw=w[a];int b=a-1;
            while(b>=0&&((desc&&w[b]<kw)||(!desc&&w[b]>kw))){ds[b+1]=ds[b];w[b+1]=w[b];b--;}
            ds[b+1]=kd;w[b+1]=kw;}
        return;
    }
    if(ORDER_DP){
        int dp=0;
        if(is_multi_fuse(i,&dp)){
            for(int k=0;k<nd;k++) if(ds[k]==dp){ int t=ds[k]; ds[k]=ds[0]; ds[0]=t; break; }
        }
    }
    double w[9];
    for(int k=0;k<nd;k++){ int d=ds[k]; unsigned bit=1u<<(d-1); int c=0;
        for(int z=0;z<NPEER[i];z++) if(cand[PEERS[i][z]]&bit) c++;
        w[k]=(double)c; }
    for(int a=1;a<nd;a++){int kd=ds[a];double kw=w[a];int b=a-1;
        while(b>=0&&w[b]<kw){ds[b+1]=ds[b];w[b+1]=w[b];b--;} ds[b+1]=kd;w[b+1]=kw;}
}
static int LA_ON=1;
/* ---------- locked candidates (pointing / claiming) ----------
 * 对手 tdoku(T/shrc+.) 与 fsss(C/shrc..) 均有此传播能力, 我们原本没有
 * 调度: singles 跑到不动点 -> 卡住才上 locked -> 有产出回到 singles
 */
static int LOCKED_ON=0;
static int SP_LEVEL_EXT=0;
static long stat_point=0, stat_claim=0;
static inline int kill_cand(int i,unsigned bit){
    if(!(cand[i]&bit)) return 0;
    Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->iscell=0;
    t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];
    bit &= cand[i];
    cand[i]&=~bit;
    NK1_SET(i);
    {   const int u0=UOF[i][0],u1=UOF[i][1],u2=UOF[i][2];
        const unsigned n0=1u<<POSIN[i][0],n1=1u<<POSIN[i][1],n2=1u<<POSIN[i][2];
        while(bit){ int d=__builtin_ctz(bit); bit&=bit-1;
            upos[u0][d]&=~n0; upos[u1][d]&=~n1; upos[u2][d]&=~n2; }
    }
    return cand[i]?1:-1;
}
static int do_pointing(void){
    int got=0;
    upos_assert();
    for(int b=0;b<9;b++){
        unsigned av=ALL&~boxm[b];            /* 只遍历本宫尚未使用的数字 */
        while(av){
        int d=__builtin_ctz(av); av&=av-1;
        unsigned bit=1u<<d;
        unsigned mm=upos[18+b][d];           /* O(1): 取代扫 9 格 */
        int np=__builtin_popcount(mm);
        if(np<2||np>3)continue;
        unsigned rows=0,cols=0;
        { unsigned t=mm; while(t){int p=__builtin_ctz(t);t&=t-1;int i=UNITS[18+b][p];
            rows|=1u<<ROWOF[i];cols|=1u<<COLOF[i];} }
        if(__builtin_popcount(rows)==1){int r0=__builtin_ctz(rows);
            for(int c=0;c<9;c++){int i=r0*9+c; if(BOXOF[i]==b)continue;
                int r=kill_cand(i,bit); if(r<0)return -1; if(r>0){got=1;stat_point++;}}}
        if(__builtin_popcount(cols)==1){int c0=__builtin_ctz(cols);
            for(int r2=0;r2<9;r2++){int i=r2*9+c0; if(BOXOF[i]==b)continue;
                int r=kill_cand(i,bit); if(r<0)return -1; if(r>0){got=1;stat_point++;}}}
        }
    }
    return got;
}
static int do_claiming(void){
    int got=0;
    upos_assert();
    for(int u=0;u<18;u++){
        unsigned base = u<9?rowm[u]:colm[u-9];
        unsigned av=ALL&~base;               /* 只遍历本行/列尚未使用的数字 */
        while(av){
        int d=__builtin_ctz(av); av&=av-1;
        unsigned bit=1u<<d;
        unsigned mm=upos[u][d];              /* O(1) */
        int np=__builtin_popcount(mm);
        if(np<2||np>3)continue;
        unsigned boxes=0;
        { unsigned t=mm; while(t){int p=__builtin_ctz(t);t&=t-1;boxes|=1u<<BOXOF[UNITS[u][p]];} }
        if(__builtin_popcount(boxes)!=1)continue;
        int b0=__builtin_ctz(boxes);
        unsigned killm = 0x1FFu & ~UBOXMASK[u][b0];   /* box 内不属于 unit u 的位置 */
        while(killm){int p=__builtin_ctz(killm);killm&=killm-1;
            int r=kill_cand(UNITS[18+b0][p],bit); if(r<0)return -1; if(r>0){got=1;stat_claim++;}}
        }
    }
    return got;
}
static long g_stat_np,g_stat_nt,g_stat_hp,g_stat_ht,g_stat_xw;
static int do_naked(void){
    int got=0;
    upos_assert();
    for(int u=0;u<27;u++){
        /* 未填格数早退: naked pair 需要 (2 格 + >=1 个被消除对象) => nemp>=3;
         * naked triple 需要 (3 格 + >=1 个被消除对象) => nemp>=4。
         * 盘面后期大量 unit 已近填满, 此判断可省掉整个收集与匹配过程。 */
        int nemp=0;
        for(int k=0;k<9;k++) if(cand[UNITS[u][k]]) nemp++;
        if(nemp<3) continue;
        int maxn = (nemp>=4)?3:2;
        /* 注意: n=2 与 n=3 两阶段必须各自【重新收集】 —— pair 阶段的消除会把
         * 某些格从 4 候选降到 3, 使其进入 triple 阶段。合并收集会改变语义。 */
        for(int n=2;n<=maxn;n++){
            int cells[9]; unsigned char pcs[9]; int nc=0;
            for(int k=0;k<9;k++){int i=UNITS[u][k];
                if(!cand[i])continue;
                int pc=__builtin_popcount(cand[i]);
                if(pc>=2&&pc<=n){cells[nc]=i;pcs[nc]=(unsigned char)pc;nc++;}}
            if(nc<n)continue;
            /* pcs[] 用收集时快照: pair/triple 成员自身不会被本阶段 kill
             * (消除对象恒为 cells 之外的格), 故其 popcount 在本阶段内不变。 */
            if(n==2){
                for(int a=0;a<nc;a++){
                    if(pcs[a]!=2)continue;
                    for(int b2=a+1;b2<nc;b2++){
                        if(pcs[b2]!=2)continue;
                        if(cand[cells[a]]!=cand[cells[b2]])continue;
                        unsigned m=cand[cells[a]];
                        for(int k=0;k<9;k++){int i=UNITS[u][k];
                            if(i==cells[a]||i==cells[b2])continue;
                            int rc=kill_cand(i,m); if(rc<0)return -1; if(rc){got=1;g_stat_np++;}
                        }
                    }
                }
            } else {
                for(int a=0;a<nc;a++)for(int b2=a+1;b2<nc;b2++){
                    unsigned mab=cand[cells[a]]|cand[cells[b2]];
                    if(__builtin_popcount(mab)>3)continue;   /* 剪枝: 再加一格也不可能=3 */
                    for(int c3=b2+1;c3<nc;c3++){
                        unsigned m=mab|cand[cells[c3]];
                        if(__builtin_popcount(m)!=3)continue;
                        for(int k=0;k<9;k++){int i=UNITS[u][k];
                            if(i==cells[a]||i==cells[b2]||i==cells[c3])continue;
                            int rc=kill_cand(i,m); if(rc<0)return -1; if(rc){got=1;g_stat_nt++;}
                        }
                    }
                }
            }
        }
    }
    return got;
}
static int do_hidden(void){
    int got=0;
    for(int u=0;u<27;u++){
        unsigned base=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
        unsigned avail=0; for(int k=0;k<9;k++){int i=UNITS[u][k];if(cand[i])avail|=cand[i];}
        avail&=~base;
        for(int d1=0;d1<9;d1++){
            if(!(avail&(1u<<d1)))continue;
            for(int d2=d1+1;d2<9;d2++){
                if(!(avail&(1u<<d2)))continue;
                unsigned m2=(1u<<d1)|(1u<<d2);
                int cnt=0,g1=-1,g2=-1;
                for(int k=0;k<9;k++){int i=UNITS[u][k];
                    if(cand[i]&m2){cnt++;if(g1<0)g1=i;else if(g2<0)g2=i;}}
                if(cnt==2){
                    int rc;
                    rc=kill_cand(g1,cand[g1]&~m2); if(rc<0)return -1; if(rc){got=1;g_stat_hp++;}
                    rc=kill_cand(g2,cand[g2]&~m2); if(rc<0)return -1; if(rc){got=1;g_stat_hp++;}
                }
                for(int d3=d2+1;d3<9;d3++){
                    if(!(avail&(1u<<d3)))continue;
                    unsigned m3=m2|(1u<<d3);
                    int cn=0,gs[3];
                    for(int k=0;k<9;k++){int i=UNITS[u][k];
                        if(cand[i]&m3){if(cn<3)gs[cn]=i;cn++;}}
                    if(cn!=3)continue;
                    for(int z=0;z<3;z++){
                        int rc=kill_cand(gs[z],cand[gs[z]]&~m3);
                        if(rc<0)return -1; if(rc){got=1;g_stat_ht++;}
                    }
                }
            }
        }
    }
    return got;
}
static int do_xwing(void){
    int got=0;
    for(int d=0;d<9;d++){
        unsigned bit=1u<<d;
        for(int dim=0;dim<2;dim++){
            int pos[9][9],np[9];
            for(int a=0;a<9;a++){ np[a]=0;
                for(int k=0;k<9;k++){
                    int i = dim==0 ? UNITS[a][k] : UNITS[9+a][k];
                    if(cand[i]&bit) pos[a][np[a]++]=k; } }
            for(int a1=0;a1<9;a1++){ if(np[a1]!=2)continue;
                for(int a2=a1+1;a2<9;a2++){ if(np[a2]!=2)continue;
                    if(pos[a1][0]!=pos[a2][0]||pos[a1][1]!=pos[a2][1])continue;
                    int c1=pos[a1][0],c2=pos[a1][1];
                    for(int a3=0;a3<9;a3++){ if(a3==a1||a3==a2)continue;
                        int i1 = dim==0 ? UNITS[a3][c1] : UNITS[9+a3][c1];
                        int i2 = dim==0 ? UNITS[a3][c2] : UNITS[9+a3][c2];
                        int rc=kill_cand(i1,bit); if(rc<0)return -1; if(rc){got=1;g_stat_xw++;}
                        rc=kill_cand(i2,bit); if(rc<0)return -1; if(rc){got=1;g_stat_xw++;}
                    }}}
        }
    }
    return got;
}
/* ---------- band 配置约束 (3 条线 x 3 个 partner 的完美匹配) ----------
 * 数字 d 在一个 band(3 条平行线) 中恰出现 3 次: 每条线一次, 且三个位置分属
 * 三个不同 box(行 band 看 box-col, 列 band 看 box-row)。这构成一个 3x3
 * 二分图(线 x partner)的完美匹配约束。
 * cell 级 singles/locked 只分别用"线内 exactly-one"与"box 内 exactly-one",
 * 从不检查二者联立的匹配可行性, 故本层可额外排除"不参与任何完美匹配"的
 * 位置, 并更早检出矛盾 —— 与 crit 的"让矛盾更早暴露"是同一机制。
 * 对照意义: tdoku 用 band 表示但也是 band 分支(粒度匹配); 本文保持 cell
 * 级分支, 只借 band 的推理信息, 是 tdoku 未做的消融。 */
static int BAND_ON=0;
void our_set_band(int on){ BAND_ON=on; }
static long stat_bandkill=0, stat_bandcontra=0, stat_bandinfo=0;
static long stat_bandcall=0, stat_bandnf[4]={0,0,0,0};
long our_band_kills(void){ return stat_bandkill; }
long our_band_contra(void){ return stat_bandcontra; }
static int do_band(void){
    int got=0;
    stat_bandcall++;
    upos_assert();
    /* PERM[p][r] = 第 p 种排列中, 线 r 分到的 partner */
    static const int PERM[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for(int v=0;v<2;v++){            /* v=0 行band(partner=box-col); v=1 列band(partner=box-row) */
        for(int b=0;b<3;b++){
            for(int d=0;d<9;d++){
                unsigned bit=1u<<d;
                int M[3],fixed[3],nfixed=0,bad=0;
                for(int r=0;r<3;r++){
                    M[r]=0; fixed[r]=-1;
                    for(int k=0;k<9;k++){
                        int i = (v==0) ? ((b*3+r)*9 + k) : (k*9 + (b*3+r));
                        if(curval[i]==d+1){ fixed[r]=k/3; nfixed++; break; }
                        if(cand[i]&bit) M[r]|=1u<<(k/3);
                    }
                    if(fixed[r]<0 && !M[r]) bad=1;   /* 该线未填 d 且无处可放 => 矛盾 */
                }
                stat_bandnf[nfixed]++;
                if(nfixed==3) continue;              /* d 在本 band 已完整放置 */
                if(bad){ stat_bandcontra++; return -1; }
                int permok[6],nok=0;
                for(int p=0;p<6;p++){
                    int okp=1;
                    for(int r=0;r<3;r++){
                        int want=PERM[p][r];
                        if(fixed[r]>=0){ if(fixed[r]!=want){okp=0;break;} }
                        else if(!(M[r]&(1u<<want))){ okp=0; break; }
                    }
                    permok[p]=okp; if(okp)nok++;
                }
                if(nok==0){ stat_bandcontra++; return -1; }  /* 无完美匹配 => 矛盾 */
                if(nok==6) continue;                         /* 全排列可行 => 无信息 */
                stat_bandinfo++;
                int allow[3]={0,0,0};
                for(int p=0;p<6;p++) if(permok[p])
                    for(int r=0;r<3;r++) allow[r]|=1u<<PERM[p][r];
                for(int r=0;r<3;r++){
                    if(fixed[r]>=0) continue;
                    for(int k=0;k<9;k++){
                        int i = (v==0) ? ((b*3+r)*9 + k) : (k*9 + (b*3+r));
                        if(!(cand[i]&bit)) continue;
                        if(!(allow[r]&(1u<<(k/3)))){
                            int rc=kill_cand(i,bit);
                            if(rc<0) return -1;
                            if(rc>0){ got=1; stat_bandkill++; }
                        }
                    }
                }
            }
        }
    }
    return got;
}
static int propagate_locked(void){
    int r=propagate(); if(r<0)return -1;
    if(!LOCKED_ON&&SP_LEVEL_EXT==0)return r;
    for(int guard=0;guard<50;guard++){
        int a=0,b=0;
        if(LOCKED_ON){ a=do_pointing(); if(a<0)return -1; b=do_claiming(); if(b<0)return -1; }
        int c=0,e=0,f=0;
        if(SP_LEVEL_EXT>=2){ c=do_naked();  if(c<0)return -1; }
        if(SP_LEVEL_EXT>=3){ e=do_hidden(); if(e<0)return -1; }
        if(SP_LEVEL_EXT>=4){ f=do_xwing();  if(f<0)return -1; }
        int g=0;
        if(BAND_ON){ g=do_band(); if(g<0)return -1; }
        /* 若强传播无任何产出, 则候选集未变, propagate() 必然无事可做 => 跳过 */
        if(!a&&!b&&!c&&!e&&!f&&!g) break;
        int r2=propagate(); if(r2<0)return -1;
    }
    return r;
}
static long SOFT_BUD=0; static int g_rem=-1;
void our_set_soft_budget(long b){ SOFT_BUD=b; }
int our_remaining(void){ int n=0; for(int i=0;i<81;i++) if(cand[i])n++; return n; }
int our_last_rem(void){ return g_rem; }
static int g_first_pick=-1;
static int dfs(void){
    if(SOFT_BUD>0 && guesses>=SOFT_BUD){ g_rem=our_remaining(); return -2; }
    if(!g_prop_clean){ if(propagate_locked()<0){ if(strat==2){} return 0; } g_prop_clean=1; }
    int any=0; for(int i=0;i<81;i++)if(cand[i]){any=1;break;}
    if(!any){ if(verify_sol()){ g_nsols++; if(g_nsols==1){ g_ok=1; for(int z=0;z<81;z++)g_snap[z]=curval[z]; g_hassnap=1; }
        if(g_nsols>=SOLVE_LIMIT) return 1; return 0; } return 0; }
    int i=pick(); if(i<0) return 1;
    if(g_depth==0 && g_first_pick<0) g_first_pick=i;
    unsigned cc=cand[i]; int ds[9],nd=0;
    for(int d=1;d<=9;d++)if(cc&(1u<<(d-1)))ds[nd++]=d;
    order_vals(i,ds,nd);
    for(int k=0;k<nd;k++){
        if(!(cand[i]&(1u<<(ds[k]-1)))){
            /* 该候选已被 fuse_deduct 引发的传播删除 */
            if(!cand[i]){                 /* i 本身也已被填入 => 免费推进, 继续深入 */
                if(propagate_locked()<0) return 0;
                g_prop_clean=1;
                return dfs();
            }
            continue;
        }
        if(guesses>=GCAP){ CAP_HIT++; return -1; }
        int mark0=tlen;
        int ok=assign(i,ds[k]);
        int filled = ok?propagate_locked():-1;
        if(filled>=0) g_prop_clean=1;
        if(LA_ON){ if(filled<0){ undo_to(mark0);
            if(FUSE_DEDUCT && fuse_deduct(i,ds[k])) return 0;  /* 推出矛盾=>节点失败 */
            continue; } }
        else { if(!ok){ undo_to(mark0); continue; } }
        guesses++;
        g_depth++;
        int r=dfs();
        g_depth--;
        undo_to(mark0);
        g_prop_clean=0;   /* 保守: 回溯后强制下次重新传播 */
        if(r==1)return 1;
        if(r==-1)return -1;
        if(r==-2)return -2;
        /* dom/wdeg: 该值失败(或子树失败) -> 相关约束加权 */
        if(strat==2){ for(int z=0;z<3;z++) wcon[UOF[i][z]]+=1.0; }
    }
    return 0;
}
/* 随机重启包装 */
static int RESTARTS=0;
static int solve_with_restart(void){
    
    long base=GCAP;
    for(int attempt=0;attempt<=RESTARTS;attempt++){
        GCAP = (long)(base*(attempt+1));
        guesses=0; g_ok=0; tlen=0; reset_state(); wdeg_reset();
        int r=dfs();
        if(g_ok) { GCAP=base; return 1; }
        if(r==-1) continue;   /* 触顶 -> 重启 */
        GCAP=base; return 0;
    }
    GCAP=base; return 0;
}

/* ---------- 主程序 ---------- */
static char line[256]; static int grids[MAXQ][81];
static int nq=0;
static int load(const char*fn,int maxq){
    FILE*f=fopen(fn,"r"); if(!f)return 0; nq=0;
    while(fgets(line,sizeof(line),f)&&nq<maxq){
        int n=0; for(int i=0;i<81;i++){ char ch=0;
            while(line[n]&&(line[n]<'0'||line[n]>'9')&&line[n]!='.')n++;
            if(!line[n])break; ch=line[n++]; grids[nq][i]=(ch>='0'&&ch<='9')?ch-'0':0; }
        if(n>0||line[0]) { int ok=1; for(int i=0;i<81;i++) if(grids[nq][i]<0||grids[nq][i]>9)ok=0;
            if(ok)nq++; }
    }
    fclose(f); return nq;
}
static double geomean(long*v,int n){ double s=0; int c=0;
    for(int i=0;i<n;i++) if(v[i]>0){s+=log((double)v[i]);c++;}
    return c?exp(s/c):0; }

static void run(const char*name,int st,int filt,int nk,int*keys,int la,int rest,int verbose){
    strat=st; FILT=filt; NK=nk; for(int z=0;z<nk;z++)KEYS[z]=keys[z];
    LA_ON=la; RESTARTS=rest;
    long*G=malloc(sizeof(long)*nq); long*F=malloc(sizeof(long)*nq); long*R=malloc(sizeof(long)*nq);
    long TG=0,TF=0,TR=0; int solved=0;
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
        S_rounds=0;S_fill=0; guesses=0; g_ok=0; tlen=0; g_nsols=0; /* 坑58: g_nsols 题间泄漏 */
        reset_state(); wdeg_reset();
        if(rest>0){ int r=solve_with_restart(); (void)r; }
        else dfs();
        if(g_ok)solved++;
        G[q]=guesses; F[q]=S_fill; R[q]=S_rounds;
        TG+=guesses; TF+=S_fill; TR+=S_rounds;
    }
    double gm=geomean(G,nq);
    printf("  %-30s %10ld %10.2f %10.1f %10.1f %8d\n",
        name,TG/nq,gm,(double)TF/nq,(double)TR/nq,solved);
    free(G);free(F);free(R);
}
#ifndef OUR_SOLVER_LIB
int main(int argc,char**argv){
    init_tables();
    if(argc<2){fprintf(stderr,"用法: %s puzzles.txt [maxq] [子集0基线1消融2全部]\n",argv[0]);return 1;}
    int maxq = argc>2?atoi(argv[2]):100000;
    int subset = argc>3?atoi(argv[3]):2;
    if(!load(argv[1],maxq>MAXQ?MAXQ:maxq)){fprintf(stderr,"加载失败\n");return 1;}
    printf("\n=== 成果夯实: %s (N=%d) ===\n\n",argv[1],nq);
    printf("  %-30s %10s %10s %10s %10s %8s\n","策略","平均假设","几何均值","平均填格","传播轮","解出");
    printf("  %s\n","--------------------------------------------------------------------------------------");
    /* ---------- A. 独立基线 ---------- */
    printf("\n[A] 独立基线 (调研报告要求)\n");
    int k0[1]={1};
    run("MRV only (Norvig 风格)",0,0,1,k0,1,0,1);
    int k1[2]={1,2};
    run("MRV + degree",1,0,2,k1,1,0,1);
    run("dom/wdeg",2,0,1,k0,1,0,1);
    run("MRV + 随机重启(3次)",0,0,1,k0,1,3,1);
    run("MRV only, 无 la0opt",0,0,1,k0,0,0,1);
    if(subset==0) return 0;
    if(subset==3){   /* 精简: 只跑关键策略 (用于大批量) */
        int a1[1]={1};
        run("MRV only",0,0,1,a1,1,0,1);
        int k1[2]={1,2};
        run("MRV + degree",1,0,2,k1,1,0,1);
        int full[5]={0,1,2,3,4};
        run("完整 K1-K5",10,1,5,full,1,0,1);
        int m1[4]={1,2,3,4};  run("  去掉 K1",10,1,4,m1,1,0,1);
        int m2[4]={0,2,3,4};  run("  去掉 K2",10,1,4,m2,1,0,1);
        int m3[4]={0,1,3,4};  run("  去掉 K3",10,1,4,m3,1,0,1);
        int m4[4]={0,1,2,4};  run("  去掉 K4",10,1,4,m4,1,0,1);
        int m5[4]={0,1,2,3};  run("  去掉 K5",10,1,4,m5,1,0,1);
        run("critmrv",10,1,1,a1,1,0,1);
        return 0;
    }
    /* ---------- B. 消融 ---------- */
    printf("\n[B] 消融: 逐个去掉打分键 (完整配方 = F=crit>0 + K1..K5)\n");
    int full[5]={0,1,2,3,4};
    run("完整 K1-K5 (F=crit>0)",10,1,5,full,1,0,1);
    int m1[4]={1,2,3,4};  run("  去掉 K1 (-crit)",10,1,4,m1,1,0,1);
    int m2[4]={0,2,3,4};  run("  去掉 K2 (-pc, MRV)",10,1,4,m2,1,0,1);
    int m3[4]={0,1,3,4};  run("  去掉 K3 (+nEmptyPeer)",10,1,4,m3,1,0,1);
    int m4[4]={0,1,2,4};  run("  去掉 K4 (-prodF)",10,1,4,m4,1,0,1);
    int m5[4]={0,1,2,3};  run("  去掉 K5 (-critPeer)",10,1,4,m5,1,0,1);
    printf("\n[B2] 累积添加 (从纯 MRV 开始)\n");
    int a1[1]={1};        run("MRV (F=all)",10,0,1,a1,1,0,1);
    int a2[2]={0,1};      run("+ K1 crit (F=crit>0)",10,1,2,a2,1,0,1);
    int a3[3]={0,1,2};    run("+ K3 nEmptyPeer",10,1,3,a3,1,0,1);
    int a4[4]={0,1,2,3};  run("+ K4 prodF",10,1,4,a4,1,0,1);
    run("+ K5 critPeer (=完整)",10,1,5,full,1,0,1);
    printf("\n[B3] 过滤器消融 (键固定为 -pc)\n");
    run("F=all, keys=[-pc]",10,0,1,a1,1,0,1);
    run("F=crit>0, keys=[-pc] (=critmrv)",10,1,1,a1,1,0,1);
    printf("\n[B4] 值序消融 (选格固定为完整配方)\n");
    run("完整 + la0opt off",10,1,5,full,0,0,1);
    if(subset>=2){
        printf("\n[B5] 3 键与 4 键精简版\n");
        int s3[3]={0,1,2};    run("3 键 K1-K3",10,1,3,s3,1,0,1);
        int s4[4]={0,1,2,3};  run("4 键 K1-K4",10,1,4,s4,1,0,1);
    }
    return 0;
}


#endif  /* OUR_SOLVER_LIB */

#ifdef OUR_SOLVER_LIB
/* ---------- 库接口: 供 head-to-head 对比调用 ---------- */
typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
static int lib_inited=0;
void our_init(void){ if(!lib_inited){ init_tables(); lib_inited=1; } }
/* puzzle: 81 字符, '.' 或 '0' 表示空 */
OurResult our_solve(const char*puzzle){
    OurResult R; R.guesses=0;R.fill=0;R.rounds=0;R.solved=0;
    our_init();
    for(int i=0;i<81;i++){
        char c=puzzle[i];
        g_grid[i]=(c>='1'&&c<='9')?(c-'0'):0;
    }
    S_rounds=0;S_fill=0;guesses=0;g_ok=0;g_hassnap=0;tlen=0;g_nsols=0;g_first_pick=-1;
    g_prop_clean=0;
    reset_state();wdeg_reset();
    long saved_cap=GCAP; GCAP=5000000L;
    dfs();
    GCAP=saved_cap;
    R.guesses=guesses; R.fill=S_fill; R.rounds=S_rounds; R.solved=g_hassnap?1:0;
    return R;
}
/* 配置: 由调用方设置后调用 our_solve */
void our_set_limit(int lim){ SOLVE_LIMIT=lim; }
void our_set_locked(int on){ LOCKED_ON=on; }
void our_set_splevel(int lv){ SP_LEVEL_EXT=lv; }
void our_set_dp(int on){ ORDER_DP=on; }
void our_set_oracle_val(int on,const char*sol,int maxdepth){
    ORACLE_VAL=on?1:0;
    OV_MAXDEPTH=(on&&maxdepth>=0)?maxdepth:1000000;
    if(sol&&on){ for(int z=0;z<81;z++) g_oracle_sol[z]=(sol[z]>='1'&&sol[z]<='9')?(sol[z]-'0'):0; }
    else { for(int z=0;z<81;z++) g_oracle_sol[z]=0; }
}
int our_last_fill(void){ return S_fill; }
int our_last_rounds(void){ return S_rounds; }
void our_set_vord(int v){ VORD=v; }
void our_set_force_cell(int c,int n){ FORCE_CELL=c; FORCE_LEFT=n; }
void our_set_strategy(int st,int filt,int nk,const int*keys,int la){
    strat=st; FILT=filt; NK=nk;
    for(int z=0;z<nk;z++) KEYS[z]=keys[z];
    LA_ON=la; RESTARTS=0; ORDER_DP=0; FUSE_DEDUCT=0; PRIORITY_TRIPLE=0;
}
int our_last_nsols(void){ return g_nsols; }
void our_last_sol(char*out){ if(out)for(int z=0;z<81;z++)out[z]=(char)('0'+g_snap[z]); }
int our_first_pick(void){ return g_first_pick; }
/* 只传播不搜索: 取初始不动点上的特征 */
int our_prop_only(const char*p){
    our_init();
    for(int i=0;i<81;i++){ char c=p[i]; g_grid[i]=(c>='1'&&c<='9')?(c-'0'):0; }
    reset_state(); wdeg_reset();
    int r=propagate_locked();
    if(r>=0){ compute_crit(); compute_freed(); }
    return r;
}
int    our_crit_at(int i){ return (i>=0&&i<81)?crit[i]:0; }
int    our_pc_at(int i){ return (i>=0&&i<81)?__builtin_popcount(cand[i]):0; }
double our_prodF_at(int i){ return (i>=0&&i<81)?prodF(i):0.0; }
int    our_nEmptyPeer_at(int i){ return (i>=0&&i<81)?nEmptyPeer(i):0; }
int    our_critPeer_at(int i){ return (i>=0&&i<81)?critPeer(i):0; }
int    our_empty_at(int i){ return (i>=0&&i<81)?(cand[i]!=0):0; }
#endif
