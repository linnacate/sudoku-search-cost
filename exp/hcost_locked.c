/* hcost_locked.c — locked candidates 对人类手算成本的影响
 * 模型 (§2.54 修正后): 擦除≈免费(标记+分层丢弃), 成本 = 扫描 + 试填
 *   T = rounds * scan_cost + guesses * try_cost
 * locked 增加每轮扫描成本, 但减少 rounds 与 guesses
 */
#define main consolidate_main_unused
#include "consolidate.c"
#undef main
int main(int argc,char**argv){
    init_tables();
    const char*fn=argc>1?argv[1]:"forum_hardest_1905_11plus.txt";
    int maxq=argc>2?atoi(argv[2]):600;
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}
    int keys[5]={0,1,2,3,4};
    strat=10;FILT=1;NK=5;for(int z=0;z<5;z++)KEYS[z]=keys[z];
    LA_ON=1;RESTARTS=0;SOLVE_LIMIT=1;
    printf("=== locked candidates 的人类成本 (%s, N=%d) ===\n\n",fn,nq);
    printf("  模型: T = rounds*scan + guesses*try   (擦除≈0, 见 §2.54)\n");
    printf("  locked 使每轮扫描变贵(要额外查 pointing/claiming)\n\n");
    struct{long rounds,guesses,fill;} R[2];
    for(int lk=0;lk<2;lk++){
        
        LOCKED_ON=lk;
        long tr=0,tg=0,tf=0;
        for(int q=0;q<nq;q++){
            for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
            tlen=0;reset_state();wdeg_reset();
            guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;S_rounds=0;S_fill=0;
            long sc=GCAP;GCAP=5000000L;dfs();GCAP=sc;
            tr+=S_rounds;tg+=guesses;tf+=S_fill;
        }
        R[lk].rounds=tr;R[lk].guesses=tg;R[lk].fill=tf;
    }
    printf("  %-12s %12s %12s %12s\n","","传播轮/题","假设/题","填格/题");
    printf("  %s\n","----------------------------------------------------");
    printf("  %-12s %12.1f %12.1f %12.1f\n","无 locked",
        (double)R[0].rounds/nq,(double)R[0].guesses/nq,(double)R[0].fill/nq);
    printf("  %-12s %12.1f %12.1f %12.1f\n","+ locked",
        (double)R[1].rounds/nq,(double)R[1].guesses/nq,(double)R[1].fill/nq);
    printf("  %-12s %12.3f %12.3f %12.3f\n","比值",
        (double)R[1].rounds/(R[0].rounds?R[0].rounds:1),
        (double)R[1].guesses/(R[0].guesses?R[0].guesses:1),
        (double)R[1].fill/(R[0].fill?R[0].fill:1));
    printf("\n  人类时间估算 (小时), scan=20s/轮, try=15s/次:\n\n");
    printf("  %-24s %10s %10s\n","","无 locked","+ locked");
    printf("  %s\n","------------------------------------------");
    for(int lk=0;lk<2;lk++){}
    double t0=(R[0].rounds*20.0+R[0].guesses*15.0)/nq/3600.0;
    double t1=(R[1].rounds*20.0+R[1].guesses*15.0)/nq/3600.0;
    printf("  %-24s %10.2f %10.2f\n","scan=20s (不查locked)",t0,t1);
    /* locked 使扫描变贵: 假设扫描成本乘以 m */
    for(double m=1.0;m<=2.01;m+=0.25){
        double a=(R[0].rounds*20.0+R[0].guesses*15.0)/nq/3600.0;
        double b=(R[1].rounds*20.0*m+R[1].guesses*15.0)/nq/3600.0;
        printf("  %-24s %10.2f %10.2f   %s\n",
            (char*)(m==1.0?"scan 不变":""),a,b, b<a?"✅ 更快":"❌ 更慢");
        if(m==1.0){}
    }
    printf("\n  临界点: +locked 划算 <=> scan 增幅 < %.2fx\n",
        ((R[0].rounds*20.0+R[0].guesses*15.0)-(R[1].guesses*15.0))/(R[1].rounds*20.0));
    return 0;
}
