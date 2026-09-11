/* canon_test.c — 规范化 × 当前最优 5 键配方
 *
 * 2.41 当时测的是老策略(3键), 劣化 1~6%
 * 现在 5 键配方把平局率压得很低 => 规范化的"索引序任意"代价应该变小
 * 假设: 规范化 + 5键 => CV 精确 0, 且性能劣化 < 1%
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define NP 1296
static const int P3[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static int ROWP[NP][9];
static void genperms(void){
    int p=0;
    for(int b=0;b<6;b++)for(int i0=0;i0<6;i0++)for(int i1=0;i1<6;i1++)for(int i2=0;i2<6;i2++){
        int *r=ROWP[p++]; int k=0;
        for(int bb=0;bb<3;bb++){
            int band=P3[b][bb];
            const int *in = (bb==0?P3[i0]:(bb==1?P3[i1]:P3[i2]));
            for(int j=0;j<3;j++) r[k++]=band*3+in[j];
        }
    }
}
static int best[81];
static void canon_grid(const int*g,int*out){
    int hasbest=0; int tmp[81], rowg[81], tmp2[81];
    for(int tr=0;tr<2;tr++){
        for(int i=0;i<81;i++) tmp[i] = tr ? g[(i%9)*9+i/9] : g[i];
        for(int rp=0;rp<NP;rp++){
            const int*rp9=ROWP[rp];
            for(int i=0;i<81;i++) rowg[i]=tmp[rp9[i/9]*9+(i%9)];
            for(int cp=0;cp<NP;cp++){
                const int*cp9=ROWP[cp];
                int fr[9], map[10], nm=0;
                for(int i=0;i<10;i++)map[i]=0;
                for(int c=0;c<9;c++){int d=rowg[cp9[c]];
                    if(d){ if(!map[d])map[d]=++nm; fr[c]=map[d]; } else fr[c]=0;}
                int cmp=0;
                if(!hasbest) cmp=-1;
                else for(int c=0;c<9;c++) if(fr[c]!=best[c]){ cmp=fr[c]<best[c]?-1:1; break; }
                if(cmp<=0){
                    for(int i=0;i<10;i++)map[i]=0; nm=0;
                    for(int i=0;i<81;i++){int d=rowg[(i/9)*9+cp9[i%9]];
                        if(d){ if(!map[d])map[d]=++nm; tmp2[i]=map[d]; } else tmp2[i]=0;}
                    if(cmp<0){ memcpy(best,tmp2,sizeof(int)*81); hasbest=1; }
                    else{ int c2=0;
                        for(int i=0;i<81;i++) if(tmp2[i]!=best[i]){ c2=tmp2[i]<best[i]?-1:1; break; }
                        if(c2<0) memcpy(best,tmp2,sizeof(int)*81); }
                }
            }
        }
    }
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
static char line[256]; static int grids[60000][81]; static int nq=0;
static int load(const char*fn,int maxq){
    FILE*f=fopen(fn,"r"); if(!f)return 0; nq=0;
    while(fgets(line,sizeof(line),f)&&nq<maxq){
        int n=0; for(int i=0;i<81;i++){ char ch=0;
            while(line[n]&&(line[n]<'0'||line[n]>'9')&&line[n]!='.')n++;
            if(!line[n])break; ch=line[n++]; grids[nq][i]=(ch>='0'&&ch<='9')?ch-'0':0; }
        if(n>0||line[0]) nq++;
    }
    fclose(f); return nq;
}
int main(int argc,char**argv){
    genperms();
    const char*fn=argc>1?argv[1]:"forum_hardest_1905_11plus.txt";
    int maxq=argc>2?atoi(argv[2]):1000;
    if(!load(fn,maxq)){printf("加载失败\n");return 1;}
    our_init(); int keys[5]={0,1,2,3,4}; our_set_strategy(10,1,5,keys,1);
    printf("=== 规范化 × 5键配方 (%s, N=%d) ===\n\n",fn,nq);
    double s1=0,s2=0; int n1=0,n2=0; long t1=0,t2=0;
    int cg[81]; char pz[82];
    for(int q=0;q<nq;q++){
        for(int i=0;i<81;i++) pz[i]=grids[q][i]?('0'+grids[q][i]):'.';
        pz[81]=0;
        OurResult a=our_solve(pz);
        if(a.solved){ s1+=log((double)a.guesses+1.0); n1++; t1+=a.guesses; }
        canon_grid(grids[q],cg);
        char pz2[82];
        for(int i=0;i<81;i++) pz2[i]=cg[i]?('0'+cg[i]):'.';
        pz2[81]=0;
        OurResult b=our_solve(pz2);
        if(b.solved){ s2+=log((double)b.guesses+1.0); n2++; t2+=b.guesses; }
    }
    printf("  %-28s %10s %10s\n","","原盘","规范化后");
    printf("  %s\n","----------------------------------------------------");
    printf("  %-28s %10d %10d\n","解出",n1,n2);
    printf("  %-28s %10.2f %10.2f\n","平均 guesses",(double)t1/nq,(double)t2/nq);
    printf("  %-28s %10.2f %10.2f\n","几何均值",exp(s1/(n1?n1:1))-1,exp(s2/(n2?n2:1))-1);
    printf("  %-28s %10s %9.3f\n","规范化/原盘","1.000",
        (exp(s1/(n1?n1:1))-1)?(exp(s2/(n2?n2:1))-1)/(exp(s1/(n1?n1:1))-1):0);
    printf("\n  规范化保证 CV=0 (2.41 已证, 10 种策略全部精确 0.0000)\n");
    printf("  本实验只问: 现在的 5 键配方下, 性能的代价还有多大?\n");
    return 0;
}
