/* abl3.c — L2 强传播下的键子集搜索
 *
 * abl2 发现: L2 下 K1(crit) 价值 9.7%->26.7%, K3 符号翻转(1.045->0.980)
 * => 最优键组合可能不再是 §2.62 的 5 键
 * 本轮: 枚举全部 31 个非空子集(保持原相对顺序), 在 forum 选, indep600 验证
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
static double run5(const int*keys,int nk,int nq){
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
static void evalset(const char*fn,int maxq,double*out){
    if(!load(fn,maxq)){printf("加载失败 %s\n",fn);exit(1);}
    for(int m=1;m<32;m++){
        int keys[5],nk=0;
        for(int i=0;i<5;i++)if(m&(1<<i))keys[nk++]=i;
        strat=10;FILT=1;NK=nk;
        for(int i=0;i<nk;i++)KEYS[i]=keys[i];
        LA_ON=1;RESTARTS=0;ORDER_DP=0;FUSE_DEDUCT=0;PRIORITY_TRIPLE=0;
        LOCKED_ON=1;SP_LEVEL=2;SOLVE_LIMIT=1;
        out[m]=run5(keys,nk,nq);
    }
}
int main(int argc,char**argv){
    init_tables();
    int nsel=argc>1?atoi(argv[1]):500;
    double A[32],B[32];
    printf("枚举 31 个子集 @ forum N=%d ...\n",nsel);fflush(stdout);
    evalset("forum_hardest_1905_11plus.txt",nsel,A);
    printf("枚举 31 个子集 @ indep600 (验证) ...\n");fflush(stdout);
    evalset("indep600.txt",600,B);
    const char*kn[5]={"crit","pc","nEP","prodF","critPeer"};
    printf("\n  %-6s %-28s %10s %10s %8s\n","掩码","键组合","forum","indep600","平均序");
    printf("  %s\n","----------------------------------------------------------------");
    /* 排序输出: 按 forum 升序 */
    int ord[31],no=0;
    for(int m=1;m<32;m++)ord[no++]=m;
    for(int i=0;i<no;i++)for(int j=i+1;j<no;j++)
        if(A[ord[j]]<A[ord[i]]){int t=ord[i];ord[i]=ord[j];ord[j]=t;}
    for(int i=0;i<no&&i<12;i++){
        int m=ord[i];char buf[64];buf[0]=0;
        for(int k=0;k<5;k++)if(m&(1<<k)){strcat(buf,kn[k]);strcat(buf," ");}
        printf("  %-6d %-28s %10.2f %10.2f %8d\n",m,buf,A[m],B[m],i+1);
    }
    printf("\n  --- 关键对照 ---\n");
    int full=31,k4=31&(~(1<<4)),k3=31&(~(1<<2)),k35=31&~((1<<2)|(1<<4));
    printf("  5键(全)        mask=%2d  forum %8.2f  indep %8.2f\n",full,A[full],B[full]);
    printf("  4键(去critPeer) mask=%2d  forum %8.2f  indep %8.2f\n",k4,A[k4],B[k4]);
    printf("  4键(去nEP)      mask=%2d  forum %8.2f  indep %8.2f\n",k3,A[k3],B[k3]);
    printf("  3键(去nEP,critPeer) mask=%2d forum %8.2f  indep %8.2f\n",k35,A[k35],B[k35]);
    return 0;
}
