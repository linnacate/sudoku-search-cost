/* abl2.c — 强传播(L2)下的键消融
 *
 * §2.62 在 L0(弱传播) 下测得: K2(MRV) 最重要(+50.5%), K5 应删除(-0.5%)
 * 但 §2.87 加了 locked+naked 后传播强度大变, 各键相对价值可能重排
 * 本轮在 L2 下重做消融
 */
int propagate_sp(void);
#define main consolidate_main_unused
#include "consolidate_sp.c"
#undef main

static int SP_LEVEL=0;
static long stat_np=0,stat_nt=0,stat_hp=0,stat_ht=0,stat_xw=0;
#include "sp_impl2.inc"
int propagate_sp(void){
    int r=propagate_locked(); if(r<0)return -1;
    if(SP_LEVEL<2)return r;
    for(int guard=0;guard<60;guard++){
        int c=do_naked(); if(c<0)return -1;
        if(!c)break;
        int r2=propagate_locked(); if(r2<0)return -1;
    }
    return r;
}

static double run5(const int*keys,int nk,int nq,int locked,int sp){
    double sl=0;int ns=0;
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
        tlen=0;reset_state();wdeg_reset();
        guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;S_rounds=0;S_fill=0;
        long sc=GCAP;GCAP=5000000L; dfs(); GCAP=sc;
        if(g_hassnap){ns++;sl+=log((double)guesses+1.0);}
    }
    return exp(sl/(ns?ns:1))-1.0;
}

int main(int argc,char**argv){
    init_tables();
    const char*fn=argc>1?argv[1]:"forum_hardest_1905_11plus.txt";
    int maxq=argc>2?atoi(argv[2]):500;
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}

    int all[5]={0,1,2,3,4};
    const char*knm[5]={"K1 -crit","K2 -pc(MRV)","K3 +nEmptyPeer","K4 -prodF","K5 -critPeer"};
    printf("=== 消融: 强传播(L2) vs 弱传播(L0) ===\n");
    printf("数据集 %s  N=%d\n\n",fn,nq);
    printf("  %-18s %10s %10s %10s %10s\n","配置","L0几何","L0相对","L2几何","L2相对");
    printf("  %s\n","------------------------------------------------------------");

    /* 配置 0: 全 5 键 */
    /* 配置 1..5: 去掉 Ki */
    /* 配置 6: 4键(去K5) */
    /* 配置 7: 裸 MRV (strat=0) */
    double b0=0,b2=0;
    for(int cfg=0;cfg<=7;cfg++){
        int keys[5];int nk=0;
        if(cfg==7){ /* 裸 MRV 单独处理 */ }
        else if(cfg==6){ for(int i=0;i<4;i++)keys[nk++]=all[i]; }
        else if(cfg==0){ for(int i=0;i<5;i++)keys[nk++]=all[i]; }
        else { for(int i=0;i<5;i++)if(i!=cfg-1)keys[nk++]=all[i]; }

        /* L0 */
        strat=(cfg==7)?0:10; FILT=1; NK=nk;
        for(int i=0;i<nk;i++)KEYS[i]=keys[i];
        LA_ON=1;RESTARTS=0;ORDER_DP=0;FUSE_DEDUCT=0;PRIORITY_TRIPLE=0;
        LOCKED_ON=0;SP_LEVEL=0;SOLVE_LIMIT=1;
        double g0=run5(keys,nk,nq,0,0);
        /* L2 */
        LOCKED_ON=1;SP_LEVEL=2;
        double g2=run5(keys,nk,nq,1,2);
        if(cfg==0){b0=g0;b2=g2;}
        const char*nm = cfg==0?"完整 5 键(基线)":(cfg==7?"裸 MRV(无键)":(cfg==6?"4键(去K5)":knm[cfg-1]));
        printf("  %-18s %10.2f %10.3f %10.2f %10.3f\n",
            nm,g0,g0/(b0?b0:1),g2,g2/(b2?b2:1));
        fflush(stdout);
    }
    printf("\n  注: 相对值 >1 表示去掉/改变后变差(该键重要), <1 表示变好(该键应删)\n");
    printf("      关键看 L0相对 与 L2相对 两列是否同号、同序\n");
    return 0;
}
