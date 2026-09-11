/* collide.c — 找出候选矩阵碰撞的具体题目对 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define ALL 0x1FFu
static int ROWOF[81],COLOF[81],BOXOF[81],UNITS[27][9];
static void init_tables(void){
    for(int i=0;i<81;i++){ROWOF[i]=i/9;COLOF[i]=i%9;BOXOF[i]=(i/27)*3+(i%9)/3;}
    for(int r=0;r<9;r++)for(int c=0;c<9;c++)UNITS[r][c]=r*9+c;
    for(int c=0;c<9;c++)for(int r=0;r<9;r++)UNITS[9+c][r]=r*9+c;
    for(int b=0;b<9;b++){int k=0;for(int i=0;i<81;i++)if(BOXOF[i]==b)UNITS[18+b][k++]=i;}
}
static unsigned cand[81],rowm[9],colm[9],boxm[9]; static int g[81];
static void build_cand(void){
    memset(rowm,0,sizeof(rowm));memset(colm,0,sizeof(colm));memset(boxm,0,sizeof(boxm));
    for(int i=0;i<81;i++)if(g[i]){unsigned b=1u<<(g[i]-1);cand[i]=0;
        rowm[ROWOF[i]]|=b;colm[COLOF[i]]|=b;boxm[BOXOF[i]]|=b;}
    for(int i=0;i<81;i++)if(!g[i])cand[i]=ALL&~(rowm[ROWOF[i]]|colm[COLOF[i]]|boxm[BOXOF[i]]);
}
static void candmat_key(char*out){
    int p=0;
    for(int i=0;i<81;i++){unsigned c=cand[i];
        out[p++]=(char)('0'+(c%50)); c/=50;
        out[p++]=(char)('0'+(c%50)); c/=50;
        out[p++]=(char)('0'+c);}
    out[p]=0;
}
typedef struct{uint64_t a,b;} H128;
static H128 mk2(const char*s){
    uint64_t h1=1469598103934665603ULL,h2=0x9E3779B97F4A7C15ULL;
    while(*s){ unsigned char c=(unsigned char)*s;
        h1^=c; h1*=1099511628211ULL;
        h2^=c; h2*=0x100000001B3ULL; h2^=h2>>29; s++; }
    return (H128){h1?h1:1,h2?h2:2};
}
#define HS 4000037
static H128 TAB[HS]; static int IDX[HS];
static unsigned char (*GRIDS)[81]; static int nstored;
static long hist_nd[20],hist_ur,hist_2val,hist_both;
static void print_grid(int idx){
    printf("    题#%d: ",idx);
    for(int i=0;i<81;i++)putchar(GRIDS[idx][i]?'0'+GRIDS[idx][i]:'.');
    putchar('\n');
}
int main(int argc,char**argv){
    if(argc<3){return 1;}
    int NQ=atoi(argv[2]);
    init_tables();
    GRIDS=malloc(sizeof(*GRIDS)*(NQ+10)); nstored=0;
    FILE*f=fopen(argv[1],"r"); if(!f)return 1;
    char line[512]; int n=0, ncoll=0, shown=0;
    char kC[256];
    while(fgets(line,sizeof(line),f)&&n<NQ){
        int m=0;
        for(char*p=line;*p&&m<81;p++){
            if(*p>='1'&&*p<='9')g[m++]=*p-'0';
            else if(*p=='.'||*p=='0')g[m++]=0;}
        if(m!=81)continue;
        for(int i=0;i<81;i++)GRIDS[nstored][i]=(unsigned char)g[i];
        int myidx=nstored++; n++;
        build_cand(); candmat_key(kC);
        H128 h=mk2(kC); size_t k=(size_t)(h.a%HS);
        while(TAB[k].a){
            if(TAB[k].a==h.a&&TAB[k].b==h.b){
                ncoll++;
                {
                    int nd=0; int rows[81],cols[81]; unsigned vmask=0;
                    for(int i=0;i<81;i++) if(GRIDS[IDX[k]][i]!=GRIDS[myidx][i]){
                        rows[nd]=ROWOF[i];cols[nd]=COLOF[i];
                        vmask|=(1u<<GRIDS[IDX[k]][i]); vmask|=(1u<<GRIDS[myidx][i]);
                        nd++;}
                    int ur=0;
                    if(nd==4){
                        int rr[2]={rows[0],-1},cc[2]={cols[0],-1};
                        for(int z=1;z<4;z++){
                            if(rows[z]!=rr[0]&&rr[1]<0)rr[1]=rows[z];
                            if(cols[z]!=cc[0]&&cc[1]<0)cc[1]=cols[z];}
                        int okr=0,okc=0;
                        for(int z=0;z<4;z++){
                            if(rows[z]==rr[0]||rows[z]==rr[1])okr++;
                            if(cols[z]==cc[0]||cols[z]==cc[1])okc++;}
                        /* size-4 unavoidable set: 2行x2列矩形 (宫内或跨宫均可)
                           交换两个数字后 每行/每列/每宫 的数字集合都不变 */
                        if(okr==4&&okc==4&&rr[1]>=0&&cc[1]>=0)ur=1;
                    }
                    int ur6=0;
                    if(nd==6){
                        /* 统计不同的行/列数 */
                        int nr=0,nc=0,ur_[9],uc_[9];
                        for(int z=0;z<9;z++){ur_[z]=0;uc_[z]=0;}
                        for(int z=0;z<nd;z++){ur_[rows[z]]=1;uc_[cols[z]]=1;}
                        for(int z=0;z<9;z++){nr+=ur_[z];nc+=uc_[z];}
                        if((nr==3&&nc==2)||(nr==2&&nc==3)) ur6=1;
                    }
                    int nv=__builtin_popcount(vmask);
                    hist_nd[nd<20?nd:19]++;
                    if(ur||ur6)hist_ur++;
                    if(nv==2)hist_2val++;
                    if((ur||ur6)&&nv==2)hist_both++;
                    if(shown<3){ shown++;
                        printf("  --- 碰撞对 #%d (题%d 与 题%d) ---\n",ncoll,IDX[k],myidx);
                        print_grid(IDX[k]); print_grid(myidx);
                        printf("    差异 %d 格, 宫内2x2矩形=%s, 涉及%d个数字\n\n",nd,ur?"是":"否",nv);
                    }
                }
                goto nextline;
            }
            k=(k+1)%HS;
        }
        TAB[k]=h; IDX[k]=myidx;
        nextline: ;
    }
    fclose(f);
    printf("  总碰撞 %d 对\n\n",ncoll);
    printf("  === 碰撞对的模式分析 ===\n");
    printf("  差异格数分布:\n");
    for(int z=0;z<20;z++) if(hist_nd[z]) printf("    %2d 格: %ld 对\n",z,hist_nd[z]);
    printf("\n  其中:\n");
    printf("    构成矩形unavoidable set: %ld 对 (%.1f%%)\n",hist_ur,100.0*hist_ur/ncoll);
    printf("    只涉及 2 个数字      : %ld 对 (%.1f%%)\n",hist_2val,100.0*hist_2val/ncoll);
    printf("    = unavoidable set 互换: %ld 对 (%.1f%%)\n",hist_both,100.0*hist_both/ncoll);
    return 0;
}
