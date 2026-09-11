/* row1exp.c —— 检验「把解盘第一行规范成 1..9，然后去猜空格对应什么」这一构想
 *
 * 三件事:
 *  (1) 零信息核验: 数字重标(不做位置置换)后, 各格候选数是否逐格不变
 *  (2) Oracle 价值: 若「第一行空缺的真值」白送, 假设数能降多少
 *  (3) 真实代价: 枚举第一行全部一致补全(提交 k 个赋值后才传播)的总代价
 *
 * 编译: gcc -O2 -march=native -DOUR_SOLVER_LIB -c consolidate.c -o cons_lib.o
 *       gcc -O2 -march=native row1exp.c cons_lib.o -o row1exp -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
void our_init(void);
OurResult our_solve(const char*puzzle);
void our_set_locked(int on);
void our_set_splevel(int lv);
void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
void our_last_sol(char*out);
int  our_prop_only(const char*p);
int  our_cand_at(int i);
int  our_pc_at(int i);
int  our_first_pick(void);

static char P[20000][82]; static int NP=0;
static void loadp(const char*fn,int maxq){
    FILE*f=fopen(fn,"r"); if(!f){fprintf(stderr,"no %s\n",fn);exit(1);}
    char line[256];
    while(fgets(line,sizeof line,f)&&NP<maxq){
        int n=0; for(int i=0;line[i];i++){ char c=line[i];
            if((c>='1'&&c<='9')||c=='.'||c=='0'){ if(n<81) P[NP][n++]=(c=='0'||c=='.')?'.':c; } }
        if(n==81){ P[NP][81]=0; NP++; }
    }
    fclose(f);
}
static int krow1=0;
static int blanks[9];            /* 第一行空缺格的 cell 索引 */
static int cand_mask[9];         /* 初始传播后各空缺格的候选位掩码 */
static int used_digit[10];
static long batch_total, batch_cnt, batch_perm;
static int perm_digits[9], nb;
static char aug[82];

/* 递归枚举第一行的合法补全(仅要求数字互异 + 落在候选集内), 不做格间传播 */
static void enum_row(int pos){
    if(pos==nb){
        for(int z=0;z<nb;z++) aug[blanks[z]]=(char)('0'+perm_digits[z]);
        OurResult r=our_solve(aug);
        batch_perm++;
        batch_total += (long)nb + r.guesses;   /* 提交 k 个赋值 + 其子树 */
        batch_cnt++;
        return;
    }
    for(int d=1;d<=9;d++){
        if(used_digit[d]) continue;
        if(!(cand_mask[pos]&(1<<(d-1)))) continue;   /* d 的位表示: bit(d-1)? */
        used_digit[d]=1; perm_digits[pos]=d;
        enum_row(pos+1);
        used_digit[d]=0;
    }
}

int main(int argc,char**argv){
    const char*fn = argc>1?argv[1]:"sample5000.txt";
    int maxq      = argc>2?atoi(argv[2]):200;
    int do_batch  = argc>3?atoi(argv[3]):1;
    loadp(fn,maxq);
    our_init(); our_set_locked(1); our_set_splevel(2);
    int full[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,full,1);

    long SG0=0,SG1=0,SK=0,SH1=0; long SOV=0; int n=0;
    int relabel_diff=0;
    char sol[82];
    for(int q=0;q<NP;q++){
        OurResult r0=our_solve(P[q]);
        our_last_sol(sol);
        /* ---- (1) 零信息核验: 用解盘第一行定出重标 sigma, 比较候选数 ---- */
        int perm[10];  /* perm[j] = 解盘第一行第 j 列的值 */
        for(int j=0;j<9;j++) perm[j]=sol[j]-'0';
        int sig[10];   /* sig[v] = v 被重标成什么: 使第一行变成 1..9 */
        for(int j=0;j<9;j++) sig[perm[j]]=j+1;
        char rl[82];
        for(int i=0;i<81;i++){ char c=P[q][i]; rl[i]=(c>='1'&&c<='9')?(char)('0'+sig[c-'0']):'.'; }
        rl[81]=0;
        our_prop_only(P[q]); int a[81]; for(int i=0;i<81;i++)a[i]=our_pc_at(i);
        our_prop_only(rl);   int b[81]; for(int i=0;i<81;i++)b[i]=our_pc_at(i);
        for(int i=0;i<81;i++) if(a[i]!=b[i]) relabel_diff++;

        /* ---- (2) Oracle: 第一行空缺填真值 ---- */
        strcpy(aug,P[q]);
        krow1=0;
        for(int j=0;j<9;j++) if(P[q][j]=='.'){ aug[j]=sol[j]; krow1++; }
        OurResult r1=our_solve(aug);

        /* ---- (2b) 只白送 1 个真值: 启发式首个选中的格 ---- */
        int fp=our_first_pick();
        char aug2[82]; strcpy(aug2,P[q]);
        if(fp>=0&&fp<81) aug2[fp]=sol[fp];
        OurResult r2=our_solve(aug2);
        SH1 += r2.guesses;

        SG0+=r0.guesses; SG1+=r1.guesses; SK+=krow1;
        n++;
        if(do_batch){
            /* ---- (3) 真实代价: 枚举第一行补全 ---- */
            our_prop_only(P[q]);
            nb=0; for(int j=0;j<9;j++) if(P[q][j]=='.'){ blanks[nb]=j; cand_mask[nb]=our_cand_at(j); nb++; }
            for(int d=0;d<10;d++) used_digit[d]=0;
            for(int j=0;j<9;j++) if(P[q][j]>='1'&&P[q][j]<='9') used_digit[P[q][j]-'0']=1;
            strcpy(aug,P[q]);
            long t0=batch_total,c0=batch_cnt;
            enum_row(0);
            SOV += (batch_total-t0);
            (void)c0;
        }
    }
    printf("N=%d  数据集=%s\n",n,fn);
    printf("(1) 重标后候选数不一致的格数: %d  (应为 0 -> 重标不产生新信息)\n",relabel_diff);
    printf("(2) 第一行空缺数 k 平均 = %.2f\n",(double)SK/n);
    printf("    基线 G0            = %.3f\n",(double)SG0/n);
    printf("    白送第一行真值 G1  = %.3f   -> Oracle 收益 %.1f%%\n",
           (double)SG1/n, 100.0*(1.0-(double)SG1/(double)SG0));
    printf("    白送 1 个真值(启发式首格) G2 = %.3f -> 收益 %.1f%%\n",
           (double)SH1/n, 100.0*(1.0-(double)SH1/(double)SG0));
    if(do_batch){
        printf("(3) 枚举第一行补全(提交 k 个再传播):\n");
        printf("    平均合法补全数    = %.2f\n",(double)batch_perm/n);
        printf("    总代价(guesses)   = %.1f  -> 相对基线 %.2fx\n",
               (double)SOV/n, (double)SOV/(double)SG0);
    }
    return 0;
}
