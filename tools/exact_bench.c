/*  exact_bench.c — 论文核心数字的精确核对工具
 *
 *  用途: 让审核者一键复现论文中的关键数字（精确到小数点后两位）
 *
 *  编译: gcc -O2 -I core -o exact_bench tools/exact_bench.c -lm
 *       （内部 #include "consolidate.c"，故需 -I core）
 *
 *  用法: ./bin/exact_bench <题集> <N> [ALT_REM]
 *        ALT_REM: 分层反转阈值，默认 30；<0 关闭分层（=静态基线）
 *
 *  常用核对命令:
 *    # 论文 §6.3.2 最终配方（分层开启）
 *    ./bin/exact_bench data/sample5000.txt 5000 30
 *    # 论文 §6.3.2 静态基线（分层关闭）
 *    ./bin/exact_bench data/sample5000.txt 5000 -1
 *    # 独立种群 top1465
 *    ./bin/exact_bench data/top1465_clean.txt 1465 30
 *
 *  环境变量: LOCKED_ON=1  SP_LEVEL=2  （论文完整配方 L2）
 */
#define OUR_SOLVER_LIB
#include "consolidate.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static double now_us(void){
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec*1e6 + t.tv_nsec/1e3;
}

int main(int argc,char**argv){
    const char*fn = argc>1 ? argv[1] : "data/sample5000.txt";
    int N         = argc>2 ? atoi(argv[2]) : 5000;
    int altrem    = argc>3 ? atoi(argv[3]) : 30;

    /* 环境变量覆盖（与论文口径一致） */
    char*e;
    if((e=getenv("LOCKED_ON"))) our_set_locked(atoi(e));   else our_set_locked(1);
    if((e=getenv("SP_LEVEL")))  our_set_splevel(atoi(e));  else our_set_splevel(2);
    ALT_REM = altrem;                       /* <0 关闭分层 */

    int kf[5]={0,1,2,3,4};
    our_init();
    our_set_limit(1);                       /* 论文口径: limit=1 找第一解 */
    our_set_strategy(10,1,5,kf,1);          /* F=crit>0, K1..K5, la0opt on */

    FILE*f=fopen(fn,"r");
    if(!f){ printf("无法打开: %s\n",fn); return 1; }

    char buf[512];
    long tot=0, totf=0; int n=0, solved=0;
    double sum_log=0.0;
    double t0 = now_us();

    while(fgets(buf,sizeof buf,f) && n<N){
        int L=(int)strcspn(buf,"\r\n");
        if(L<81) continue;
        char*pz = buf + (L-81);
        char tmp = *(pz+81); *(pz+81)=0;
        for(int i=0;i<81;i++) if(pz[i]=='.') pz[i]='0';

        OurResult R = our_solve(pz);
        *(pz+81)=tmp;

        if(R.solved) solved++;
        tot  += R.guesses;
        totf += R.fill;
        if(R.guesses>0) sum_log += log((double)R.guesses);
        n++;
    }
    double t1 = now_us();
    fclose(f);

    double am = (double)tot/n;
    double gm = exp(sum_log/n);
    double us = (t1-t0)/n;

    printf("\n===== exact_bench: %s =====\n", fn);
    printf("  N=%d  解出=%d  ALT_REM=%d%s\n", n, solved, altrem,
           altrem<0 ? " (分层关闭=静态基线)" : " (分层开启)");
    printf("  guesses  算术均值 AM = %.4f\n", am);
    printf("  guesses  几何均值 GM = %.4f\n", gm);
    printf("  填格     fill     AM = %.2f\n", (double)totf/n);
    printf("  墙钟               = %.2f us/题  (%.0f 题/秒)\n", us, 1e6/us);
    printf("  per-guess          = %.3f us\n", us/am);
    printf("================================\n\n");
    return 0;
}
