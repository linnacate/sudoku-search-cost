/* keycombo.c — 打分键组合 × 传播配置 交叉裁决
 * 动机: 论文 §5.8 —— 检验"键组合是否依赖传播配置"
 *       在 LOCKED_ON=0+SP_LEVEL=2（较弱传播）下，
 *       精简键组合 {crit,MRV} 是否可能优于全 5 键？
 *       此前的消融均在 LOCKED_ON=1+SP_LEVEL=2 下完成，该组合从未被检验
 * 纪律: 坑52 — 多数据集全量裁决, 不用小样本定档
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
    for(int g=0;g<60;g++){int c=do_naked(); if(c<0)return -1; if(!c)break;
        int r2=propagate_locked(); if(r2<0)return -1;}
    return r;
}
static double run5(int mask,int nq,int lo,int sp){
    int keys[5],nk=0;
    for(int i=0;i<5;i++)if(mask&(1<<i))keys[nk++]=i;
    strat=10;FILT=1;NK=nk;
    for(int i=0;i<nk;i++)KEYS[i]=keys[i];
    LA_ON=1;RESTARTS=0;ORDER_DP=0;FUSE_DEDUCT=0;PRIORITY_TRIPLE=0;
    LOCKED_ON=lo;SP_LEVEL=sp;SOLVE_LIMIT=1;
    double sl=0;int ns=0,tg=0;
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
        tlen=0;reset_state();wdeg_reset();
        guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;S_rounds=0;S_fill=0;
        long sc=GCAP;GCAP=5000000L; dfs(); GCAP=sc;
        if(g_hassnap){ns++;tg+=guesses;sl+=log((double)guesses+1.0);}
    }
    return exp(sl/(ns?ns:1))-1.0;
}
int main(int argc,char**argv){
    init_tables();
    /* 数据集: sample5000=SER>=11 最硬抽样, 其余为交叉裁决集 */
    const char*ds[4]={"sample5000.txt","forum_hardest_1905_11plus.txt",
                      "indep600.txt","top1465_clean.txt"};
    int nq4[4]={5000,1905,600,1465};
    int nd=4;
    /* 先跑全部4个; 若太慢可用 argv 限制 */
    if(argc>1)nd=atoi(argv[1]);
    /* mask: bit0=crit bit1=pc(MRV) bit2=nEP bit3=prodF bit4=critPeer */
    int masks[]={31,23,15,7,3,2,1};
    const char*mn[]={"全5键(F0)","去prodF(F1)","去critPeer(F2)",
                     "去prodF+critPeer(F3)","crit+pc(F4)","纯MRV(F5)","纯crit(F6)"};
    int nm=7;
    /* 配置: {locked, sp} */
    int cfg[][2]={{0,2},{1,2},{1,0},{0,0}};
    const char*cn[]={"A:locked0+naked(较弱传播)","B:locked1+naked(论文L2标准)",
                     "C:locked1 only(L1)","D:locked0 only(L0)"};
    int nc=4;
    if(argc>2)nc=atoi(argv[2]);
    static double R[4][7][4];
    for(int d=0;d<nd;d++){
        if(!load(ds[d],nq4[d])){printf("加载失败 %s\n",ds[d]);return 1;}
        printf("  [%s] N=%d ...",ds[d],nq);fflush(stdout);
        for(int c=0;c<nc;c++)
            for(int m=0;m<nm;m++)
                R[c][m][d]=run5(masks[m],nq,cfg[c][0],cfg[c][1]);
        printf(" done\n");
    }
    for(int c=0;c<nc;c++){
        printf("\n=== %s ===\n",cn[c]);
        printf("  %-22s %9s %9s %9s %9s %9s\n","键组合",
               "sample5k","forum","indep600","top1465","几何平均");
        printf("  %s\n","--------------------------------------------------------------");
        int best=-1;double bg=1e18;
        for(int m=0;m<nm;m++){
            double g=0;int cnt=0;
            for(int d=0;d<nd;d++){g+=log(R[c][m][d]);cnt++;}
            g=exp(g/cnt);
            printf("  %-22s",mn[m]);
            for(int d=0;d<nd;d++)printf(" %9.2f",R[c][m][d]);
            printf(" %9.2f\n",g);
            if(g<bg){bg=g;best=m;}
        }
        double g0=0;for(int d=0;d<nd;d++)g0+=log(R[c][0][d]);g0=exp(g0/nd);
        printf("  >>> 几何最优: %s  (相对全5键 %.4f)\n",mn[best],bg/g0);
    }
    return 0;
}
