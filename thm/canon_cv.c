/* canon_cv.c — 验证规范化后 CV 确实 = 0 (用当前 5 键配方)
 * 对照: 同一批变换, 不规范化时的 CV
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define NP 1296
static const int P3[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static int ROWP[NP][9];
static void genperms(void){int p=0;
    for(int b=0;b<6;b++)for(int i0=0;i0<6;i0++)for(int i1=0;i1<6;i1++)for(int i2=0;i2<6;i2++){
        int *r=ROWP[p++];int k=0;
        for(int bb=0;bb<3;bb++){int band=P3[b][bb];
            const int*in=(bb==0?P3[i0]:(bb==1?P3[i1]:P3[i2]));
            for(int j=0;j<3;j++)r[k++]=band*3+in[j];}}}
static int best[81];
static void canon_grid(const int*g,int*out){
    int hasbest=0;int tmp[81],rowg[81],tmp2[81];
    for(int tr=0;tr<2;tr++){
        for(int i=0;i<81;i++)tmp[i]=tr?g[(i%9)*9+i/9]:g[i];
        for(int rp=0;rp<NP;rp++){const int*rp9=ROWP[rp];
            for(int i=0;i<81;i++)rowg[i]=tmp[rp9[i/9]*9+(i%9)];
            for(int cp=0;cp<NP;cp++){const int*cp9=ROWP[cp];
                int fr[9],map[10],nm=0;for(int i=0;i<10;i++)map[i]=0;
                for(int c=0;c<9;c++){int d=rowg[cp9[c]];if(d){if(!map[d])map[d]=++nm;fr[c]=map[d];}else fr[c]=0;}
                int cmp=0;if(!hasbest)cmp=-1;
                else for(int c=0;c<9;c++)if(fr[c]!=best[c]){cmp=fr[c]<best[c]?-1:1;break;}
                if(cmp<=0){
                    for(int i=0;i<10;i++)map[i]=0;nm=0;
                    for(int i=0;i<81;i++){int d=rowg[(i/9)*9+cp9[i%9]];
                        if(d){if(!map[d])map[d]=++nm;tmp2[i]=map[d];}else tmp2[i]=0;}
                    if(cmp<0){memcpy(best,tmp2,sizeof(int)*81);hasbest=1;}
                    else{int c2=0;for(int i=0;i<81;i++)if(tmp2[i]!=best[i]){c2=tmp2[i]<best[i]?-1:1;break;}
                        if(c2<0)memcpy(best,tmp2,sizeof(int)*81);}}}}}
    memcpy(out,best,sizeof(int)*81);
}
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OurResult_struct{ long guesses; long fill; long rounds; int solved; } OurResult;
  void our_init(void);
  OurResult our_solve(const char*puzzle);
  void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
#ifdef __cplusplus
}
#endif
static char line[256];static int grids[2000][81];static int nq=0;
static int load(const char*fn,int maxq){
    FILE*f=fopen(fn,"r");if(!f)return 0;nq=0;
    while(fgets(line,sizeof(line),f)&&nq<maxq){
        int n=0;for(int i=0;i<81;i++){char ch=0;
            while(line[n]&&(line[n]<'0'||line[n]>'9')&&line[n]!='.')n++;
            if(!line[n])break;ch=line[n++];grids[nq][i]=(ch>='0'&&ch<='9')?ch-'0':0;}
        if(n>0||line[0])nq++;}
    fclose(f);return nq;}
static unsigned long st=88172645463325252UL;
static unsigned long rnd(void){st^=st<<13;st^=st>>7;st^=st<<17;return st;}
int main(int argc,char**argv){
    genperms();
    const char*fn=argc>1?argv[1]:"indep600.txt";
    int maxq=argc>2?atoi(argv[2]):60;
    int T=argc>3?atoi(argv[3]):10;
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}
    our_init();int keys[5]={0,1,2,3,4};our_set_strategy(10,1,5,keys,1);
    printf("=== CV 验证: 规范化后是否精确 = 0 ===\n\n");
    printf("  数据集 %s, N=%d, 每题 T=%d 个随机对称变换\n\n",fn,nq,T);
    double cvRaw=0,cvCan=0;int nn=0,diffCnt=0;
    for(int q=0;q<nq;q++){
        long raw[T],can[T];int ok=1;
        for(int t=0;t<T;t++){
            /* 随机变换: 转置 + 行带/带内 + 列栈/栈内 + 数字重标 */
            int g[81];
            int tr=(int)(rnd()%2);
            int rp[9],cp[9],dm[10];
            {int p=(int)(rnd()%NP);for(int i=0;i<9;i++)rp[i]=ROWP[p][i];}
            {int p=(int)(rnd()%NP);for(int i=0;i<9;i++)cp[i]=ROWP[p][i];}
            {int m[9],k;for(int i=0;i<9;i++)m[i]=i;
                for(int i=8;i>0;i--){int j=(int)(rnd()%(i+1));int t2=m[i];m[i]=m[j];m[j]=t2;}
                for(int i=0;i<10;i++)dm[i]=0;
                for(int i=0;i<9;i++)dm[i+1]=m[i]+1;}
            for(int r=0;r<9;r++)for(int c=0;c<9;c++){
                int v=grids[q][(tr?c:r)*9+(tr?r:c)];
                v=grids[q][rp[tr?c:r]*9+cp[tr?r:c]];
                int rr=rp[tr?c:r],cc=cp[tr?r:c];
                int src=rr*9+cc;
                int val=grids[q][src];
                g[r*9+c]=val?dm[val]:0;}
            char pz[82];for(int i=0;i<81;i++)pz[i]=g[i]?('0'+g[i]):'.';pz[81]=0;
            OurResult a=our_solve(pz); if(!a.solved){ok=0;break;} raw[t]=a.guesses;
            int cg[81];canon_grid(g,cg);
            char pz2[82];for(int i=0;i<81;i++)pz2[i]=cg[i]?('0'+cg[i]):'.';pz2[81]=0;
            OurResult b=our_solve(pz2); if(!b.solved){ok=0;break;} can[t]=b.guesses;
        }
        if(!ok)continue;
        nn++;
        /* 规范化后 T 个值是否全相同 */
        for(int t=1;t<T;t++) if(can[t]!=can[0]){diffCnt++;break;}
        double m1=0,m2=0;for(int t=0;t<T;t++){m1+=raw[t];m2+=can[t];}
        m1/=T;m2/=T;
        double v1=0,v2=0;for(int t=0;t<T;t++){v1+=(raw[t]-m1)*(raw[t]-m1);v2+=(can[t]-m2)*(can[t]-m2);}
        v1/=T;v2/=T;
        cvRaw+=sqrt(v1)/(m1>0?m1:1);cvCan+=sqrt(v2)/(m2>0?m2:1);
    }
    printf("  %-34s %10s %10s\n","","不规范化","规范化后");
    printf("  %s\n","----------------------------------------------------");
    printf("  %-34s %10.4f %10.4f\n","平均 CV",cvRaw/(nn?nn:1),cvCan/(nn?nn:1));
    printf("  %-34s %34d\n","规范化后组内出现差异的题数",diffCnt);
    printf("  %-34s %34d\n","有效题数",nn);
    printf("\n  diffCnt==0 表示 CV 精确 = 0 (不是很小, 是完全相同)\n");
    return 0;
}
