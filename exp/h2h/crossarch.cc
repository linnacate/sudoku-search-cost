/* crossarch.cc — 逐题三方代价矩阵（论文 §6.2.1 / §6.2.4）
 *
 * 口径：
 *   A_best = (L2 传播 singles+locked+naked, crit 过滤, 全5键打分, la0opt), limit=1
 *   tdoku  = TdokuSolverDpllTriadSimd（主求解器 DPLL-Triad-SCC/SIMD，非 basic 变体）, limit=1
 *   fsss   = fsss_solve(mode=0)
 * 三列均为"猜测数/题"，内部相对口径。
 *
 * 输出: 每题一行 TSV -> crossarch.tsv (q, our, tdoku, fsss)
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
extern "C" {
  size_t TdokuSolverDpllTriadSimd(const char*, size_t, uint32_t, char*, size_t*);
  int fsss_solve(const char* in, char* out, std::size_t *num_guesses, const int mode);
}
extern "C" {
  typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
  void our_init(void);
  OurResult our_solve(const char*puzzle);
  void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
  void our_set_limit(int lim);
  void our_set_locked(int on);
  void our_set_splevel(int lv);
}
int main(int argc,char**argv){
    const char*fn = argc>1? argv[1] : "sample5000.txt";
    int maxq = argc>2? atoi(argv[2]) : 5000;
    const char*out = argc>3? argv[3] : "crossarch.tsv";
    FILE*f=fopen(fn,"r"); if(!f){printf("打不开 %s\n",fn);return 1;}
    std::vector<std::string> P; char buf[512];
    while(fgets(buf,sizeof(buf),f)&&(int)P.size()<maxq){
        if(strlen(buf)<81)continue;
        std::string s(buf); while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();
        if(s.size()>=81) P.push_back(s.substr(0,81));
    }
    fclose(f);
    int N=P.size();
    fprintf(stderr,"N=%d\n",N);

    our_init();
    int keys[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,keys,1);   /* crit 过滤 + 全5键 + la0opt */
    our_set_limit(1);                   /* 找第一个解 */
    our_set_locked(1);
    our_set_splevel(2);                 /* L2 = singles + locked + naked */

    FILE*g=fopen(out,"w");
    fprintf(g,"q\tour\ttdoku\tfsss\n");
    long so=0,st=0,sf=0; int no=0,nt=0,nf=0;
    unsigned char pin[82]; char sol[128];
    for(int q=0;q<N;q++){
        OurResult r=our_solve(P[q].c_str());
        long go=r.guesses; if(r.solved)no++; so+=go;

        std::size_t ng=0;
        size_t c=TdokuSolverDpllTriadSimd(P[q].c_str(),1,0,sol,&ng);
        long gt=(long)ng; if(c>0)nt++; st+=gt;

        for(int i=0;i<81;i++){ char ch=P[q][i]; pin[i]=(ch>='1'&&ch<='9')?(ch-'0'):0; }
        pin[81]=0;
        std::size_t nf2=0; int cf=fsss_solve((const char*)pin,sol,&nf2,0);
        long gf=(long)nf2; if(cf>0)nf++; sf+=gf;

        fprintf(g,"%d\t%ld\t%ld\t%ld\n",q,go,gt,gf);
        if((q+1)%1000==0)fprintf(stderr,"  ...%d\n",q+1);
    }
    fclose(g);
    printf("solved: our=%d tdoku=%d fsss=%d / N=%d\n",no,nt,nf,N);
    printf("mean  : our=%.2f tdoku=%.2f fsss=%.2f\n",(double)so/N,(double)st/N,(double)sf/N);
    return 0;
}
