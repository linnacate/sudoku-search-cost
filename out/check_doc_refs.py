"""check_doc_refs.py — 扫文档里反引号包住的文件名，报告引用了但仓库里不存在的

用法: python check_doc_refs.py <repo_root>
"""
import io
import os
import re
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

root = sys.argv[1]
existing = set()
for dp, dn, fs in os.walk(root):
    dn[:] = [d for d in dn if d != '__pycache__']
    for f in fs:
        existing.add(f)

DOCS = ['README.md', 'README_EN.md', 'CHANGELOG.md', 'TEST_REPORT.md', 'REPRODUCE.md',
        'DATA_NOTICE.md', 'PACKAGING.md', 'THIRD_PARTY.md', 'CITATION.cff', 'NOTICE',
        os.path.join('results', 'README.md')]

# 这些是"将来才会有"或"使用者需自行获取"的，允许不在此刻存在
ALLOW = {
    'fetch_third_party.sh', 'build.sh', 'build_3rd.sh', 'build_windows.ps1',
    'LICENSE', 'NOTICE', 'MANIFEST.sha256', 'smoke_all.ps1', 'verify_manifest.py',
    'recompute_crossarch.py', 'check_array_literals.py', 'fix_array_literals.py',
    'make_manifest.py', 'update_en_pdf.py', 'verify_pdf.py',
    'cstar.bin',                     # 由 cstar_dump 生成
    'data.zip',                      # tdoku 官方数据包
    'sudoku_fuze_artifacts_v1.1_20260911.zip',
    'sudoku_fuze_artifacts_v1.1_20260911.zip.sha256',
    'sudoku-search-cost',            # 仓库名，不是文件
    'CITATION.cff',
    # ---- 以下是"文档里确实会提到、但本仓库有意不提供"的，属正常 ----
    'fsss.cc', 'JCZSolve.c', 'solver_dpll_triad_simd.cc',   # 第三方源码，使用者自行获取
    'out_s5k.tsv',                   # 旧名：CHANGELOG/TEST_REPORT 在描述"文件名错配"这个 bug 时必须写它
    'out_s5k_alt30.tsv',             # 尚未提供：文档中明确标注为"建议后续补"的逐题产物
    # ---- 数据集：本包有意不分发，由 out/fetch_data.py 从上游重建（见 DATA_NOTICE.md）----
    'forum_hardest_1905_11plus.txt', 'f20k.txt', 'indep600.txt',
    'sample5000.txt', 'top1465_clean.txt', 'data.zip',
}

PAT = re.compile(r'`([A-Za-z0-9_./\\-]+\.(?:md|sh|ps1|py|cff|txt|tsv|json|ya?ml|c|cc|h|inc))`')

missing = {}
for d in DOCS:
    p = os.path.join(root, d)
    if not os.path.exists(p):
        print("  DOC MISSING: %s" % d)
        continue
    t = io.open(p, encoding='utf-8', errors='replace').read()
    for m in PAT.finditer(t):
        raw = m.group(1)
        name = raw.replace('\\', '/').split('/')[-1]
        if name in existing or name in ALLOW:
            continue
        missing.setdefault(name, set()).add(d.replace(os.sep, '/'))

for k, v in sorted(missing.items()):
    print("  NOT FOUND: %-34s referenced by %s" % (k, ', '.join(sorted(v))))
print("  ---- total %d unresolved references" % len(missing))
sys.exit(1 if missing else 0)
