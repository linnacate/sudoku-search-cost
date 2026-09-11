/* critmech.c — 分离 crit 的优势来源: 传播更强(P变小) 还是失败更便宜(c变小)
 *
 * 正确的分解 (与值序实验 L8 同构):
 *     G  =  P  +  Ntop * ctop
 *   G    : 总猜测数 (== 总尝试数, 每次尝试 = 1 guess)
 *   P    : 成功路径上的分支点数 (每节点恰好 1 次成功尝试)
 *   Ntop : "顶层失败"次数 = 挂在成功路径节点上的失败子树个数
 *   ctop : 每个顶层失败子树的平均大小(含自身, 递归计入其所有后代)
 *
 * 关键: 只有当某节点最终成功时, 它在成功前试错的那些子树才算"顶层失败".
 *       因此需先缓存在本节点, 待节点返回 1 时再提交.
 *
 * 竞争假设:
 *   H_prop: crit 触发更强传播 => fill/guess 更高 => P 变小
 *   H_fail: crit 让矛盾更早暴露 => ctop 变小 (失败子树更快死)
 *
 * 编译: gcc -O3 -march=native -o critmech critmech.c -lm
 * 用法: ./critmech <题库> [N]
 */
#define OUR_SOLVER_LIB
#include "consolidate.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long I_P=0;          /* 成功路径节点数 */
static long I_Ntop=0;       /* 顶层失败子树个数 */
static long I_Gtop=0;       /* 顶层失败子树总大小 */
static long I_hit1=0;       /* 成功节点中, 第一个值就对的次数 */
static long I_tries=0;
static long I_fill_all=0, I_fill_succ=0, I_fill_fail=0;

static int dfs_i(void){
    if(SOFT_BUD>0 && guesses>=SOFT_BUD){ g_rem=our_remaining(); return -2; }
    if(!g_prop_clean){ if(propagate_locked()<0) return 0; g_prop_clean=1; }
    int any=0; for(int i=0;i<81;i++)if(cand[i]){any=1;break;}
    if(!any){ if(verify_sol()){ g_nsols++; if(g_nsols==1){ g_ok=1; for(int z=0;z<81;z++)g_snap[z]=curval[z]; g_hassnap=1; }
        if(g_nsols>=SOLVE_LIMIT) return 1; return 0; } return 0; }
    int i=pick(); if(i<0) return 1;
    if(g_depth==0 && g_first_pick<0) g_first_pick=i;
    unsigned cc=cand[i]; int ds[9],nd=0;
    for(int d=1;d<=9;d++)if(cc&(1u<<(d-1)))ds[nd++]=d;
    order_vals(i,ds,nd);

    long lsz[9]; int ln=0;   /* 本节点的失败子树大小缓存 */
    int ret=0;

    for(int k=0;k<nd;k++){
        if(!(cand[i]&(1u<<(ds[k]-1)))){
            if(!cand[i]){ if(propagate_locked()<0) return 0; g_prop_clean=1; return dfs_i(); }
            continue;
        }
        if(guesses>=GCAP){ CAP_HIT++; return -1; }
        int mark0=tlen;
        int rem_before=our_remaining();
        int ok=assign(i,ds[k]);
        int filled = ok?propagate_locked():-1;
        if(filled>=0) g_prop_clean=1;
        if(LA_ON){ if(filled<0){ undo_to(mark0);
            if(FUSE_DEDUCT && fuse_deduct(i,ds[k])) return 0;
            continue; } }
        else { if(!ok){ undo_to(mark0); continue; } }
        int rem_after=our_remaining();
        int nf = rem_before-rem_after;
        long g0=guesses;
        guesses++;
        g_depth++;
        int r=dfs_i();
        g_depth--;
        long sz = guesses - g0;      /* 含自身的整棵子树大小 */
        I_tries++;
        I_fill_all += nf;
        if(r==1){
            I_P++; I_fill_succ+=nf;
            if(k==0) I_hit1++;
            /* 本节点成功 => 之前缓存的失败都是顶层失败 */
            for(int t=0;t<ln;t++){ I_Ntop++; I_Gtop+=lsz[t]; }
            ln=0;
            undo_to(mark0); g_prop_clean=0;
            return 1;
        } else if(r==0){
            I_fill_fail+=nf;
            if(ln<9) lsz[ln++]=sz;   /* 暂存, 待定 */
        }
        undo_to(mark0);
        g_prop_clean=0;
        if(r==-1)return -1;
        if(r==-2)return -2;
        if(strat==2){ for(int z=0;z<3;z++) wcon[UOF[i][z]]+=1.0; }
    }
    return 0;
}

static int solve_i(const char*p){
    for(int i=0;i<81;i++){ char c=p[i]; g_grid[i]=(c>='1'&&c<='9')?(c-'0'):0; }
    S_rounds=0;S_fill=0;guesses=0;g_ok=0;g_hassnap=0;tlen=0;g_nsols=0;g_first_pick=-1;
    g_prop_clean=0;
    reset_state();wdeg_reset();
    long saved_cap=GCAP; GCAP=5000000L;
    dfs_i();
    GCAP=saved_cap;
    return g_hassnap?1:0;
}

static char **PZ; static int NP;
static void loadset(const char*fn,int maxn){
    static char buf[4096]; FILE*f=fopen(fn,"r"); if(!f){printf("no %s\n",fn);exit(1);}
    PZ=malloc(sizeof(char*)*(maxn+10)); NP=0;
    while(fgets(buf,sizeof buf,f)&&NP<maxn){
        int L=(int)strcspn(buf,"\r\n"); if(L<81)continue;
        char* st=buf+(L-81);            /* 取尾部 81 字符, 跳过可能的编号前缀 */
        for(int i=0;i<81;i++) if(st[i]=='.') st[i]='0';
        PZ[NP]=malloc(82); memcpy(PZ[NP],st,81); PZ[NP][81]=0; NP++;
    }
    fclose(f);
}

static void reset_i(void){ I_P=I_Ntop=I_Gtop=I_hit1=I_tries=I_fill_all=I_fill_succ=I_fill_fail=0; }

/* 剂量-反应对照: 只在 crit==0 的格中做 MRV (与 critmrv 相反的一端) */
static int pick_anticrit(void){
    compute_crit();
    int best=-1,bp=99;
    for(int i=0;i<81;i++){ if(!cand[i])continue; if(crit[i]>0)continue;
        int pc=__builtin_popcount(cand[i]); if(pc<bp){bp=pc;best=i;} }
    return best;
}
/* 只在 crit>0 中取 crit 最大的 (测试是"过滤"还是"排序"在起作用) */
static int pick_critmax(void){
    compute_crit();
    int best=-1,bc=-1;
    for(int i=0;i<81;i++){ if(!cand[i])continue; if(crit[i]<=0)continue;
        if(crit[i]>bc){bc=crit[i];best=i;} }
    return best;
}

static void go(const char*name,int filt,int nk,int*keys){
    our_init();
    our_set_limit(1); our_set_locked(1); our_set_splevel(2);
    our_set_strategy(10,filt,nk,keys,1);
    long G=0,FILL=0; int solved=0;
    reset_i();
    for(int t=0;t<NP;t++){ if(solve_i(PZ[t])) solved++; G+=guesses; FILL+=S_fill; }
    double P_=I_P, Nt=I_Ntop, Gt=I_Gtop;
    double ctop = Nt>0 ? Gt/Nt : 0.0;
    double chk  = P_ + Gt;
    printf("%-22s sol=%3d/%3d G=%8ld chk=%8.0f | P=%7.0f Ntop=%7.0f ctop=%6.3f | miss/node=%5.3f hit1=%5.3f fill/G=%5.3f(s%5.3f f%5.3f)\n",
        name, solved, NP, G, chk, P_, Nt, ctop,
        P_>0?Nt/P_:0, P_>0?(double)I_hit1/P_:0,
        I_tries>0?(double)I_fill_all/I_tries:0,
        P_>0?(double)I_fill_succ/P_:0,
        Nt>0?(double)I_fill_fail/I_tries:0);
    fflush(stdout);
}

int main(int argc,char**argv){
    const char*fn = argc>1?argv[1]:"sample5000.txt";
    int maxn = argc>2?atoi(argv[2]):300;
    loadset(fn,maxn);
    printf("dataset=%s N=%d [locked=1 splevel=2 limit=1 la0opt=1]\n",fn,NP);
    printf("G = P + Ntop*ctop ; chk 应等于 G\n\n");
    int k_mrv[1]={1};
    int k_full[5]={0,1,2,3,4};
    go("anti-crit (crit==0)",0,1,k_mrv);
    go("MRV (F=all)",        0,1,k_mrv);
    go("critmrv (F=crit>0)", 1,1,k_mrv);
    go("full K1-K5",         1,5,k_full);
    g_pick_hook=pick_anticrit; go("[hook] anticrit MRV",0,1,k_mrv); g_pick_hook=NULL;
    g_pick_hook=pick_critmax;  go("[hook] critmax",     0,1,k_mrv); g_pick_hook=NULL;
    return 0;
}
