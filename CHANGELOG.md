# CHANGELOG

本文件记录本工具包**发布前测试与修复**的全部改动。
原始交付包（`toolkit_original.zip`，sha256 见 `MANIFEST.sha256`）与本次发布包的差异，逐条列在下面。

---

## v1.1 — 2026-09-11（发布版；含发布前测试修复）

### 🔴 修复：两个结果文件被写反（影响论文 §6.2.4 的可复现性）

逐文件说明改名方向（**按"内容 → 旧文件名 → 新文件名"读**）：

| 内容（行数 / `our` 均值） | 修复前的文件名 | 修复后的文件名 |
|---|---|---|
| N=**5000** / 30.7382 | `results/out_s5k.tsv` | **`results/crossarch.tsv`** ← 改名 |
| N=**1465** / 3.7891 | `results/crossarch.tsv` | **`results/top1465_crossarch.tsv`** ← 改名 |

即：**两份文件的名字互换**，并把 1465 行那份补上能说明其身份的后缀
（原名 `out_s5k.tsv` 里的 "s5k" 其实指向 5000 行的那份，本身也是误导）。

* **症状**：`REPRODUCE.md` §三 指定用 `results/crossarch.tsv` 复算 §6.2.4 的 ρ 三元组，
  但该文件实际装的是 top1465（N=1465）的数据。按文档跑，复算得到 0.324 / 0.297 / 0.360，
  **与论文的 0.123 / 0.095 / 0.080 完全不同** —— 审稿人必然复现失败。
* **根因**：打包时文件名与内容错配。论文数字本身没问题：
  用 N=5000 那份复算得 **0.1233 / 0.0952 / 0.0796**，与论文一致到 ±0.0004。
* **判别办法**（写给后来者）：看 `q` 列最大值与 `our` 列均值 ——
  5000 题那份 `our` 均值 ≈ 30.74、1465 题那份 ≈ 3.7891；两文件的 `q` 都是各自数据集内的
  行下标，**不可跨文件对齐**。
* **佐证**：`out/recompute_crossarch.py`（新增，纯 numpy + 纯 Python，不依赖 scipy）
  可一键复算两个文件；`REPRODUCE.md` §三 已同步改正，并补了"两文件列名相同但 `q` 下标
  不可跨文件对齐"的判据说明。

### 🔴 修复：`COLOF[81]` 数组初始化超长（MSVC 下编译失败）

* `exp/hcost.c`、`exp/hcost2.c`：`COLOF[81]` 的初始化列表有 **117** 个值。
* `exp/keyscan.c`：`COLOF[81]` 的初始化列表有 **108** 个值。
* **为什么之前没暴露**：C 标准允许初始化列表长于数组长度（多余项被静默丢弃），
  GCC 接受；MSVC 报 `error C2078: 初始值设定项太多`。
* **影响**：语义上无害（多余项本来不参与程序行为），但会让 MSVC 用户直接编译失败，
  也让审稿人怀疑代码质量。
* **修复**：截断到声明长度 81（`out/fix_array_literals.py` 自动完成，可复跑核对）。
  原始文件备份在 `logs/c2078_backup/`。

### 🔴 修复：`build.sh` 遗漏"零容忍"判据工具

* `tools/exact_bench.c` **没有**被 `build.sh` 编译（脚本全文无 `tools/` 引用），
  但 `README.md` 把它当作环境自检与"零容忍"判据（29.1144 / 30.7382）。
  随包的 `bin/exact_bench` 是 Linux ELF，新克隆的用户**无法重建**它。
* **修复**：新增 `build_windows.ps1`（Windows/MSVC 全量构建，含 `exact_bench`），
  并在 `README.md` 注明 `build.sh` 需补上该行。

### 🟡 新增：Windows/MSVC 构建通路

* `build_windows.ps1` — 不依赖 bash/gcc/make，在 Windows 上编译全部原创工具。
* `compat/msvc_compat.h` — POSIX 兼容垫片：`clock_gettime` → QueryPerformanceCounter，
  `__builtin_ctz` / `__builtin_popcount` → MSVC 等价实现，`gettimeofday` 兜底。
* `compat/sys/time.h` — 让 `#include <sys/time.h>` 在 MSVC 下可用（配合 `-I compat`）。
* `compat/verify_thm_msvc.c` — `thm/verify_thm.c` 的 MSVC 移植版
  （原版末尾用了 GCC 的语句表达式扩展 `({...})`，MSVC 不支持）。
  逐行等价，只把 4 处语句表达式改成具名变量；权威版本仍是 `thm/verify_thm.c`。
* **实测**：38 个不依赖第三方的程序中 **36 个编译通过**；
  `thm/canon_cv.c` 与 `exp/keyscan.c` 因使用 C99 变长数组（VLA）在 MSVC 下仍失败，
  已在 `TEST_REPORT.md` 记录。

### 🔴 修复：数据集许可口径自相矛盾（审稿人最容易抓的一致性攻击点）

* `NOTICE` 原文写 **"Puzzle strings themselves are factual data."**（数据可自由使用的**最松**尺度），
  而同一份包里的 `THIRD_PARTY.md` 对同级的第三方风险（jczsolve，源码注明
  "The copyright is not specified."）用的是**最严**尺度："版权未指定即禁止再分发"。
  **两套相反的标准之间没有任何论证。**
* 实测暴露面：随包论坛题库约 **6.0 MB / 75,831 实例 / 50,231 道不同题面**，
  其中只有 `top1465_clean.txt`（按体积 2.0%）来自真正独立的来源。
* **修复**：统一为**最严尺度**。
  * `NOTICE` 的数据集段改写为"许可状态 **UNRESOLVED**"，并明确**不援引**"事实数据"论据
    （`sample5000` 的 SER≥11 抽样、`top1465` 的整理可能构成数据库权/汇编权，且因法域而异）。
  * `THIRD_PARTY.md` §三 同步改写，并加注"标准已统一"。
  * 新增 `DATA_NOTICE.md`：来源、逐文件 MD5、暴露面量化、以及三档缓解方案。
* 配套：**发布包与仓库都不含 `data/*.txt`**。改为 `out/fetch_data.py`：从 tdoku 官方 `data.zip`
  （git blob SHA1 校验）下载后**在本地逐字节重建** 5 个数据集并比对 MD5 —— 已实测 5/5 匹配。

### 🔴 修复：交付版 PDF 曾静默丢失摘要正文（380 字符）

* 英文终稿 PDF 第一版在加作者块时，把第 1 页 Abstract 及以下**整体下移 104 pt**，
  结果摘要从 "…proportional to the subtree size of its decision point" 之后
  **380 字符被推出页面**（最后一行落到 y=858 > 页高 792，直接被裁），
  且**全文都找不到**。旧自查没发现，因为它只比"总字符数"，而新增作者块让总数净增。
* **根因纠正**：一度归因于 pply_redactions()，**这是错的**。对照实验表明
  redaction 无害（p1 只少掉日期 10 字符，Abstract 完整）；真正的元凶是**位移出界**。
  另有第二个坑：文本块是「首行 Tm 绝对 + 后续行 Td 相对」，任何整体位移都会
  压坏块内行距。
* **修复**：作者块改为塞进标题与 Abstract 之间的**原有 43.19 pt 空隙**（3 行，合计 40.36 pt），
  **一行既有正文都不移动**；标题与 Abstract 全文逐字符不变。
  并用 insert_text 按基线定位（insert_textbox 对单行要求 ~1.9×字号 的框高，会误判放不下）。
* **自查加固**：erify_pdf.py 由 49 项增至 **53 项**，新增
  ① 第 2..N 页与原稿**逐字符**比对 ② 第 1 页 Abstract 起前缀完整性
  ③ 每页最后墨迹不出界；update_en_pdf.py 内置同一套"丢字即拒绝出稿"的硬校验。
  最终 53/53 PASS，且第 2..41 页与原稿逐字符相同（**页码不错位**）。

### 🟡 修复：文档与实际不符的五处（复核发现）

| 文档说法 | 实际 | 处置 |
|---|---|---|
| logs/c2078_backup/ 有备份 | 目录是**空的** | 已把 3 个原始文件放进去 |
| PACKAGING.md 的 smoke_all.ps1 调用形式 | 脚本在 out/，需 -Root | 脚本改为自动识别仓库根，文档同步 |
| DATA_NOTICE.md 提到 etch_data.sh | **该文件不存在** | 改为指向 .gitignore 与来源说明 |
| "51,640 道不同题面" | 实测 **50,231** | 4 处文档统一改正 |
| TEST_REPORT.md 说冒烟 "34/36 通过" | 旧日志 OK=31、非零 5（参数用法问题） | 用修正后的调用**重跑**：**OK=35、非零 0**、1 项跳过（cstar_learn 需 cstar.bin） |

* 另外 MANIFEST.sha256 第一版**不完整**（生成脚本按目录名过滤 uild_win，误删了
  证据日志 logs/build_win/ 共 60 个文件；清单 165 条 vs 磁盘 225 个文件）。
  已修正过滤规则并**全量**校验（0 缺失 / 0 多余 / 0 哈希不符），并附 out/verify_manifest.py。

### 🟢 新增：发布配套

* `MANIFEST.sha256` — 全包逐个文件的 SHA256 清单（**条数随包内容变化，以文件首行实际条数为准**；
   生成幂等，可用 `out/verify_manifest.py` 全量复核）。
* `out/verify_manifest.py` — 全量校验清单与磁盘文件是否一一对应且哈希一致。
* `out/fetch_data.py` — **本包不含题库数据**；该脚本从 tdoku 官方 `data.zip` 下载并在本地
  **逐字节重建** 5 个数据集（含 git blob SHA1 身份校验与 MD5 比对）。详见 `DATA_NOTICE.md`。
* `logs/recompute_crossarch.txt` — §6.2.4 的 ρ/τ 原始输出（此前文档里的 6 个数无日志支持）。
* `DATA_NOTICE.md` — 数据来源、许可状态与再分发风险的明确说明（原先只在 6 行里顺带提及）。
* `PACKAGING.md` — 仓库 / Release 附件 / Zenodo 三层分发怎么切。
* `TEST_REPORT.md` — 发布前测试报告（编译矩阵 + 冒烟矩阵 + 零容忍判据）。
* `logs/` — 测试原始证据（`logs/build_win/` 编译日志、`logs/smoke/` 冒烟日志、
  头条数字的原始输出），使文档里每个数字都可在包内追溯。
* `out/recompute_crossarch.py`、`out/check_array_literals.py`、
  `out/fix_array_literals.py`、`out/smoke_all.ps1`、`out/build_windows.ps1` — 可复跑的核查脚本。
* `.gitignore` — 扩充：排除 `data/*.txt`、`results/*.tsv`、`build_win/`、`*.exe`、`*.obj`。

### ✅ 发布前实测复核通过（本机 Windows / MSVC 19.51 x64）

| 判据 | 论文/文档值 | 本机实测 | 结论 |
|---|---:|---:|---|
| `exact_bench data/sample5000.txt 5000 30`（AM） | 29.1144 | **29.1144** | ✅ 逐位一致 |
| `exact_bench data/sample5000.txt 5000 -1`（AM） | 30.7382 | **30.7382** | ✅ 逐位一致 |
| 同上 GM | 20.9580 / 21.6869 | **20.9580 / 21.6869** | ✅ |
| `exact_bench data/top1465_clean.txt 1465 30` | 3.6758 | **3.6758** | ✅ |
| `exact_bench data/top1465_clean.txt 1465 -1` | 3.7891 | **3.7891** | ✅ |
| 同上 GM | 2.6388 / 2.6705 | **2.6388 / 2.6705** | ✅ |
| `verify_thm data/indep600.txt 100` 非对角块 | 0 / 0 | **0 / 0** | ✅ |
| §6.2.4 ρ（用 N=5000 数据） | 0.123 / 0.095 / 0.080 | **0.1233 / 0.0952 / 0.0796** | ✅ |
| 数据文件 MD5（5 个） | `THIRD_PARTY.md` §3 | **全部逐位一致** | ✅ |

> **重要口径**：上文 AM/GM 是"机器无关量"（guesses），在 MSVC/Windows 上与
> 制作方的 gcc/Linux 构建逐位一致。**墙钟数字不可跨机构比较**（本机 365.56 µs/题
> vs 论文 429.98 µs/题），论文已声明墙钟仅同构建自比有效。

---

## 未修复 / 已知缺口（诚实声明）

1. **论文主打值 29.1144 在包内无逐题产物**。`results/crossarch.tsv` 的 `our` 列是
   静态基线（`ALT_REM=-1`，30.7382）那一路的 guesses；分层反转（`ALT_REM=30`）那一路
   只保存了汇总值 29.1144，没有逐题文件。审稿人只能靠**现跑** `exact_bench` 复核
   （已实测一致，约 15 分钟）。建议后续补 `results/out_s5k_alt30.tsv`。
2. **两个工具在 MSVC 下仍编不过**：`canon_cv`、`keyscan`（C99 变长数组）。
   Linux/gcc 路径不受影响；这两个工具在原始包里的 Linux 二进制可正常对应。
3. **三个工具需要完整论坛题库或特定参数**：
   `abl3` 与 `keycombo` 用**硬编码文件名**从当前目录加载
   （须 `cd data && ../build_win/<tool>`）；`cstar_learn` 需要 `cstar.bin`
   （由 `cstar_dump` 先生成）。
4. **需第三方源码的数字未复核**：§6.2.1（vs tdoku/fsss/fsss2）、§6.2.3（vs jczsolve）
   全部依赖使用者自行获取的第三方求解器，本包不含，故未实测。
5. **数据集与 tdoku 官方 `data.zip` 的哈希同一性**未核验（论文 A.3 已记为待办）。
6. **CPU 微架构型号**未记录。
7. **`bin/` 里的 Linux ELF 是否应随包**：见 `PACKAGING.md`。它们是 Ubuntu GCC 11.4.0
   产物，在 Windows 上无法运行；`.gitignore` 已排除，但**发布 zip 里仍在**（便于审核者
   直接跑复现）。
