"""make_manifest.py — 为发布包生成 SHA256 清单（MANIFEST.sha256）+ 数据清单（DATA_NOTICE 用表）"""
import hashlib
import os
import sys

root = sys.argv[1]
out_manifest = sys.argv[2]

rows = []
for dirpath, dirnames, files in os.walk(root):
    # 只排除真正的构建中间产物与 VCS 元数据；
    # 注意 logs/build_win/ 是**证据日志**，必须进清单（早期版本误按名字过滤掉了它）
    dirnames[:] = [d for d in dirnames if d not in ('__pycache__', '.git')]
    for fn in sorted(files):
        p = os.path.join(dirpath, fn)
        rel = os.path.relpath(p, root).replace('\\', '/')
        if rel == 'MANIFEST.sha256':
            continue          # 清单不列自己
        h = hashlib.sha256()
        with open(p, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        rows.append((h.hexdigest(), os.path.getsize(p), rel))

rows.sort(key=lambda r: r[2])
with open(out_manifest, 'w', encoding='utf-8', newline='\n') as f:
    for d, n, rel in rows:
        f.write("%s  %s\n" % (d, rel))

print("files:", len(rows), " total bytes:", sum(r[1] for r in rows))
print()
print("=== data/ 清单（用于 DATA_NOTICE.md） ===")
for d, n, rel in rows:
    if rel.startswith('data/'):
        with open(os.path.join(root, rel.replace('/', os.sep)), 'rb') as f:
            lines = sum(1 for _ in f)
        print("| `%s` | %d | %s | %.2f MB |" % (rel, lines, d[:16] + '…', n / 1048576))
print()
print("=== results/ 清单 ===")
for d, n, rel in rows:
    if rel.startswith('results/'):
        print("| `%s` | %.0f KB | %s |" % (rel, n / 1024, d[:16] + '…'))
