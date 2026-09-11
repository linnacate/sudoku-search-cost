/* crit 时效性探针 driver
 * 用法: ./critprobe <题库> [N]
 * 每题: (1) 求解取真解 (2) 设 refsol + 开探针再求解 (3) CRIT_STALE 端到端对照
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{ long guesses; long fill; long rounds; int solved; } OurResult;
extern void our_init(void);
extern OurResult our_solve(const char*);
extern void our_set_strategy(int st,int filt,int nk,const int*keys,int la);
extern void our_set_locked(int on);
extern void our_set_splevel(int lv);
extern void our_set_limit(int lim);
extern void our_last_sol(char*out);
extern void our_set_refsol(const char*sol);
extern void our_set_probe(int on);
extern void our_set_critstale(int on);
extern void our_probe_reset(void);
extern void our_probe_dump(void);
extern int our_last_nsols(void);

static char*read_lines(const char*fn,int*n,int cap){
    FILE*f=fopen(fn,"r"); if(!f){perror(fn);exit(1);}
    char*buf=malloc((size_t)cap*100); (void)buf;
    char**L=malloc(sizeof(char*)*cap);
    char line[256]; *n=0;
    while(fgets(line,sizeof line,f) && *n<cap){
        int L2=(int)strcspn(line,"\r\n"); line[L2]=0;
        if(L2<81) continue;
        L[*n]=strdup(line); (*n)++;
    }
    fclose(f);
    /* 打包成单一缓冲 */
    size_t tot=0; for(int i=0;i<*n;i++) tot+=strlen(L[i])+1;
    char*out=malloc(tot+1); out[0]=0; size_t o=0;
    for(int i=0;i<*n;i++){ strcpy(out+o,L[i]); o+=strlen(L[i]); out[o++]='\n'; }
    out[o]=0;
    for(int i=0;i<*n;i++) free(L[i]); free(L);
    return out;
}
static int split(char*blob,char**rows,int cap){
    int n=0; char*p=blob;
    while(*p && n<cap){ rows[n++]=p; p=strchr(p,'\n'); if(!p)break; *p++=0; }
    return n;
}

int main(int argc,char**argv){
    if(argc<2){ fprintf(stderr,"用法: %s <题库> [N] [skip_stale:0/1]\n",argv[0]); return 1; }
    int N = argc>2?atoi(argv[2]):300;
    int dostale = argc>3?atoi(argv[3]):1;
    int nl=0; char*blob=read_lines(argv[1],&nl,200000);
    char**rows=malloc(sizeof(char*)*nl); int nq=split(blob,rows,nl);
    if(N<=0||N>nq)N=nq;
    int full[5]={0,1,2,3,4};
    our_init();
    our_set_limit(1);
    our_set_locked(1);
    our_set_splevel(2);
    our_set_strategy(10,1,5,full,1);

    long TG_new=0,TG_stale=0; int solved=0;
    char sol[128];
    our_probe_reset();
    for(int q=0;q<N;q++){
        OurResult R=our_solve(rows[q]);
        if(!R.solved) continue;
        solved++;
        our_last_sol(sol);
        our_set_refsol(sol);
        our_set_probe(1);
        OurResult R2=our_solve(rows[q]);
        our_set_probe(0);
        TG_new+=R2.guesses;
        if(dostale){
            our_set_critstale(1);
            OurResult R3=our_solve(rows[q]);
            our_set_critstale(0);
            TG_stale+=R3.guesses;
            if(!R3.solved) fprintf(stderr,"[stale unsolved] q=%d\n",q);
        }
        if((q+1)%100==0) fprintf(stderr,"  ...%d/%d\n",q+1,N);
    }
    printf("\n[solved %d/%d]\n",solved,N);
    printf("端到端 guesses: crit_new(重算) = %.4f/题",(double)TG_new/(solved?solved:1));
    if(dostale) printf(" | crit_stale(传播前快照) = %.4f/题  比值 stale/new = %.4f",
        (double)TG_stale/(solved?solved:1),(double)TG_stale/(double)(TG_new?TG_new:1));
    printf("\n");
    our_probe_dump();
    return 0;
}
