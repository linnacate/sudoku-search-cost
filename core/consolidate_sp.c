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
}
static unsigned cand[81],rowm[9],colm[9],boxm[9];
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
}
static inline void undo_to(int mark){
    while(tlen>mark){Trail*t=&trail[--tlen];cand[t->idx]=t->oldcand;
        if(t->iscell){curval[t->idx]=0;
            rowm[ROWOF[t->idx]]=t->oldr;colm[COLOF[t->idx]]=t->oldc;boxm[BOXOF[t->idx]]=t->oldb;}}
}
static inline int assign(int i,int d){
    unsigned bit=1u<<(d-1); Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];t->iscell=1;
    cand[i]=0; curval[i]=d; rowm[ROWOF[i]]|=bit;colm[COLOF[i]]|=bit;boxm[BOXOF[i]]|=bit;
    for(int k=0;k<NPEER[i];k++){int j=PEERS[i][k];
        if(cand[j]&bit){Trail*u=&trail[tlen++];u->idx=j;u->oldcand=cand[j];u->iscell=0;
            cand[j]&=~bit; if(cand[j]==0)return 0;}}
    return 1;
}
static long S_rounds, S_fill;
static int propagate(void){
    S_rounds++; long f=0; int changed=1;
    while(changed){
        changed=0;
        for(int i=0;i<81;i++){ if(!cand[i])continue;
            unsigned c=cand[i]; if((c&(c-1))==0){
                int d=__builtin_ctz(c)+1; int m=tlen;
                if(!assign(i,d)){undo_to(m);return -1;} f++; changed=1; } }
        if(changed)continue;
        for(int u=0;u<27;u++){
            unsigned base = u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
            for(int d=0;d<9;d++){
                if(base&(1u<<d))continue;
                unsigned bit=1u<<d; int cnt=0,last=-1;
                for(int k=0;k<9;k++){int i=UNITS[u][k];if(cand[i]&bit){cnt++;last=i;}}
                if(cnt==0)return -1;
                if(cnt==1){int m=tlen; if(!assign(last,d+1)){undo_to(m);return -1;} f++; changed=1; }
            }
        }
    }
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
    for(int i=0;i<81;i++)crit[i]=0;
    for(int u=0;u<27;u++){
        unsigned base=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
        for(int d=0;d<9;d++){
            if(base&(1u<<d))continue;
            unsigned bit=1u<<d; int cnt=0; int pos[9];
            for(int k=0;k<9;k++){int i=UNITS[u][k];if(cand[i]&bit)pos[cnt++]=i;}
            if(cnt==2){ crit[pos[0]]++; crit[pos[1]]++; }
        }
    }
}
static int nEmptyPeer(int i){int n=0;for(int k=0;k<NPEER[i];k++)if(cand[PEERS[i][k]])n++;return n;}
static int freed[27][9];
static void compute_freed(void){
    for(int u=0;u<27;u++){unsigned base=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
        for(int d=0;d<9;d++){ if(base&(1u<<d)){freed[u][d]=0;continue;}
            unsigned bit=1u<<d;int c=0;
            for(int k=0;k<9;k++)if(cand[UNITS[u][k]]&bit)c++;
            freed[u][d]=c; }}
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

static void build_keys(int i,double*kv,int*nk){
    *nk=NK;
    for(int z=0;z<NK;z++){
        switch(KEYS[z]){
            case 0: kv[z]=-(double)crit[i]; break;
            case 1: kv[z]=-(double)__builtin_popcount(cand[i]); break;
            case 2: kv[z]= (double)nEmptyPeer(i); break;
            case 3: kv[z]= -prodF(i); break;
            case 4: kv[z]=-(double)critPeer(i); break;
            default: kv[z]=0; break;
        }
    }
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
    int best=pool[0]; double bkv[8]; int bnk; build_keys(best,bkv,&bnk);
    for(int k=1;k<pn;k++){ int i=pool[k]; double kv[8]; int nk; build_keys(i,kv,&nk);
        int better=0;
        for(int z=0;z<nk;z++){ if(kv[z]>bkv[z]){better=1;break;} if(kv[z]<bkv[z]){better=0;break;} }
        if(better){ best=i; build_keys(best,bkv,&bnk); } }
    return best;
}
/* 值序: vo1 = 波及面降序 */
static int ORDER_DP=0;   /* 值序: 优先试 multi-fuse 的引信数字 */
static void order_vals(int i,int*ds,int nd){
    if(nd<=1)return;
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
static long stat_point=0, stat_claim=0;
static inline int kill_cand(int i,unsigned bit){
    if(!(cand[i]&bit)) return 0;
    Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->iscell=0;
    t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];
    cand[i]&=~bit;
    return cand[i]?1:-1;
}
static int do_pointing(void){
    int got=0;
    for(int b=0;b<9;b++)for(int d=0;d<9;d++){
        unsigned bit=1u<<d; int pos[9],np=0;
        for(int k=0;k<9;k++){int i=UNITS[18+b][k]; if(cand[i]&&(cand[i]&bit))pos[np++]=i;}
        if(np<2||np>3)continue;
        unsigned rows=0,cols=0;
        for(int k=0;k<np;k++){rows|=1u<<ROWOF[pos[k]];cols|=1u<<COLOF[pos[k]];}
        if(__builtin_popcount(rows)==1){int r0=__builtin_ctz(rows);
            for(int c=0;c<9;c++){int i=r0*9+c; if(BOXOF[i]==b)continue;
                int r=kill_cand(i,bit); if(r<0)return -1; if(r>0){got=1;stat_point++;}}}
        if(__builtin_popcount(cols)==1){int c0=__builtin_ctz(cols);
            for(int r2=0;r2<9;r2++){int i=r2*9+c0; if(BOXOF[i]==b)continue;
                int r=kill_cand(i,bit); if(r<0)return -1; if(r>0){got=1;stat_point++;}}}
    }
    return got;
}
static int do_claiming(void){
    int got=0;
    for(int u=0;u<18;u++)for(int d=0;d<9;d++){
        unsigned bit=1u<<d; int pos[9],np=0;
        for(int k=0;k<9;k++){int i=UNITS[u][k]; if(cand[i]&&(cand[i]&bit))pos[np++]=i;}
        if(np<2||np>3)continue;
        unsigned boxes=0;
        for(int k=0;k<np;k++)boxes|=1u<<BOXOF[pos[k]];
        if(__builtin_popcount(boxes)!=1)continue;
        int b0=__builtin_ctz(boxes);
        for(int k=0;k<9;k++){int i=UNITS[18+b0][k];
            int inU=0; for(int q=0;q<9;q++)if(UNITS[u][q]==i){inU=1;break;}
            if(inU)continue;
            int r=kill_cand(i,bit); if(r<0)return -1; if(r>0){got=1;stat_claim++;}}
    }
    return got;
}
static int propagate_locked(void){
    int r=propagate(); if(r<0)return -1;
    if(!LOCKED_ON)return r;
    for(int guard=0;guard<50;guard++){
        int a=do_pointing(); if(a<0)return -1;
        int b=do_claiming(); if(b<0)return -1;
        int r2=propagate(); if(r2<0)return -1;
        if(!a&&!b)break;
    }
    return r;
}
static long SOFT_BUD=0; static int g_rem=-1;
void our_set_soft_budget(long b){ SOFT_BUD=b; }
int our_remaining(void){ int n=0; for(int i=0;i<81;i++) if(cand[i])n++; return n; }
int our_last_rem(void){ return g_rem; }
static int dfs(void){
    if(SOFT_BUD>0 && guesses>=SOFT_BUD){ g_rem=our_remaining(); return -2; }
    if(propagate_sp()<0){ if(strat==2){} return 0; }
    int any=0; for(int i=0;i<81;i++)if(cand[i]){any=1;break;}
    if(!any){ if(verify_sol()){ g_nsols++; if(g_nsols==1){ g_ok=1; for(int z=0;z<81;z++)g_snap[z]=curval[z]; g_hassnap=1; }
        if(g_nsols>=SOLVE_LIMIT) return 1; return 0; } return 0; }
    int i=pick(); if(i<0) return 1;
    unsigned cc=cand[i]; int ds[9],nd=0;
    for(int d=1;d<=9;d++)if(cc&(1u<<(d-1)))ds[nd++]=d;
    order_vals(i,ds,nd);
    for(int k=0;k<nd;k++){
        if(!(cand[i]&(1u<<(ds[k]-1)))){
            /* 该候选已被 fuse_deduct 引发的传播删除 */
            if(!cand[i]){                 /* i 本身也已被填入 => 免费推进, 继续深入 */
                if(propagate_sp()<0) return 0;
                return dfs();
            }
            continue;
        }
        if(guesses>=GCAP){ CAP_HIT++; return -1; }
        int mark0=tlen;
        int ok=assign(i,ds[k]);
        int filled = ok?propagate_sp():-1;
        if(LA_ON){ if(filled<0){ undo_to(mark0);
            if(FUSE_DEDUCT && fuse_deduct(i,ds[k])) return 0;  /* 推出矛盾=>节点失败 */
            continue; } }
        else { if(!ok){ undo_to(mark0); continue; } }
        guesses++;
        g_depth++;
        int r=dfs();
        g_depth--;
        undo_to(mark0);
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
        S_rounds=0;S_fill=0; guesses=0; g_ok=0; tlen=0;
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
    S_rounds=0;S_fill=0;guesses=0;g_ok=0;g_hassnap=0;tlen=0;g_nsols=0;
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
void our_set_dp(int on){ ORDER_DP=on; }
void our_set_strategy(int st,int filt,int nk,const int*keys,int la){
    strat=st; FILT=filt; NK=nk;
    for(int z=0;z<nk;z++) KEYS[z]=keys[z];
    LA_ON=la; RESTARTS=0; ORDER_DP=0; FUSE_DEDUCT=0; PRIORITY_TRIPLE=0;
}
#endif
