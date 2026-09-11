#include <stdio.h>
#include <stdlib.h>
#include <string.h>
size_t JCZSolve_guesses=0;
size_t JCZSolve_nodes=0;
extern int JCZSolver(const char*puzzle,char*solution,int limit);
int main(int argc,char**argv){
    const char*fn=argc>1?argv[1]:"sample5000.txt";
    int N=argc>2?atoi(argv[2]):300;
    int limit=argc>3?atoi(argv[3]):1;
    FILE*f=fopen(fn,"r"); if(!f){printf("no file\n");return 1;}
    char buf[512], sol[128]; long tot_try=0,tot_node=0; int n=0,solved=0;
    while(fgets(buf,sizeof buf,f)&&n<N){
        int L=(int)strcspn(buf,"\r\n"); if(L<81)continue;
        const char*st=buf+(L-81); char pz[82]; memcpy(pz,st,81); pz[81]=0;
        for(int i=0;i<81;i++) if(pz[i]=='.') pz[i]='0';
        JCZSolve_guesses=0; JCZSolve_nodes=0;
        int ns=JCZSolver(pz,sol,limit);
        if(ns>0) solved++;
        tot_try+=JCZSolve_guesses; tot_node+=(long)JCZSolve_nodes-1;
        n++;
    }
    printf("jczsolve  %-28s N=%4d limit=%d solved=%4d | attempts=%9ld (%.2f) | survived(=our口径)=%9ld (%.2f)\n",
        fn,n,limit,solved,tot_try,(double)tot_try/n,tot_node,(double)tot_node/n);
    return 0;
}
