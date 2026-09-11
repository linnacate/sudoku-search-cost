/* strongprop.c — 传播技术谱系测试
 *
 * 动机: locked candidates 单独带来 28% 改进 (48.27->34.74)
 *       => 我们缺的传播技术可能不止这一个
 *   L0 = singles + hidden singles        (基线)
 *   L1 = + locked candidates
 *   L2 = + naked pair / naked triple
 *   L3 = + hidden pair / hidden triple
 *   L4 = + X-Wing
 * 手法: 用宏把 consolidate.c 里的 propagate_locked 换成 propagate_sp,
 *       这样 dfs()/主流程无需改动
 */
int propagate_sp(void);
#define main consolidate_main_unused
#include "consolidate_sp.c"
#undef main

static int SP_LEVEL=0;
static long stat_np=0,stat_nt=0,stat_hp=0,stat_ht=0,stat_xw=0;

/* naked pair/triple: unit 内 n 个格候选并集恰 n 个数字 -> 删 unit 内其他格这些数字 */
static int do_naked(void){
    int got=0;
    for(int u=0;u<27;u++){
        for(int n=2;n<=3;n++){
            int cells[9],nc=0;
            for(int k=0;k<9;k++){int i=UNITS[u][k];
                if(!cand[i])continue;
                int pc=__builtin_popcount(cand[i]);
                if(pc>=2&&pc<=n)cells[nc++]=i;}
            if(nc<n)continue;
            for(int a=0;a<nc;a++){
                if(n==2){
                    if(__builtin_popcount(cand[cells[a]])!=2)continue;
                    for(int b2=a+1;b2<nc;b2++){
                        if(__builtin_popcount(cand[cells[b2]])!=2)continue;
                        if(cand[cells[a]]!=cand[cells[b2]])continue;
                        unsigned m=cand[cells[a]];
                        for(int k=0;k<9;k++){int i=UNITS[u][k];
                            if(i==cells[a]||i==cells[b2])continue;
                            int rc=kill_cand(i,m); if(rc<0)return -1; if(rc){got=1;stat_np++;}
                        }
                    }
                } else {
                    for(int b2=a+1;b2<nc;b2++)for(int c2=b2+1;c2<nc;c2++){
                        unsigned m=cand[cells[a]]|cand[cells[b2]]|cand[cells[c2]];
                        if(__builtin_popcount(m)!=3)continue;
                        for(int k=0;k<9;k++){int i=UNITS[u][k];
                            if(i==cells[a]||i==cells[b2]||i==cells[c2])continue;
                            int rc=kill_cand(i,m); if(rc<0)return -1; if(rc){got=1;stat_nt++;}
                        }
                    }
                }
            }
        }
    }
    return got;
}
/* hidden pair/triple: unit 内 n 个数字只出现在同 n 个格 -> 这些格只留这 n 个数字 */
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
                    rc=kill_cand(g1,cand[g1]&~m2); if(rc<0)return -1; if(rc){got=1;stat_hp++;}
                    rc=kill_cand(g2,cand[g2]&~m2); if(rc<0)return -1; if(rc){got=1;stat_hp++;}
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
                        if(rc<0)return -1; if(rc){got=1;stat_ht++;}
                    }
                }
            }
        }
    }
    return got;
}
/* X-Wing: 数字 d 在两行(列)可放位置恰为同两列(行) -> 删这两列(行)其他行(列)的 d */
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
                        int rc=kill_cand(i1,bit); if(rc<0)return -1; if(rc){got=1;stat_xw++;}
                        rc=kill_cand(i2,bit); if(rc<0)return -1; if(rc){got=1;stat_xw++;}
                    }}}
        }
    }
    return got;
}
int propagate_sp(void){
    int r=propagate_locked(); if(r<0)return -1;
    if(SP_LEVEL<2)return r;
    for(int guard=0;guard<60;guard++){
        int c=do_naked();  if(c<0)return -1;
        int e=(SP_LEVEL>=3)?do_hidden():0; if(e<0)return -1;
        int f=(SP_LEVEL>=4)?do_xwing():0;  if(f<0)return -1;
        if(!c&&!e&&!f)break;
        int r2=propagate_locked(); if(r2<0)return -1;
    }
    return r;
}
int main(int argc,char**argv){
    init_tables();
    const char*fn=argc>1?argv[1]:"forum_hardest_1905_11plus.txt";
    int maxq=argc>2?atoi(argv[2]):300;
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}
    printf("=== 传播技术谱系测试 (%s, N=%d) ===\n\n",fn,nq);
    printf("  %-16s %8s %10s %10s %10s %10s\n","配置","解出","几何均值","相对L0","传播轮","耗时ms");
    printf("  %s\n","----------------------------------------------------------------");
    int keys[5]={0,1,2,3,4};
    double base_gm=0;
    /* 配置: {名, strat, FILT, NK, LA, locked, sp} */
    struct{const char*nm;int st;int fi;int nk;int la;int lk;int sp;}CFG[]={
        {"完整配方+lk+nk", 10,1,5,1, 1,2},
        {"裸MRV+lk+nk",     0,0,1,0, 1,2},
        {"裸MRV+lk",        0,0,1,0, 1,0},
        {"裸MRV(纯)",       0,0,1,0, 0,0},
        {"完整配方+lk",    10,1,5,1, 1,0},
        {"完整配方(纯)",   10,1,5,1, 0,0},
    };
    int NC=sizeof(CFG)/sizeof(CFG[0]);
    for(int ci=0;ci<NC;ci++){
        int lv=CFG[ci].sp;
        strat=CFG[ci].st;FILT=CFG[ci].fi;NK=CFG[ci].nk;
        for(int z=0;z<NK;z++)KEYS[z]=keys[z];
        LA_ON=CFG[ci].la;RESTARTS=0;ORDER_DP=0;FUSE_DEDUCT=0;PRIORITY_TRIPLE=0;
        LOCKED_ON=CFG[ci].lk;SP_LEVEL=lv;SOLVE_LIMIT=1;
        double sl=0;int ns=0;long tr=0;
        struct timeval a,b;gettimeofday(&a,0);
        for(int q=0;q<nq;q++){
            for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
            tlen=0;reset_state();wdeg_reset();
            guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;S_rounds=0;S_fill=0;
            long sc=GCAP;GCAP=5000000L; dfs(); GCAP=sc;
            if(g_hassnap){ns++;sl+=log((double)guesses+1.0);}
            tr+=S_rounds;
        }
        gettimeofday(&b,0);
        double el=(b.tv_sec-a.tv_sec)*1000.0+(b.tv_usec-a.tv_usec)/1000.0;
        double gm=exp(sl/(ns?ns:1))-1.0;
        if(lv==0)base_gm=gm;
        printf("  %-16s %8d %10.2f %10.3f %10.1f %10.0f\n",
            CFG[ci].nm,ns,gm,gm/(base_gm?base_gm:1),(double)tr/(nq?nq:1),el);
        fflush(stdout);
    }
    printf("\n  统计: nakedpair=%ld nakedtri=%ld hidpair=%ld hidtri=%ld xwing=%ld\n",
        stat_np,stat_nt,stat_hp,stat_ht,stat_xw);
    return 0;
}
