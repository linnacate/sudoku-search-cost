/* verify_sp.c — 强传播正确性交叉验证
 *
 * 目的: 确认 locked/naked 没有误删正确候选
 * 手法: L0(弱传播) 与 L2(强传播) 推理规则完全不同,
 *       若两者对同一题给出同一个解, 则强传播不可能误删了正确候选
 *       再独立校验解的合法性(行/列/宫 1-9) 与提示一致性
 */
int propagate_sp(void);
#define main consolidate_main_unused
#include "consolidate_sp.c"
#undef main

static int SP_LEVEL=0;
static long stat_np=0,stat_nt=0,stat_hp=0,stat_ht=0,stat_xw=0;
#include "sp_impl.inc"

static int legal(const int*v){
    for(int u=0;u<27;u++){
        unsigned m=0;
        for(int k=0;k<9;k++){int d=v[UNITS[u][k]];
            if(d<1||d>9)return 0;
            unsigned b=1u<<(d-1);
            if(m&b)return 0;
            m|=b;}
        if(m!=0x1FF)return 0;
    }
    return 1;
}

int main(int argc,char**argv){
    const char*fn=argc>1?argv[1]:"top1465_clean.txt";
    int maxq=argc>2?atoi(argv[2]):1465;
    init_tables();
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}
    printf("数据集 %s  nq=%d\n\n",fn,nq);

    static int sol0[MAXQ][81], sol2[MAXQ][81];
    static int ok0[MAXQ], ok2[MAXQ];
    long g0=0,g2=0;

    int keys[5]={0,1,2,3,4};
    for(int lv=0;lv<=2;lv+=2){
        strat=10;FILT=1;NK=5;for(int z=0;z<NK;z++)KEYS[z]=keys[z];
        LA_ON=1;RESTARTS=0;ORDER_DP=0;FUSE_DEDUCT=0;PRIORITY_TRIPLE=0;
        LOCKED_ON=(lv>=1);SP_LEVEL=lv;SOLVE_LIMIT=1;
        int ns=0;long tg=0;
        for(int q=0;q<nq;q++){
            for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
            tlen=0;reset_state();wdeg_reset();
            guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;S_rounds=0;S_fill=0;
            long sc=GCAP;GCAP=5000000L; dfs(); GCAP=sc;
            if(g_hassnap){ns++;tg+=guesses;
                for(int i=0;i<81;i++){
                    if(lv==0)sol0[q][i]=g_snap[i]; else sol2[q][i]=g_snap[i];}
            }
            if(lv==0)ok0[q]=g_hassnap?1:0; else ok2[q]=g_hassnap?1:0;
        }
        if(lv==0){g0=tg;printf("L0 解出 %d/%d   总guesses=%ld\n",ns,nq,tg);}
        else{g2=tg;printf("L2 解出 %d/%d   总guesses=%ld\n",ns,nq,tg);}
        fflush(stdout);
    }

    /* 逐题比对 */
    int nsame=0,ndiff=0,nmiss=0;
    int illeg0=0,illeg2=0,mism0=0,mism2=0;
    for(int q=0;q<nq;q++){
        if(!ok0[q]||!ok2[q]){nmiss++;continue;}
        int same=1;
        for(int i=0;i<81;i++)if(sol0[q][i]!=sol2[q][i]){same=0;break;}
        if(same)nsame++;else ndiff++;
        if(!legal(sol0[q]))illeg0++;
        if(!legal(sol2[q]))illeg2++;
        for(int i=0;i<81;i++){
            if(grids[q][i]){
                if(grids[q][i]!=sol0[q][i]){mism0++;break;}
            }
        }
        for(int i=0;i<81;i++){
            if(grids[q][i]){
                if(grids[q][i]!=sol2[q][i]){mism2++;break;}
            }
        }
    }
    printf("\n===== 交叉验证结果 =====\n");
    printf("  未解出(任一):      %d\n",nmiss);
    printf("  L0与L2解相同:      %d\n",nsame);
    printf("  L0与L2解不同:      %d   <-- 必须 0\n",ndiff);
    printf("  L0 解非法:         %d   <-- 必须 0\n",illeg0);
    printf("  L2 解非法:         %d   <-- 必须 0\n",illeg2);
    printf("  L0 与提示不符:     %d   <-- 必须 0\n",mism0);
    printf("  L2 与提示不符:     %d   <-- 必须 0\n",mism2);
    printf("\n  guesses 比 L2/L0 = %.4f\n",(double)g2/(double)(g0?g0:1));

    int pass = (ndiff==0&&illeg0==0&&illeg2==0&&mism0==0&&mism2==0&&nmiss==0);
    printf("\n  >>> %s\n", pass?"验证通过: 强传播未误删候选":"*** 验证失败 ***");
    return pass?0:1;
}
