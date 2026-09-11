"""fetch_data.py — 从上游获取并**逐字节重建**论文的 5 个数据集（本包不随附数据）

为什么有这个脚本
----------------
随包的论坛题库（约 5.9 MB / 75,831 实例）**上游没有声明任何许可证**，
再分发存在版权风险。因此本发布包**不含** `data/*.txt`，
改为提供本脚本：从 tdoku 官方 `data.zip` 下载后**本地重建**，并逐文件校验 MD5。

已核实的事实（2026-09-11 实测）
--------------------------------
* tdoku `data.zip` 的 git blob SHA1 = `2ae6e4f8d021d2198069814c7db18bf11fcd9591`
  （与 GitHub API 声明逐位一致，可用来证明下载件未被篡改）。
* 论文 5 个数据集的题面 **100% 包含**在 `data.zip` 内。
* 重建规则（已用 MD5 逐字节验证）：

  | 目标文件 | 上游来源 | 规则 |
  |---|---|---|
  | `forum_hardest_1905_11plus.txt` | `data/puzzles5_forum_hardest_1905_11+` | 原样 |
  | `sample5000.txt` | 同上 | 前 5000 行 |
  | `f20k.txt` | 同上 | 前 20000 行 |
  | `indep600.txt` | 同上 | 第 30001–30600 行（即 `lines[30000:30600]`） |
  | `top1465_clean.txt` | `data/puzzles3_magictour_top1465` | 去掉开头的 `#` 注释行，取 1465 行题面 |

用法
----
    python fetch_data.py                 # 下载 + 重建 + 校验，输出到 ./data
    python fetch_data.py --out DIR       # 指定输出目录
    python fetch_data.py --zip FILE      # 用本地已有的 data.zip（跳过下载）
    python fetch_data.py --check-only    # 只校验 ./data 里已有的文件

退出码 0 = 5 个文件全部逐字节匹配；非 0 = 有缺失或不匹配。
"""
import argparse
import hashlib
import io
import os
import re
import sys
import zipfile

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

UPSTREAM_ZIP = 'https://raw.githubusercontent.com/t-dillon/tdoku/master/data.zip'
UPSTREAM_ZIP_ALT = 'https://github.com/t-dillon/tdoku/raw/master/data.zip'
ZIP_BLOB_SHA1 = '2ae6e4f8d021d2198069814c7db18bf11fcd9591'

EXPECT_MD5 = {
    'forum_hardest_1905_11plus.txt': '700ce0bf1f6c5516ed772712006e107d',
    'f20k.txt':                       '7f6d9ce557606915988cb5ee9474a78f',
    'indep600.txt':                   'c60b01ba018eca011d82a181da280def',
    'sample5000.txt':                 'c13b13654affa0f79d68c4ea93bb9a04',
    'top1465_clean.txt':              '57645b96f47d9549b393e89e3e3aa8e6',
}

UP_11PLUS = 'data/puzzles5_forum_hardest_1905_11+'
UP_TOP1465 = 'data/puzzles3_magictour_top1465'


def md5_of_bytes(b):
    return hashlib.md5(b).hexdigest()


def git_blob_sha1(data):
    return hashlib.sha1(b'blob %d\0' % len(data) + data).hexdigest()


def download(dest):
    import urllib.request
    last = None
    for url in (UPSTREAM_ZIP, UPSTREAM_ZIP_ALT):
        try:
            print("  下载 %s" % url)
            req = urllib.request.Request(url, headers={'User-Agent': 'sudoku-fuze-artifact/1.1'})
            with urllib.request.urlopen(req, timeout=120) as r, open(dest, 'wb') as f:
                n = 0
                while True:
                    b = r.read(1 << 20)
                    if not b:
                        break
                    f.write(b)
                    n += len(b)
            print("  完成：%d 字节" % n)
            return True
        except Exception as exc:
            print("  失败：%s" % exc)
            last = exc
    print("  两条 URL 都失败（%s）。请手动下载 data.zip 后用 --zip 指定。" % last)
    return False


def lines_of(text):
    return [l for l in text.split('\n') if l.strip()]


def build(zp):
    """从 data.zip 重建 5 个数据集。所有失败都抛 DataError（不抛裸异常），便于给出可读提示。"""
    try:
        z = zipfile.ZipFile(zp)
    except zipfile.BadZipFile:
        raise DataError("不是有效的 zip 文件（下载可能被截断或损坏）：%s\n"
                        "    请删除它后重跑本脚本，或用 --zip 指向另一个 data.zip。" % zp)
    except OSError as exc:
        raise DataError("无法读取 %s：%s" % (zp, exc))

    names = set(z.namelist())
    for need in (UP_11PLUS, UP_TOP1465):
        if need not in names:
            raise DataError(
                "data.zip 里找不到 %s\n"
                "    这说明拿到的不是 tdoku 的 data.zip（其内部应为 data/puzzles* 若干文件）。\n"
                "    现有条目示例：%s" % (need, sorted(names)[:6]))

    raw11 = z.read(UP_11PLUS).decode('utf-8', errors='replace')
    L = lines_of(raw11)
    rawtop = z.read(UP_TOP1465).decode('utf-8', errors='replace')
    T = [l for l in lines_of(rawtop) if not l.lstrip().startswith('#')]
    if len(L) < 30600:
        raise DataError("%s 只有 %d 行，不足 30600 —— data.zip 版本可能不同。"
                        % (UP_11PLUS, len(L)))
    if len(T) < 1465:
        raise DataError("%s 只有 %d 行，不足 1465。" % (UP_TOP1465, len(T)))
    return {
        'forum_hardest_1905_11plus.txt': ('\n'.join(L) + '\n').encode(),
        'f20k.txt':                      ('\n'.join(L[:20000]) + '\n').encode(),
        'sample5000.txt':                ('\n'.join(L[:5000]) + '\n').encode(),
        'indep600.txt':                  ('\n'.join(L[30000:30600]) + '\n').encode(),
        'top1465_clean.txt':             ('\n'.join(T[:1465]) + '\n').encode(),
    }


class DataError(Exception):
    """可预期的失败（下载/文件/版本问题），以可读信息报错而不是抛栈。"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='data')
    ap.add_argument('--zip', dest='zip_path', default=None)
    ap.add_argument('--check-only', action='store_true')
    ap.add_argument('--purge-zip', action='store_true',
                    help='重建后删除 data.zip（默认保留，便于离线复用）')
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    fails = 0

    if a.check_only:
        print("=== 仅校验 %s 中已有文件 ===" % out)
        for fn, want in sorted(EXPECT_MD5.items()):
            p = os.path.join(out, fn)
            if not os.path.exists(p):
                print("  %-34s 缺失 ❌" % fn); fails += 1; continue
            got = md5_of_bytes(open(p, 'rb').read())
            ok = got == want
            if not ok:
                fails += 1
            print("  %-34s md5=%s %s" % (fn, got[:16] + '…', '✅' if ok else '❌ 期望 ' + want))
        print("\n%s" % ("全部通过 ✅" if not fails else "有 %d 项不通过 ❌" % fails))
        return 1 if fails else 0

    zip_path = a.zip_path
    if not zip_path:
        zip_path = os.path.join(out, '_tdoku_data.zip')
        if not os.path.exists(zip_path) or git_blob_sha1(open(zip_path, 'rb').read()) != ZIP_BLOB_SHA1:
            print("=== 下载 tdoku data.zip（约 73 MB）===")
            if not download(zip_path):
                return 2
    print("=== 校验 data.zip 身份（git blob SHA1）===")
    blob = git_blob_sha1(open(zip_path, 'rb').read())
    print("  实际 %s" % blob)
    print("  声明 %s  -> %s" % (ZIP_BLOB_SHA1, "一致 ✅" if blob == ZIP_BLOB_SHA1 else "**不一致**"))
    if blob != ZIP_BLOB_SHA1:
        print("  ⚠️ 下载件与上游不符，继续但请自行判断来源。")

    print("=== 重建并校验 ===")
    try:
        built = build(zip_path)
    except DataError as exc:
        print("  ✗ %s" % exc)
        print("\n未能重建数据集；**没有写入任何文件**。退出码 1。")
        return 1
    for fn, want in sorted(EXPECT_MD5.items()):
        data = built[fn]
        got = md5_of_bytes(data)
        ok = got == want
        if not ok:
            fails += 1
        with open(os.path.join(out, fn), 'wb') as f:
            f.write(data)
        print("  %-34s %8d 字节  md5=%s %s"
              % (fn, len(data), got[:16] + '…', '✅' if ok else '❌ 期望 ' + want))
    # 保留 data.zip：它是离线重建/复核的凭据，删掉会迫使使用者每次重下 73 MB。
    # 只有显式 --purge-zip 才删。
    if a.purge_zip:
        try:
            os.remove(zip_path)
            print("\n（已按 --purge-zip 删除 %s）" % zip_path)
        except OSError as exc:
            print("\n（--purge-zip 删除失败：%s）" % exc)
    else:
        print("\n已保留 data.zip：%s" % zip_path)
        print("  → 下次运行会直接复用，不再下载 73 MB。要删除请加 --purge-zip。")
    print("\n%s（输出目录 %s）" % ("5 个数据集全部逐字节匹配 ✅" if not fails else "%d 项不匹配 ❌" % fails, out))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
