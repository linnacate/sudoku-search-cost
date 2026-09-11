# 分发方式（PACKAGING）

> 一个包不能同时满足"审稿人开箱即跑"和"公开仓库干净合规"。本文件说明三层怎么切。

---

## 1. 三层结构

| 层 | 放什么 | 不放什么 | 谁看 |
|---|---|---|---|
| **GitHub 仓库** | 源码（`core/ thm/ exp/ open/ tools/ compat/`）、文档、`out/` 核查脚本、`results/README.md` | `data/*.txt`（许可未明，改为 `out/fetch_data.py` 本地重建）、`results/*.tsv`、`bin/`、`build_win/`、`third_party/` | 公众、审稿人（看代码） |
| **GitHub Release 附件** | 本发布 zip（含 `results/`、`logs/`、`out/`）、`MANIFEST.sha256`；**不含 `data/`** | 题库数据 | 审稿人（重建数据后复现） |
| **Zenodo** | 与 Release 同一份 zip，拿 DOI | — | 长期引用 |

> ⚠️ **尚未生成**：arXiv 编号未申请；Zenodo DOI 未注册。文档里出现的
> `10.5281/zenodo.XXXXXXX` 一律是**占位符**，拿到真号后需替换论文的
> Data availability 段、`CITATION.cff`，并重出 PDF。

`.gitignore` 已按此配置。**注意**：`bin/` 里的 40 个文件是 Ubuntu GCC 11.4.0 编出的
**Linux ELF**（0 个 PE），在 Windows 上无法运行；`.gitignore` 已排除，但**发布 zip 里保留**，
以便 Linux 用户不装编译器也能直接跑。

---

## 2. 发布 zip 的构成（本包）

```
sudoku_fuze_artifacts/
├─ README.md / README_EN.md      # 中文 + 英文说明
├─ CHANGELOG.md                  # ⭐ 发布前测试与修复记录（先看这个）
├─ REPRODUCE.md                  # 逐数字复现清单
├─ TEST_REPORT.md                # ⭐ 发布前测试报告（编译矩阵 + 冒烟矩阵）
├─ DATA_NOTICE.md                # ⭐ 数据来源与再分发风险
├─ THIRD_PARTY.md / NOTICE / LICENSE
├─ MANIFEST.sha256               # 全包 SHA256
├─ PACKAGING.md                  # 本文件
├─ build.sh / build_windows.ps1 / build_3rd.sh / fetch_third_party.sh
├─ compat/                       # MSVC 兼容垫片（新）
├─ core/ thm/ exp/ open/ tools/  # 源码
├─ data/                         # ⚠️ **不随包**；用 out/fetch_data.py 从上游重建（见 DATA_NOTICE.md）
├─ results/                      # 逐题结果 tsv
├─ bin/                          # 预编译 Linux ELF（方便直接复现）
└─ out/                          # 可复跑的核查脚本（新）
```

---

## 3. 最少必要操作（给使用者）

```bash
# 0) 先取得数据（本包不含题库）
python out/fetch_data.py                           # 下载上游 data.zip 并在本地逐字节重建

# Linux / macOS：直接跑预编译二进制
./bin/exact_bench data/sample5000.txt 5000 30      # 期望 AM = 29.1144

# 自己编译（Linux）
bash build.sh

# 自己编译（Windows，需 Visual Studio Build Tools 的 C++ 工作负载）
powershell -NoProfile -ExecutionPolicy Bypass -File build_windows.ps1

# 复算 §6.2.4 的相关性（不依赖 scipy）
python3 out/recompute_crossarch.py results/

# 全工具冒烟测试（Windows；脚本在 out\ 下，会自动把上一级当仓库根）
powershell -NoProfile -ExecutionPolicy Bypass -File out\smoke_all.ps1
```

---

## 4. 数据：仓库与发布包都不含，由使用者本地重建

```bash
python out/fetch_data.py              # 下载 + 重建 + 逐文件 MD5 校验（5/5 通过才退出 0）
python out/fetch_data.py --check-only # 只校验已有文件
python out/fetch_data.py --zip FILE   # 离线：用本地已有的 data.zip
```

> 论文使用的题库**不随仓库、也不随 Release 附件分发**，是**有意为之**（上游许可状态未明）。
> 已实测可从上游**逐字节重建**全部 5 个数据集，故这不影响可复现性 ——
> 这是数据政策上最保守、也最容易过审的选择。详见 `DATA_NOTICE.md`。
