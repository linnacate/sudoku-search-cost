#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三方跨架构矩阵分析 —— 补真实 tdoku 列后重算"""
import sys, csv

def rank(xs):
    n=len(xs)
    idx=sorted(range(n), key=lambda i: xs[i])
    r=[0.0]*n
    i=0
    while i<n:
        j=i
        while j+1<n and xs[idx[j+1]]==xs[idx[i]]: j+=1
        avg=(i+j)/2.0+1.0
        for k in range(i,j+1): r[idx[k]]=avg
        i=j+1
    return r

def pearson(a,b):
    n=len(a); ma=sum(a)/n; mb=sum(b)/n
    num=sum((a[i]-ma)*(b[i]-mb) for i in range(n))
    da=sum((a[i]-ma)**2 for i in range(n))**0.5
    db=sum((b[i]-mb)**2 for i in range(n))**0.5
    return num/(da*db) if da*db else 0.0

def spearman(a,b): return pearson(rank(a),rank(b))

def kendall(a,b):
    n=len(a); c=d=0
    for i in range(n):
        for j in range(i+1,n):
            s=(a[i]-a[j])*(b[i]-b[j])
            if s>0: c+=1
            elif s<0: d+=1
    return (c-d)/(c+d) if c+d else 0.0

def med(xs):
    s=sorted(xs); n=len(s)
    return s[n//2] if n%2 else (s[n//2-1]+s[n//2])/2.0

def main(path):
    rows=[]
    with open(path) as f:
        rd=csv.DictReader(f, delimiter='\t')
        for r in rd:
            rows.append((int(r['our']), int(r['tdoku']), int(r['fsss'])))
    N=len(rows)
    cols={'A_ours':[r[0] for r in rows],
          'tdoku_real':[r[1] for r in rows],
          'fsss':[r[2] for r in rows]}
    print(f"N = {N}\n")
    print("=== 逐列统计（单位：猜测数/题，limit=1，内部相对口径）===")
    print(f"{'架构':<14}{'mean':>10}{'median':>10}{'min':>8}{'max':>10}")
    print("-"*52)
    for k,v in cols.items():
        print(f"{k:<14}{sum(v)/N:>10.2f}{med(v):>10.1f}{min(v):>8}{max(v):>10}")

    print("\n=== 排序一致性（难度榜单是否同构）===")
    print(f"{'配对':<26}{'Spearman':>10}{'Kendall':>10}{'top1%重叠/50':>14}")
    print("-"*62)
    ks=list(cols)
    K=max(1,int(N*0.01))
    for i in range(len(ks)):
        for j in range(i+1,len(ks)):
            a,b=cols[ks[i]],cols[ks[j]]
            ta=set(sorted(range(N),key=lambda t:-a[t])[:K])
            tb=set(sorted(range(N),key=lambda t:-b[t])[:K])
            ov=len(ta&tb)
            exp=K*K/N
            print(f"{ks[i]+' vs '+ks[j]:<26}{spearman(a,b):>10.3f}{kendall(a,b):>10.3f}"
                  f"{str(ov)+'/'+str(K):>10}  exp={exp:.1f}")


if __name__=='__main__':
    main(sys.argv[1] if len(sys.argv)>1 else 'crossarch.tsv')
