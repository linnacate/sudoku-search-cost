"""recompute_crossarch.py — 独立复算 §6.2.4 跨架构相关性（不依赖 scipy）

用法: python recompute_crossarch.py <results_dir>
输出: 每个 tsv 的 N、AM，以及三对求解器的 Spearman rho / Kendall tau_b / 最难 50 题重叠
"""
import csv, os, sys
import numpy as np


def ranks(x):
    x = np.asarray(x, float); n = len(x)
    order = np.argsort(x, kind='mergesort'); r = np.empty(n, float)
    i = 0
    while i < n:
        j = i
        while j + 1 < n and x[order[j + 1]] == x[order[i]]:
            j += 1
        avg = (i + j) / 2.0 + 1
        for k in range(i, j + 1):
            r[order[k]] = avg
        i = j + 1
    return r


def pearson(a, b):
    a = np.asarray(a, float); b = np.asarray(b, float)
    a = a - a.mean(); b = b - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d else 0.0


def spearman(a, b):
    return pearson(ranks(a), ranks(b))


def kendall_tau_b(a, b):
    a = np.asarray(a, float); b = np.asarray(b, float); n = len(a)
    conc = disc = tie_a = tie_b = 0
    for i in range(n):
        da = a[i] - a[i + 1:]; db = b[i] - b[i + 1:]
        s = da * db
        conc += int((s > 0).sum()); disc += int((s < 0).sum())
        tie_a += int(((da == 0) & (db != 0)).sum())
        tie_b += int(((db == 0) & (da != 0)).sum())
    n0 = n * (n - 1) / 2.0
    den = np.sqrt((n0 - tie_a) * (n0 - tie_b))
    return float((conc - disc) / den) if den else 0.0


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else '.'
    files = [f for f in sorted(os.listdir(d)) if f.lower().endswith('.tsv')]
    for fn in files:
        p = os.path.join(d, fn)
        with open(p, encoding='utf-8', errors='replace') as f:
            rows = list(csv.DictReader(f, delimiter='\t'))
        if not rows or not {'our', 'tdoku', 'fsss'} <= set(rows[0].keys()):
            print("=" * 70); print(fn, ": 跳过（列不匹配）"); continue
        our = np.array([float(r['our']) for r in rows])
        td = np.array([float(r['tdoku']) for r in rows])
        fs = np.array([float(r['fsss']) for r in rows])
        print("=" * 70)
        print("%s  N=%d   AM(our)=%.4f  AM(tdoku)=%.2f  AM(fsss)=%.2f"
              % (fn, len(rows), our.mean(), td.mean(), fs.mean()))
        for name, (a, b) in {'A-tdoku': (our, td),
                             'A-fsss': (our, fs),
                             'tdoku-fsss': (td, fs)}.items():
            rho = spearman(a, b); taub = kendall_tau_b(a, b)
            K = max(1, int(round(len(rows) * 0.01)))
            o = np.argsort(-a, kind='mergesort')[:K]
            t = np.argsort(-b, kind='mergesort')[:K]
            ov = len(set(o.tolist()) & set(t.tolist()))
            print("  %-11s rho=%+.4f  tau_b=%+.4f  top1%%(%d)overlap=%d"
                  % (name, rho, taub, K, ov))


if __name__ == '__main__':
    main()
