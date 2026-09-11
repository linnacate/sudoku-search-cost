"""scan_data_leak.py — 证明发布包里**没有任何题库数据**（不只检查 data/ 目录名）

做法：把随包所有文本/二进制文件扫一遍，抽出所有"81 字符且仅含 1-9 与 ."的题面串，
与论文 5 个数据集的题面集合求交。若交集非空，说明数据以某种形式泄漏在包里。

用法: python scan_data_leak.py <repo_root> <datasets_dir>
"""
import io
import os
import re
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

root, dsdir = sys.argv[1], sys.argv[2]

# 收集"真数据"的题面
real = set()
counts = {}
for fn in sorted(os.listdir(dsdir)):
    if not fn.endswith('.txt'):
        continue
    s = set()
    with open(os.path.join(dsdir, fn), encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.strip()
            if len(line) >= 81:
                s.add(line[-81:])
    counts[fn] = len(s)
    real |= s
print("参照数据集（%d 个文件）:" % len(counts))
for k, v in counts.items():
    print("   %-34s %6d 题" % (k, v))
print("   去重合计: %d 题\n" % len(real))

PUZ = re.compile(rb'(?<![1-9.])[1-9.]{81}(?![1-9.])')
found = {}
scanned = 0
for dp, dn, fs in os.walk(root):
    dn[:] = [d for d in dn if d not in ('__pycache__', '.git')]
    for f in sorted(fs):
        p = os.path.join(dp, f)
        rel = os.path.relpath(p, root).replace('\\', '/')
        try:
            data = open(p, 'rb').read()
        except OSError:
            continue
        scanned += 1
        for m in PUZ.finditer(data):
            s = m.group(0).decode('ascii')
            if s in real:
                found.setdefault(rel, set()).add(s)

print("=" * 72)
print("扫描 %d 个文件，其中含题库题面的:" % scanned)
if not found:
    print("   *** 无 *** —— 发布包内不含任何数据集题面 ✅")
else:
    for k, v in sorted(found.items(), key=lambda kv: -len(kv[1])):
        print("   %-50s %d 题" % (k, len(v)))
    total = len(set().union(*found.values()))
    print("   泄漏的不同题面合计: %d" % total)
print("=" * 72)
sys.exit(1 if found else 0)
