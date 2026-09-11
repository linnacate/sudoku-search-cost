#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
using namespace std::chrono;
extern "C" {
  typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
  void our_init(void); OurResult our_solve(const char*p);
  void our_set_strategy(int,int,int,const int*,int);
  void our_set_limit(int); void our_set_locked(int); void our_set_splevel(int);
  int our_first_pick(void); int our_prop_only(const char*);
}
int main(int argc,char**argv){
    const char*fn=argc>1?argv[1]:"../sample5000.txt"; int maxq=argc>2?atoi(argv[2]):2000;
    FILE*f=fopen(fn,"r"); std::vector<std::string> P; char buf[512];
    while(fgets(buf,sizeof(buf),f)&&(int)P.size()<maxq){
        if(strlen(buf)<81)continue; std::string s(buf);
        while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();
        if(s.size()>=81)P.push_back(s.substr(0,81));
    } fclose(f);
    int N=P.size();
    our_init(); int keys[5]={0,1,2,3,4};
    our_set_strategy(10,1,5,keys,1); our_set_limit(1); our_set_locked(argc>3?atoi(argv[3]):1); our_set_splevel(argc>4?atoi(argv[4]):2);
    long tg=0,tf=0,tr=0; int ns=0;
    auto t0=steady_clock::now();
    for(int q=0;q<N;q++){ OurResult r=our_solve(P[q].c_str());
        tg+=r.guesses; tf+=r.fill; tr+=r.rounds; ns+=r.solved; }
    auto t1=steady_clock::now();
    double us=duration_cast<nanoseconds>(t1-t0).count()/1000.0;
    printf("N=%d solved=%d\n",N,ns);
    printf("总时间      %.1f us/题\n",us/N);
    printf("guesses/题  %.2f      -> per-guess %.3f us\n",(double)tg/N, us/tg);
    printf("fill/题     %.2f      -> per-fill  %.3f us\n",(double)tf/N, us/tf);
    printf("rounds/题   %.2f      -> per-round %.3f us\n",(double)tr/N, us/tr);
    printf("fill/guess  %.2f   rounds/guess %.2f\n",(double)tf/tg,(double)tr/tg);
    return 0;
}
