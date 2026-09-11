/* verify_thm_msvc.c — verify_thm.c 的 MSVC 移植版（语义逐行等价）
 *
 * 为什么需要这个文件：
 *   原 thm/verify_thm.c 末尾用了 GCC 的"语句表达式"扩展 ({ ...; expr; })，
 *   MSVC 的 C 前端不支持该语法（error C2059）。为了在 Windows/MSVC 上仍能跑
 *   README 的"零容忍"判据（verify_thm 的两项非对角块计数必须为 0），
 *   这里把 4 处语句表达式改写成等价的具名变量，其余逻辑与原文**逐行一致**。
 *
 * 与原文的差异（仅此 4 处，全部为纯语法等价改写）：
 *   原: printf(..., ({ long s=0; for(...) s+=...; s; }));
 *   新: long s1=0; for(...) s1+=...; printf(..., s1);
 *
 * 权威版本仍是 thm/verify_thm.c（Linux/gcc 构建即为论文口径）；
 * 本文件只用于 Windows 复核，不参与 Linux 构建。
 */
#define main consolidate_main_unused
#include "consolidate.c"
#undef main
static int *SOLP=NULL;
static long NCELL=0, VTHM=0, VLEM=0;
static long TBL[10][10];   /* [crit][minF] 联合分布 */
static int walk(void){
    if(propagate()<0) return 0;
    for(;;){
        int any=0; for(int i=0;i<81;i++) if(cand[i]){any=1;break;}
        if(!any) return 1;
        /* 扫描全盘空格, 检查定理与引理 */
        compute_freed(); compute_crit();
        for(int i=0;i<81;i++){
            if(!cand[i]) continue;
            NCELL++;
            /* minF */
            int mf=99;
            for(int d=1;d<=9;d++){
                if(!(cand[i]&(1u<<(d-1)))) continue;
                for(int z=0;z<3;z++){
                    int u=UOF[i][z];
                    if(freed[u][d-1]<mf) mf=freed[u][d-1];
                }
            }
            if(mf>9) mf=9;
            int c=crit[i]; if(c>9) c=9;
            TBL[c][mf]++;
            /* 定理 1 */
            if((c>0) != (mf==2)) VTHM++;
            /* 引理 1 */
            for(int d=1;d<=9;d++){
                if(!(cand[i]&(1u<<(d-1)))) continue;
                for(int z=0;z<3;z++){
                    int u=UOF[i][z];
                    if(freed[u][d-1]<2) VLEM++;
                }
            }
        }
        int i=pick_orig_impl(); if(i<0) return 0;
        int d=SOLP[i];
        if(!(cand[i]&(1u<<(d-1)))) return 0;
        int mark=tlen;
        if(!assign(i,d)){ undo_to(mark); return 0; }
        if(propagate()<0){ undo_to(mark); return 0; }
    }
}
int main(int argc,char**argv){
    init_tables();
    int maxq=argc>2?atoi(argv[2]):1000;
    if(!load(argv[1],maxq)){ printf("加载失败\n"); return 1; }
    strat=10;FILT=1;NK=5;
    int full[5]={0,1,2,3,4}; for(int z=0;z<5;z++) KEYS[z]=full[z];
    LA_ON=1;RESTARTS=0;
    int nsol=0;
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++) g_grid[i]=grids[q][i];
        tlen=0;reset_state();wdeg_reset();guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;
        dfs();
        if(!g_hassnap) continue;
        int *sp=malloc(sizeof(int)*81);
        for(int i=0;i<81;i++) sp[i]=g_snap[i];
        nsol++;
        tlen=0;reset_state();wdeg_reset();
        SOLP=sp;
        walk();
        free(sp);
    }
    printf("=== 定理 1 独立复核 (%s, N=%d) ===\n\n",argv[1],nsol);
    printf("  沿正确路径每个决策点扫描全盘空格\n\n");
    printf("  检查空格数:      %ld\n",NCELL);
    printf("  定理 1 违反:     %ld\n",VTHM);
    printf("  引理 1 违反:     %ld\n\n",VLEM);
    printf("  %-10s %10s %10s\n","crit","minF==2","minF!=2");
    printf("  %s\n","------------------------------------");
    long a=0,b=0;
    for(int c=0;c<10;c++){
        for(int m=0;m<10;m++){
            if(m==2) a+=TBL[c][m]; else b+=TBL[c][m];
        }
    }
    for(int c=0;c<10;c++){
        long r2=0,rn=0;
        for(int m=0;m<10;m++) if(m==2) r2+=TBL[c][m]; else rn+=TBL[c][m];
        if(r2||rn) printf("  %-10d %10ld %10ld\n",c,r2,rn);
    }
    /* 以下 4 行 = 原文 4 处 GCC 语句表达式的等价改写 */
    long s1=0; for(int c=1;c<10;c++) s1+=TBL[c][2];
    long s2=0; for(int m=0;m<10;m++) if(m!=2) s2+=TBL[0][m];
    long s3=0; for(int c=1;c<10;c++) for(int m=0;m<10;m++) if(m!=2) s3+=TBL[c][m];
    printf("\n  crit>0 且 minF==2 : %ld\n", s1);
    printf("  crit=0 且 minF!=2 : %ld\n", s2);
    printf("  非对角块 (crit>0,minF!=2): %ld\n", s3);
    printf("  非对角块 (crit=0,minF==2): %ld\n", TBL[0][2]);
    return 0;
}
