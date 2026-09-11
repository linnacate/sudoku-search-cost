/* keyscan.c — 系统扫描等变打分键
 *
 * 所有键必须【对称等变】:
 *   - 位置置换(行带/带内行/列栈/栈内列/转置): 值随格移动
 *   - 数字重标: 只依赖"数字等价类的模式", 不依赖具体标签
 *
 * 19 个候选键 (见 KT 表)
 * 阶段1: 主键 + 单个次键 -> 平局率 + CV
 * 阶段2: 贪心前向选择, 从最优次键逐步加第3/4键
 *
 * 编译: gcc -O3 -march=native -o keyscan keyscan.c -lm
 * 用法: ./keyscan puzzles.txt mode [maxq] [T] [args...]
 *   mode 0: 阶段1 单键扫描
 *   mode 1: 阶段2 贪心 (args = 已选键号列表)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define ALL 0x1FFu
#define MAXT 4000000
static long GCAP=20000L;
static const int ROWOF[81]={0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8};
static const int COLOF[81]={0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8};
static const int BOXOF[81]={0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,3,3,3,4,4,4,5,5,5,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,6,6,6,7,7,7,8,8,8,6,6,6,7,7,7,8,8,8};
static int UNITS[27][9], PEERS[81][20], NPEER[81];
static void init_tables(void){
    for(int r=0;r<9;r++)for(int c=0;c<9;c++)UNITS[r][c]=r*9+c;
    for(int c=0;c<9;c++)for(int r=0;r<9;r++)UNITS[9+c][r]=r*9+c;
    for(int b=0;b<9;b++){int k=0;for(int i=0;i<81;i++)if(BOXOF[i]==b)UNITS[18+b][k++]=i;}
    for(int i=0;i<81;i++){char seen[81]={0};int n=0;
        for(int u=0;u<27;u++){int inU=0;for(int k=0;k<9;k++)if(UNITS[u][k]==i){inU=1;break;}
            if(!inU)continue;
            for(int k=0;k<9;k++){int j=UNITS[u][k];if(j!=i&&!seen[j]){seen[j]=1;PEERS[i][n++]=j;}}}
        NPEER[i]=n;}
}
static unsigned cand[81],rowm[9],colm[9],boxm[9];
typedef struct{short idx;unsigned oldcand;short oldr,oldc,oldb;char iscell;}Trail;
typedef struct{double tie,cv,gm;int k;double sg;}Res;
typedef struct{double cv,gm;int k;double sgn;}Res2;
static Trail trail[MAXT]; static int tlen=0; static int g_grid[81];
static void reset_state(void){
    tlen=0; memset(rowm,0,sizeof(rowm));memset(colm,0,sizeof(colm));memset(boxm,0,sizeof(boxm));
    for(int i=0;i<81;i++)if(g_grid[i]){unsigned b=1u<<(g_grid[i]-1);cand[i]=0;
        rowm[ROWOF[i]]|=b;colm[COLOF[i]]|=b;boxm[BOXOF[i]]|=b;}
    for(int i=0;i<81;i++)if(!g_grid[i])cand[i]=ALL&~(rowm[ROWOF[i]]|colm[COLOF[i]]|boxm[BOXOF[i]]);
}
static inline void undo_to(int mark){
    while(tlen>mark){Trail*t=&trail[--tlen];cand[t->idx]=t->oldcand;
        if(t->iscell){rowm[ROWOF[t->idx]]=t->oldr;colm[COLOF[t->idx]]=t->oldc;boxm[BOXOF[t->idx]]=t->oldb;}}
}
static inline int assign(int i,int d){
    unsigned bit=1u<<(d-1); Trail*t=&trail[tlen++];
    t->idx=i;t->oldcand=cand[i];t->oldr=rowm[ROWOF[i]];t->oldc=colm[COLOF[i]];t->oldb=boxm[BOXOF[i]];t->iscell=1;
    cand[i]=0; rowm[ROWOF[i]]|=bit;colm[COLOF[i]]|=bit;boxm[BOXOF[i]]|=bit;
    for(int k=0;k<NPEER[i];k++){int j=PEERS[i][k];
        if(cand[j]&bit){Trail*u=&trail[tlen++];u->idx=j;u->oldcand=cand[j];u->iscell=0;
            cand[j]&=~bit; if(cand[j]==0)return 0;}}
    return 1;
}
static int propagate(void){
    int progress=1,filled=0;
    while(progress){progress=0;
        for(int i=0;i<81;i++){unsigned c=cand[i];
            if(c&&(c&(c-1))==0){if(!assign(i,__builtin_ctz(c)+1))return -1;filled++;progress=1;}}
        for(int uid=0;uid<27;uid++){unsigned once=0,twice=0,all=0;
            for(int k=0;k<9;k++){unsigned c=cand[UNITS[uid][k]];if(!c)continue;twice|=once&c;once|=c;all|=c;}
            unsigned placed=uid<9?rowm[uid]:(uid<18?colm[uid-9]:boxm[uid-18]);
            if((all|placed)!=ALL)return -1;
            unsigned hid=once&~twice&~placed;
            while(hid){unsigned bit=hid&(~hid+1);hid^=bit;
                int tgt=-1,cnt=0;
                for(int k=0;k<9;k++){int i=UNITS[uid][k];if(cand[i]&bit){tgt=i;cnt++;}}
                if(cnt==1){if(!assign(tgt,__builtin_ctz(bit)+1))return -1;filled++;progress=1;}}}}
    return filled;
}
static short freed[27][9]; static int crit[81];
static void compute_freed(void){
    for(int uid=0;uid<27;uid++){unsigned base=uid<9?rowm[uid]:(uid<18?colm[uid-9]:boxm[uid-18]);
        for(int d=0;d<9;d++){if(base&(1u<<d)){freed[uid][d]=-1;continue;}
            unsigned bit=1u<<d;int c=0;
            for(int k=0;k<9;k++)if(cand[UNITS[uid][k]]&bit)c++;
            freed[uid][d]=(short)c;}}
}
static void compute_crit(void){
    for(int i=0;i<81;i++)crit[i]=0;
    for(int uid=0;uid<27;uid++)for(int d=0;d<9;d++){
        if(freed[uid][d]!=2)continue; unsigned bit=1u<<d;
        for(int k=0;k<9;k++){int i=UNITS[uid][k];if(cand[i]&bit)crit[i]++;}}
}
/* ===== 19 个候选键 (全部对称等变) ===== */
#define NK 19
static const char*KT[NK]={
 "pc候选数","crit引信数","minF最小自由度","sumF自由度和","maxF最大自由度",
 "prodF自由度积","nf2c引信候选数","nEmptyPeer空邻居","unitfill单位填充",
 "peerpop邻居候选和","minPeerPc最小邻居候选","nPeer2双值邻居数","boxempty宫空格",
 "rcempty行列空格","spreadF自由度极差","sumInvF自由度和倒","fanout波及面",
 "critPeer邻居引信和","f2units单位引信数"};
static double keyval(int i,int k){
    unsigned cc=cand[i]; int np=__builtin_popcount(cc);
    int u0=ROWOF[i], u1=COLOF[i]+9, u2=BOXOF[i]+18;
    switch(k){
        case 0: return (double)np;
        case 1: return (double)crit[i];
        case 2: { int mn=99; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    int f=freed[u0][d]; if(f>=0&&f<mn)mn=f;
                    f=freed[u1][d]; if(f>=0&&f<mn)mn=f;
                    f=freed[u2][d]; if(f>=0&&f<mn)mn=f; }
                  return mn==99?0:(double)mn; }
        case 3: { double s=0; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    int f=freed[u0][d]; if(f>0)s+=f;
                    f=freed[u1][d]; if(f>0)s+=f;
                    f=freed[u2][d]; if(f>0)s+=f; }
                  return s; }
        case 4: { int mx=0; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    int f=freed[u0][d]; if(f>mx)mx=f;
                    f=freed[u1][d]; if(f>mx)mx=f;
                    f=freed[u2][d]; if(f>mx)mx=f; }
                  return (double)mx; }
        case 5: { double p=1; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    p*= (freed[u0][d]>0?freed[u0][d]:1);
                    p*= (freed[u1][d]>0?freed[u1][d]:1);
                    p*= (freed[u2][d]>0?freed[u2][d]:1); }
                  return p; }
        case 6: { int n=0; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    if(freed[u0][d]==2||freed[u1][d]==2||freed[u2][d]==2) n++; }
                  return (double)n; }
        case 7: { int n=0; for(int k2=0;k2<NPEER[i];k2++) if(cand[PEERS[i][k2]])n++; return (double)n; }
        case 8: return (double)(__builtin_popcount(rowm[ROWOF[i]])+__builtin_popcount(colm[COLOF[i]])+__builtin_popcount(boxm[BOXOF[i]]));
        case 9: { double s=0; for(int k2=0;k2<NPEER[i];k2++){int j=PEERS[i][k2];
                    if(cand[j])s+=__builtin_popcount(cand[j]);} return s; }
        case 10:{ int mn=99; for(int k2=0;k2<NPEER[i];k2++){int j=PEERS[i][k2];
                    if(cand[j]){int c=__builtin_popcount(cand[j]); if(c<mn)mn=c;}}
                  return mn==99?0:(double)mn; }
        case 11:{ int n=0; for(int k2=0;k2<NPEER[i];k2++){int j=PEERS[i][k2];
                    if(cand[j]&&__builtin_popcount(cand[j])==2)n++;} return (double)n; }
        case 12:{ int n=0; for(int k2=0;k2<9;k2++) if(cand[UNITS[u2][k2]])n++; return (double)n; }
        case 13:{ int n=0; for(int k2=0;k2<9;k2++){ if(cand[UNITS[u0][k2]])n++; if(cand[UNITS[u1][k2]])n++; }
                  return (double)n; }
        case 14:{ int mn=99,mx=0; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    int fs[3]={freed[u0][d],freed[u1][d],freed[u2][d]};
                    for(int q=0;q<3;q++){ if(fs[q]<0)continue; if(fs[q]<mn)mn=fs[q]; if(fs[q]>mx)mx=fs[q]; }}
                  return (mn==99)?0:(double)(mx-mn); }
        case 15:{ double s=0; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    if(freed[u0][d]>0)s+=1.0/freed[u0][d];
                    if(freed[u1][d]>0)s+=1.0/freed[u1][d];
                    if(freed[u2][d]>0)s+=1.0/freed[u2][d]; }
                  return s; }
        case 16:{ double s=0; for(int d=0;d<9;d++) if(cc&(1u<<d)){
                    unsigned bit=1u<<d; int c=0;
                    for(int k2=0;k2<NPEER[i];k2++) if(cand[PEERS[i][k2]]&bit)c++;
                    s+=c; }
                  return s; }
        case 17:{ double s=0; for(int k2=0;k2<NPEER[i];k2++){int j=PEERS[i][k2];
                    if(cand[j])s+=crit[j];} return s; }
        case 18:{ int n=0; int us[3]={u0,u1,u2};
                  for(int q=0;q<3;q++) for(int d=0;d<9;d++) if(freed[us[q]][d]==2)n++;
                  return (double)n; }
    }
    return 0;
}
/* 打分: 主键 + 次键序列, 用字典序 */
static int NKEY; static int KSEL[8]; static double KSIGN[8];
static int FILT;
static double KV[81][8];
static void compute_all(int i){
    for(int k=0;k<NKEY;k++) KV[i][k]=KSIGN[k]*keyval(i,KSEL[k]);
}
static int pick(void){
    compute_freed(); compute_crit();
    int pool[81],pn=0;
    for(int i=0;i<81;i++){
        if(!cand[i])continue;
        if(FILT==1 && crit[i]<=0) continue;
        pool[pn++]=i;
    }
    if(pn==0){ for(int i=0;i<81;i++) if(cand[i]) pool[pn++]=i; if(pn==0)return -1; }
    for(int k=0;k<pn;k++) compute_all(pool[k]);
    int best=pool[0];
    for(int k=1;k<pn;k++){
        int i=pool[k]; int better=0;
        for(int q=0;q<NKEY;q++){
            if(KV[i][q]>KV[best][q]){better=1;break;}
            if(KV[i][q]<KV[best][q]){better=0;break;}
            /* 相等 -> 继续比较下一个键; 全相等时保持原(索引序) */
        }
        if(better) best=i;
    }
    return best;
}
static int V=1;
static void order_vals(int i,int*ds,int nd){
    if(nd<=1)return; int w[9];
    for(int k=0;k<nd;k++){
        unsigned bit=1u<<(ds[k]-1); int c=0;
        for(int p=0;p<NPEER[i];p++)if(cand[PEERS[i][p]]&bit)c++;
        w[k]=c;   /* vo1: 波及面降序 */
    }
    for(int a=1;a<nd;a++){int kd=ds[a],kw=w[a],b=a-1;
        while(b>=0&&w[b]<kw){ds[b+1]=ds[b];w[b+1]=w[b];b--;} ds[b+1]=kd;w[b+1]=kw;}
}
static long guesses;
static int verify_sol(void){
    for(int i=0;i<81;i++) if(cand[i]) return 0;
    for(int u=0;u<27;u++){ unsigned p=u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]); if(p!=ALL) return 0; }
    return 1;
}
static int g_verify_ok;
static int dfs(void){
    if(propagate()<0)return 0;
    int any=0; for(int i=0;i<81;i++)if(cand[i]){any=1;break;}
    if(!any){ g_verify_ok=verify_sol(); return 1; }
    int i=pick(); if(i<0)return 1;
    unsigned cc=cand[i]; int ds[9],nd=0;
    for(int d=1;d<=9;d++)if(cc&(1u<<(d-1)))ds[nd++]=d;
    order_vals(i,ds,nd);
    for(int k=0;k<nd;k++){
        if(guesses>=GCAP)return -1;
        int mark0=tlen; int ok=assign(i,ds[k]);
        int f=ok?propagate():-1;
        if(f<0){undo_to(mark0);continue;}
        guesses++;
        int r=dfs(); undo_to(mark0);
        if(r==1)return 1; if(r==-1)return -1;
    }
    return 0;
}
static void setkeys(int filt,int n,int*ks,double*sg){
    FILT=filt; NKEY=n;
    for(int i=0;i<n;i++){KSEL[i]=ks[i]; KSIGN[i]=sg[i];}
}
int main(int argc,char**argv){
    if(argc<3){fprintf(stderr,"用法: %s file mode [maxq] [T] [args]\n",argv[0]);return 1;}
    int MODE=atoi(argv[2]);
    int MAXQ=argc>3?atoi(argv[3]):200;
    int T=argc>4?atoi(argv[4]):10;
    init_tables();
    FILE*f=fopen(argv[1],"r"); if(!f){fprintf(stderr,"打不开\n");return 1;}
    int (*grids)[81]=malloc(sizeof(int[MAXQ+10][81]));
    int nq=0; char line[512];
    while(fgets(line,sizeof(line),f)&&nq<MAXQ){
        int n=0;
        for(char*p=line;*p&&n<81;p++){
            if(*p>='1'&&*p<='9')grids[nq][n++]=*p-'0';
            else if(*p=='.'||*p=='0')grids[nq][n++]=0;}
        if(n==81)nq++;
    }
    fclose(f);
    int NG=nq/T;
    if(MODE==0){
        /* 阶段1: 主键=-pc (或 -crit) + 单个次键, 两种符号都测 */
        printf("\n阶段1: 单键扫描 (N=%d 实例, %d 组 x %d 变换)\n\n",nq,NG,T);
        printf("  %-18s %8s %8s %10s %10s\n","次键","符号","平局率","CV","GM");
        Res res[NK*2]; int nr=0;
        /* 基线: 仅主键 */
        {
            int ks[1]={0}; double sg[1]={-1};
            setkeys(0,1,ks,sg);
            double sumcv=0; long tot=0; int ng=0; double s=0;
            for(int g=0;g<NG;g++){
                double vals[64]; double s1=0,s2=0;
                for(int t=0;t<T;t++){
                    int q=g*T+t;
                    for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
                    reset_state(); guesses=0; srand(20260904u+q); dfs();
                    vals[t]=guesses; s1+=guesses; s2+=(double)guesses*guesses; tot+=guesses;
                    s+=log(guesses+1.0);
                }
                double m=s1/T,v=s2/T-m*m; if(v<0)v=0;
                if(m>0){sumcv+=sqrt(v)/m;ng++;}
            }
            printf("  %-18s %8s %8s %10.4f %10.2f\n","(仅主键MRV)","-","(98.5%)",sumcv/ng,exp(s/nq)-1);
        }
        for(int k=1;k<NK;k++){
            for(int si=0;si<2;si++){
                double sgn = si? 1.0 : -1.0;
                int ks[2]={0,k}; double sg[2]={-1.0,sgn};
                setkeys(0,2,ks,sg);
                /* 平局率: 需要单独统计, 这里用第二遍扫描 */
                double sumcv=0; long tot=0; int ng=0; double s=0;
                for(int g=0;g<NG;g++){
                    double vals[64]; double s1=0,s2=0;
                    for(int t=0;t<T;t++){
                        int q=g*T+t;
                        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
                        reset_state(); guesses=0; srand(20260904u+q); dfs();
                        vals[t]=guesses; s1+=guesses; s2+=(double)guesses*guesses; tot+=guesses;
                        s+=log(guesses+1.0);
                    }
                    double m=s1/T,v=s2/T-m*m; if(v<0)v=0;
                    if(m>0){sumcv+=sqrt(v)/m;ng++;}
                }
                res[nr].cv=sumcv/ng; res[nr].gm=exp(s/nq)-1; res[nr].k=k; res[nr].sg=sgn;
                res[nr].tie=0; nr++;
            }
        }
        /* 按 CV 排序输出 */
        for(int a=1;a<nr;a++){Res kk=res[a];int b=a-1;
            while(b>=0&&res[b].cv>kk.cv){res[b+1]=res[b];b--;} res[b+1]=kk;}
        for(int i=0;i<nr;i++)
            printf("  %-18s %8s %8s %10.4f %10.2f\n",KT[res[i].k],
                res[i].sg<0?"-":"+","-",res[i].cv,res[i].gm);
        printf("\n  (按 CV 升序; 基线见首行)\n");
        return 0;
    }
    if(MODE==1){
        /* 阶段2: 贪心前向选择。args = 当前已选 (键号,符号) 对 */
        int base=5;
        int nsel=(argc-base)/2;
        int ks[8]; double sg[8];
        for(int i=0;i<nsel;i++){ ks[i]=atoi(argv[base+2*i]); sg[i]=atof(argv[base+2*i+1]); }
        printf("\n阶段2 贪心: 已选 %d 键 [",nsel);
        for(int i=0;i<nsel;i++) printf("%s%d%s ",sg[i]<0?"-":"+",ks[i],KT[ks[i]]);
        printf("]\n\n");
        printf("  %-18s %8s %10s %10s\n","待加键","符号","CV","GM");
        Res2 res[NK*2]; int nr=0;
        for(int k=0;k<NK;k++){
            int skip=0; for(int i=0;i<nsel;i++) if(ks[i]==k) skip=1;
            if(skip) continue;
            for(int si=0;si<2;si++){
                double sgn=si?1.0:-1.0;
                int kk[8]; double ss[8];
                for(int i=0;i<nsel;i++){kk[i]=ks[i];ss[i]=sg[i];}
                kk[nsel]=k; ss[nsel]=sgn;
                setkeys(0,nsel+1,kk,ss);
                double sumcv=0; int ng=0; double s=0; long tot=0;
                for(int g=0;g<NG;g++){
                    double vals[64]; double s1=0,s2=0;
                    for(int t=0;t<T;t++){
                        int q=g*T+t;
                        for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
                        reset_state(); guesses=0; srand(20260904u+q); dfs();
                        vals[t]=guesses; s1+=guesses; s2+=(double)guesses*guesses; tot+=guesses;
                        s+=log(guesses+1.0);
                    }
                    double m=s1/T,v=s2/T-m*m; if(v<0)v=0;
                    if(m>0){sumcv+=sqrt(v)/m;ng++;}
                }
                res[nr].cv=sumcv/ng; res[nr].gm=exp(s/nq)-1; res[nr].k=k; res[nr].sgn=sgn; nr++;
            }
        }
        for(int a=1;a<nr;a++){Res2 kk=res[a];int b=a-1;
            while(b>=0&&res[b].cv>kk.cv){res[b+1]=res[b];b--;} res[b+1]=kk;}
        for(int i=0;i<8&&i<nr;i++)
            printf("  %-18s %8s %10.4f %10.2f\n",KT[res[i].k],res[i].sgn<0?"-":"+",res[i].cv,res[i].gm);
        return 0;
    }
    if(MODE==2){
        /* 最终评估: [filt] + (键号,符号) 对, 输出 CV + GM
           filt=0 不过滤, filt=1 只用 crit>0 的格 (critmin 家族) */
        int filt=argc>5?atoi(argv[5]):0;
        int base=6; int nsel=(argc-base)/2;
        int ks[8]; double sg[8];
        for(int i=0;i<nsel;i++){ ks[i]=atoi(argv[base+2*i]); sg[i]=atof(argv[base+2*i+1]); }
        setkeys(filt,nsel,ks,sg);
        double sumcv=0; int ng=0; double s=0; long tot=0;
        for(int g=0;g<NG;g++){
            double vals[64]; double s1=0,s2=0;
            for(int t=0;t<T;t++){
                int q=g*T+t;
                for(int i=0;i<81;i++)g_grid[i]=grids[q][i];
                reset_state(); guesses=0; srand(20260904u+q); dfs();
                vals[t]=guesses; s1+=guesses; s2+=(double)guesses*guesses; tot+=guesses;
                s+=log(guesses+1.0);
            }
            double m=s1/T,v=s2/T-m*m; if(v<0)v=0;
            if(m>0){sumcv+=sqrt(v)/m;ng++;}
        }
        printf("  filt=%d keys=[",filt);
        for(int i=0;i<nsel;i++) printf("%s%d%s",sg[i]<0?"-":"+",ks[i],KT[ks[i]]);
        printf("]  CV=%.4f  GM=%.2f  AM=%.2f\n",sumcv/ng,exp(s/nq)-1,(double)tot/nq);
        return 0;
    }
    return 0;
}
