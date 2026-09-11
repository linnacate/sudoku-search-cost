"""verify_manifest.py — 全量校验 MANIFEST.sha256 与磁盘文件是否一一对应且哈希一致"""
import hashlib
import os
import sys

root = sys.argv[1]
man = {}
for line in open(os.path.join(root, 'MANIFEST.sha256'), encoding='utf-8'):
    line = line.rstrip('\n')
    if not line.strip():
        continue
    h, rel = line.split('  ', 1)
    man[rel] = h

actual = []
for dp, dn, fs in os.walk(root):
    dn[:] = [d for d in dn if d != '__pycache__']
    for f in fs:
        rel = os.path.relpath(os.path.join(dp, f), root).replace('\\', '/')
        if rel != 'MANIFEST.sha256':
            actual.append(rel)

miss = [r for r in man if r not in actual]
extra = [r for r in actual if r not in man]
bad = []
for rel, h in man.items():
    p = os.path.join(root, rel.replace('/', os.sep))
    if not os.path.exists(p):
        continue
    d = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            d.update(c)
    if d.hexdigest() != h:
        bad.append((rel, h, d.hexdigest(), os.path.getsize(p)))

print("manifest entries : %d" % len(man))
print("files on disk    : %d" % len(actual))
print("missing on disk  : %d %s" % (len(miss), miss[:5]))
print("not in manifest  : %d %s" % (len(extra), extra[:5]))
print("hash mismatch    : %d" % len(bad))
for rel, want, got, size in bad:
    print("   %s" % rel)
    print("     manifest=%s" % want)
    print("     actual  =%s  size=%d" % (got, size))
print("VERDICT:", "FULL MATCH" if not (miss or extra or bad) else "PROBLEM")
sys.exit(1 if (miss or extra or bad) else 0)
