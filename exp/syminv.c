/* syminv.c — 对称变换不变性检验
 * 目的: 验证重标/行列置换/转置是否改变搜索代价 G=P+F
 * 判据: 若 G 严格不变 -> 对称化对求解零收益, 只可能用于缓存去重
 * 编译: gcc -O2 -DOUR_SOLVER_LIB -c consolidate.c -o cons_lib.o
 *       gcc -O2 syminv.c cons_lib.o -lm -o syminv
 * 用法: ./syminv <题库> <题数> <每題变换数R>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
OurResult our_solve(const char*puzzle);
void our_init(void);
void our_set_locked(int on);
void our_set_splevel(int lv);
void our_set_strategy(int st,int filt,int nk,const int*keys,int la);

static unsigned long long st64=88172645463325252ULL;
static unsigned rnd(void){ st64^=st64<<13; st64^=st64>>7; st64^=st64<<17; return (unsigned)(st64>>11); }
static void perm(int*p,int n){
    for(int i=0;i<n;i++)p[i]=i;
    for(int i=n-1;i>0;i--){ int j=(int)(rnd()%(unsigned)(i+1)); int t=p[i];p[i]=p[j];p[j]=t; }
}
/* 行/列置换: band 间置换 + band 内行置换 */
static void rcperm(int*out){
    int b[3]; perm(b,3); int k=0;
    for(int i=0;i<3;i++){
        int w[3]={0,1,2}; perm(w,3);
        for(int j=0;j<3;j++) out[k++]=b[i]*3+w[j];
    }
}
/* 新(r,c) = lab[ 旧(rp[r], cp[c]) ] */
static void xform(const char*in,char*out,int trans,int*rp,int*cp,int*lab){
    char t[81];
    if(trans){ for(int r=0;r<9;r++)for(int c=0;c<9;c++) t[r*9+c]=in[c*9+r]; }
    else memcpy(t,in,81);
    for(int r=0;r<9;r++)for(int c=0;c<9;c++){
        char v=t[rp[r]*9+cp[c]];
        out[r*9+c]=(v>='1'&&v<='9')?(char)('0'+lab[v-'0']):v;
    }
    out[81]=0;
}

int main(int argc,char**argv){
    const char*fn=(argc>1)?argv[1]:"sample5000.txt";
    int N=(argc>2)?atoi(argv[2]):300;
    int R=(argc>3)?atoi(argv[3]):8;
    int mode=(argc>4)?atoi(argv[4]):0;
    static char POOL[20000][82]; static char BUF[20000][82];
    FILE*f=fopen(fn,"r"); if(!f){ printf("open %s fail\n",fn); return 1; }
    char line[256]; int np=0;
    while(fgets(line,sizeof line,f)&&np<N){
        int L=0; while(line[L]&&line[L]!='\n'&&line[L]!='\r') L++;
        if(L<81) continue;
        line[L]=0; memcpy(POOL[np],line,82); np++;
    }
    fclose(f);
    our_init();
    int k5[5]={0,1,2,3,4};
    our_set_locked(1); our_set_splevel(2);
    our_set_strategy(10,1,5,k5,1);

    static long G0[20000],G1[20000];
    long vary=0, totmin=0, totmax=0, totid=0, totall=0;
    double spreadsum=0, sdsum=0; long allsolved=0;
    int maxspread=0;
    for(int i=0;i<np;i++){
        long g[64]; long mn=-1,mx=0,sum=0; double ss=0; G0[i]=0; G1[i]=0;
        for(int t=0;t<R;t++){
            int rp[9],cp[9],lab[10];
            if(t==0){ for(int z=0;z<9;z++){rp[z]=z;cp[z]=z;lab[z+1]=z+1;} xform(POOL[i],BUF[i],0,rp,cp,lab); }
            else{
                int tr=0;
                for(int z=0;z<9;z++){rp[z]=z;cp[z]=z;lab[z+1]=z+1;}
                if(mode==0||mode==2){ rcperm(rp); rcperm(cp); }
                if(mode==0||mode==1){ int l9[9]; perm(l9,9); for(int z=0;z<9;z++) lab[z+1]=l9[z]+1; }
                if(mode==4){ /* 首现序重标 = 我们的弱 canon 重标部分 */
                    int seen[10]={0}, nxt=1;
                    for(int z=0;z<81;z++){ int d=POOL[i][z]-'0'; if(d>=1&&d<=9&&!seen[d]) seen[d]=nxt++; }
                    for(int d=1;d<=9;d++) lab[d]=seen[d];
                }
                if(mode==0||mode==3) tr=(int)(rnd()&1);
                xform(POOL[i],BUF[i],tr,rp,cp,lab);
            }
            OurResult Rs=our_solve(BUF[i]); if(t==0) G0[i]=Rs.guesses; if(t==1) G1[i]=Rs.guesses;
            g[t]=Rs.guesses; allsolved+=Rs.solved;
            if(mn<0||g[t]<mn) mn=g[t];
            if(g[t]>mx) mx=g[t];
            sum+=g[t];
        }
        double mean=(double)sum/R;
        for(int t=0;t<R;t++){ double d=(double)g[t]-mean; ss+=d*d; }
        double sd=sqrt(ss/R);
        spreadsum+=(mx-mn); sdsum+=sd;
        if(mx>mn) vary++;
        if(mx-mn>maxspread) maxspread=(int)(mx-mn);
        totmin+=mn; totmax+=mx; totid+=g[0]; totall+=sum;
    }
    printf("题库=%s  题数=%d  每題变换数R=%d  解出=%ld\n",fn,np,R,allsolved);
    printf("G 有变化的题数      : %ld / %d  (%.2f%%)\n",vary,np,100.0*(double)vary/np);
    printf("平均 (max-min) 极差 : %.4f\n",spreadsum/np);
    printf("最大 极差           : %d\n",maxspread);
    printf("平均 标准差         : %.4f\n",sdsum/np);
    printf("\n各口径 AM:\n");
    printf("  恒等变换         : %.4f\n",(double)totid/np);
    printf("  全部变换平均     : %.4f\n",(double)totall/(np*R));
    printf("  每题取最小(作弊) : %.4f   <- 需 R 倍代价, 仅作参照\n",(double)totmin/np);
    printf("  每题取最大       : %.4f\n",(double)totmax/np);
    if(R==2){ long w=0,l=0,t=0; 
        for(int i=0;i<np;i++){ long a=G0[i],b=G1[i];
            if(a<b)w++; else if(a>b)l++; else t++; }
        double z=(w+l)?(double)(w-l)/sqrt((double)(w+l)):0.0;
        printf("\n配对 恒等 vs 变换: 恒等胜%ld/负%ld/平%ld  z=%+.2f\n",w,l,t,z);
    }
    double gain=(double)totall/(np*R)-(double)totmin/np;
    printf("\n取最小的表面收益   : %.4f (%.2f%%), 但代价 x%d -> 净比 %.3f\n",
        gain,100.0*gain/((double)totall/(np*R)),R,(double)totmin*R/np/((double)totall/(np*R)));
    return 0;
}
