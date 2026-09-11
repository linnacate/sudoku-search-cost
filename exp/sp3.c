/* sp3.c — 传播技术谱系续扫: naked quad / XY-Wing / 逐项增益
 *
 * 2.87 已确认: locked(-24%) + naked(-14~42%) 有效, hidden/xwing 收益<1.5%不采纳
 * 本轮问: naked quad (naked 的自然延伸) 与 XY-Wing (另一类技巧) 是否还有空间
 * 手法: 逐项累加, 看每项单独边际增益
 */
static int G_SKIP_HIDXW=0,G_QUAD=0,G_XYW=0;
int propagate_sp(void);
#define main consolidate_main_unused
#include "consolidate_sp.c"
#undef main

static int SP_LEVEL=0;
static long stat_np=0,stat_nt=0,stat_hp=0,stat_ht=0,stat_xw=0;
static long stat_qd=0,stat_xy=0;
#include "sp_impl2.inc"

/* naked quad: unit 内 4 个格候选并集恰 4 个数字 -> 删 unit 内其他格这 4 个数字 */
static int do_quad(void){
    int got=0;
    for(int u=0;u<27;u++){
        int cells[9],nc=0;
        for(int k=0;k<9;k++){int i=UNITS[u][k];
            if(!cand[i])continue;
            int pc=__builtin_popcount(cand[i]);
            if(pc>=2&&pc<=4)cells[nc++]=i;}
        if(nc<4)continue;
        for(int a=0;a<nc;a++)for(int b=a+1;b<nc;b++)
        for(int c=b+1;c<nc;c++)for(int d=c+1;d<nc;d++){
            unsigned m=cand[cells[a]]|cand[cells[b]]|cand[cells[c]]|cand[cells[d]];
            if(__builtin_popcount(m)!=4)continue;
            for(int k=0;k<9;k++){int i=UNITS[u][k];
                if(i==cells[a]||i==cells[b]||i==cells[c]||i==cells[d])continue;
                int rc=kill_cand(i,m); if(rc<0)return -1; if(rc){got=1;stat_qd++;}
            }
        }
    }
    return got;
}
static int sees(int a,int b){
    return ROWOF[a]==ROWOF[b]||COLOF[a]==COLOF[b]||BOXOF[a]==BOXOF[b];
}
/* XY-Wing: pivot{X,Y} 与两翼 {X,Z}/{Y,Z} 各同行列宫, 且两翼互相可见
 *          -> 删同时可见两翼的格中的 Z */
static int do_xywing(void){
    int got=0;
    for(int p=0;p<81;p++){
        if(__builtin_popcount(cand[p])!=2)continue;
        unsigned cp=cand[p];
        int X=-1,Y=-1;
        for(int d=0;d<9;d++)if(cp&(1u<<d)){if(X<0)X=d;else Y=d;}
        for(int w1=0;w1<81;w1++){
            if(w1==p||!sees(p,w1))continue;
            if(__builtin_popcount(cand[w1])!=2)continue;
            if(!(cand[w1]&(1u<<X)))continue;
            int Z=-1;
            for(int d=0;d<9;d++)if(cand[w1]&(1u<<d))if(d!=X)Z=d;
            if(Z<0||Z==Y)continue;
            for(int w2=0;w2<81;w2++){
                if(w2==p||w2==w1)continue;
                if(!sees(p,w2)||!sees(w1,w2))continue;
                if(__builtin_popcount(cand[w2])!=2)continue;
                unsigned need=(1u<<Y)|(1u<<Z);
                if(cand[w2]!=need)continue;
                for(int i=0;i<81;i++){
                    if(i==p||i==w1||i==w2)continue;
                    if(!(cand[i]&(1u<<Z)))continue;
                    if(!sees(i,w1)||!sees(i,w2))continue;
                    int rc=kill_cand(i,1u<<Z); if(rc<0)return -1; if(rc){got=1;stat_xy++;}
                }
            }
        }
    }
    return got;
}
int propagate_sp(void){
    int r=propagate_locked(); if(r<0)return -1;
    if(SP_LEVEL<2)return r;
    for(int guard=0;guard<60;guard++){
        int c=do_naked();  if(c<0)return -1;
        int e=((SP_LEVEL>=3)&&!G_SKIP_HIDXW)?do_hidden():0; if(e<0)return -1;
        int f=((SP_LEVEL>=4)&&!G_SKIP_HIDXW)?do_xwing():0;  if(f<0)return -1;
        int g=G_QUAD?do_quad():0;   if(g<0)return -1;
        int h=G_XYW?do_xywing():0;  if(h<0)return -1;
        if(!c&&!e&&!f&&!g&&!h)break;
        int r2=propagate_locked(); if(r2<0)return -1;
    }
    return r;
}

int main(int argc,char**argv){
    init_tables();
    const char*fn=argc>1?argv[1]:"forum_hardest_1905_11plus.txt";
    int maxq=argc>2?atoi(argv[2]):300;
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}
    printf("=== 传播技术续扫 (%s, N=%d) ===\n\n",fn,nq);
    printf("  %-20s %8s %10s %10s %10s\n","配置","解出","几何均值","相对L2","耗时ms");
    printf("  %s\n","----------------------------------------------------------");
    int keys[5]={0,1,2,3,4};
    const char*nm[]={"L2 +naked","L3 +hidden","L4 +xwing","L5 +quad","L6 +xywing",
                     "L2+quad(无hid/xw)","L2+quad+xy(无hid/xw)"};
    int lvarr[]={2,3,4,5,6,-5,-6};
    int nl=sizeof(lvarr)/sizeof(lvarr[0]);
    double base=0;
    for(int z=0;z<nl;z++){
        int lv=lvarr[z];
        int quad=(lv==-5||lv==-6)?1:0, xyw=(lv==-6)?1:0;
        int abslv=lv<0?-lv:lv;
        strat=10;FILT=1;NK=5;for(int k=0;k<NK;k++)KEYS[k]=keys[k];
        LA_ON=1;RESTARTS=0;ORDER_DP=0;FUSE_DEDUCT=0;PRIORITY_TRIPLE=0;
        LOCKED_ON=1;SP_LEVEL=abslv;SOLVE_LIMIT=1;
        /* 关闭 hidden/xwing: 用 SP_LEVEL 控制, 但 -5/-6 需要跳过 hid/xw */
        G_SKIP_HIDXW=(lv<0)?1:0;
        G_QUAD=quad; G_XYW=xyw;
        double sl=0;int ns=0;
        struct timeval a,b;gettimeofday(&a,0);
        for(int q=0;q<nq;q++){
            for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
            tlen=0;reset_state();wdeg_reset();
            guesses=0;g_ok=0;g_hassnap=0;g_nsols=0;S_rounds=0;S_fill=0;
            long sc=GCAP;GCAP=5000000L; dfs(); GCAP=sc;
            if(g_hassnap){ns++;sl+=log((double)guesses+1.0);}
        }
        gettimeofday(&b,0);
        double el=(b.tv_sec-a.tv_sec)*1000.0+(b.tv_usec-a.tv_usec)/1000.0;
        double gm=exp(sl/(ns?ns:1))-1.0;
        if(z==0)base=gm;
        printf("  %-20s %8d %10.2f %10.3f %10.0f\n",nm[z],ns,gm,gm/(base?base:1),el);
        fflush(stdout);
    }
    printf("\n  统计: quad=%ld xywing=%ld\n",stat_qd,stat_xy);
    return 0;
}
