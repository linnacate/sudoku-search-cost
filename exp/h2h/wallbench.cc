/* wallbench.cc — 同进程、同数据、同口径的墙钟时间对比
 *
 * 方法（对齐 tdoku 官方 run_benchmark 的精神）：
 *   - 题目全部预读入内存，计时段不含 I/O
 *   - 预热一轮后，每方重复 REP 次取【最小总时间】（抵抗调度噪声）
 *   - 三方口径统一：limit=1（找第一个解），均报 guesses 供交叉校验
 *
 * 输出: 各方 us/题、相对倍数、以及 guesses 均值（用于确认口径未变）
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
using namespace std::chrono;

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

static double now_us(){
    return (double)duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()).count()/1000.0;
}

int main(int argc,char**argv){
    const char*fn   = argc>1? argv[1] : "sample5000.txt";
    int maxq        = argc>2? atoi(argv[2]) : 2000;
    int rep         = argc>3? atoi(argv[3]) : 3;
    int locked_on   = argc>4? atoi(argv[4]) : 1;
    int splevel     = argc>5? atoi(argv[5]) : 2;

    /* ---- 读题 ---- */
    FILE*f=fopen(fn,"r"); if(!f){printf("打不开 %s\n",fn);return 1;}
    std::vector<std::string> P; char buf[512];
    while(fgets(buf,sizeof(buf),f)&&(int)P.size()<maxq){
        if(strlen(buf)<81)continue;
        std::string s(buf);
        while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();
        if(s.size()>=81) P.push_back(s.substr(0,81));
    }
    fclose(f);
    int N=P.size();
    if(N==0){printf("无题目\n");return 1;}
    printf("数据集 %s  N=%d  rep=%d  locked=%d  splevel=%d\n",fn,N,rep,locked_on,splevel);

    our_init();
    int keys[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,keys,1);
    our_set_limit(1);
    our_set_locked(locked_on);
    our_set_splevel(splevel);

    /* fsss 需要 unsigned char 形态，预转换 */
    std::vector<std::vector<unsigned char>> FP(N);
    for(int q=0;q<N;q++){
        FP[q].resize(82);
        for(int i=0;i<81;i++){char ch=P[q][i]; FP[q][i]=(ch>='1'&&ch<='9')?(ch-'0'):0;}
        FP[q][81]=0;
    }

    char sol[128];
    long so=0,st=0,sf=0;
    double bo=1e18, bt=1e18, bf=1e18;

    /* ---- 预热 ---- */
    for(int q=0;q<std::min(N,200);q++){
        our_solve(P[q].c_str());
        size_t ng=0; TdokuSolverDpllTriadSimd(P[q].c_str(),1,0,sol,&ng);
        size_t nf=0; fsss_solve((const char*)FP[q].data(),sol,&nf,0);
    }

    /* ---- 计时 ---- */
    for(int r=0;r<rep;r++){
        double t0=now_us();
        for(int q=0;q<N;q++){ OurResult rr=our_solve(P[q].c_str()); if(r==0)so+=rr.guesses; }
        double t1=now_us();
        for(int q=0;q<N;q++){ size_t ng=0; TdokuSolverDpllTriadSimd(P[q].c_str(),1,0,sol,&ng); if(r==0)st+=(long)ng; }
        double t2=now_us();
        for(int q=0;q<N;q++){ size_t nf=0; fsss_solve((const char*)FP[q].data(),sol,&nf,0); if(r==0)sf+=(long)nf; }
        double t3=now_us();
        bo=std::min(bo,t1-t0); bt=std::min(bt,t2-t1); bf=std::min(bf,t3-t2);
        fprintf(stderr,"  rep%d: our=%.1f us/题  tdoku=%.1f  fsss=%.1f\n",
                r,(t1-t0)/N,(t2-t1)/N,(t3-t2)/N);
    }

    printf("\n%-8s %12s %12s %10s\n","","us/题","guesses/题","相对ours");
    printf("%-8s %12.2f %12.2f %10s\n","ours",   bo/N,(double)so/N,"1.000");
    printf("%-8s %12.2f %12.2f %10.3f\n","tdoku", bt/N,(double)st/N,(bt/N)/(bo/N));
    printf("%-8s %12.2f %12.2f %10.3f\n","fsss",  bf/N,(double)sf/N,(bf/N)/(bo/N));
    printf("\nours/tdoku = %.2fx  (tdoku 比我们快 %.2f 倍)\n",(bo/N)/(bt/N),(bo/N)/(bt/N));
    printf("每次猜测耗时: ours=%.2f us  tdoku=%.2f us  fsss=%.2f us\n",
           (bo/N)/((double)so/N),(bt/N)/((double)st/N),(bf/N)/((double)sf/N));
    return 0;
}
