# 发布前测试报告（TEST REPORT）

**测试日期**：2026-09-11
**测试环境**：Windows 11（10.0.26200），MSVC 19.51.36256 x64（Visual Studio Build Tools 18）
**测试者**：独立复核（非作者本人会话）
**判据来源**：`README.md`、`REPRODUCE.md`、`THIRD_PARTY.md`

> **本机限制（影响下列结论的范围）**：无 `gcc`、无 `make`、无 `git`；
> `bash.exe` 是 WSL 垫片且 WSL 被沙箱拒绝（`E_ACCESSDENIED`），故**无法执行 `build.sh` 本身**。
> 因此本次是用 **MSVC 重编同一批源码** 来验证；这与原包 `bin/` 的 Linux/gcc 构建是
> **两条独立通路**，恰好构成交叉验证：**机器无关量（guesses）逐位一致**。

---

## 一、`build.sh` 覆盖度审计（逐行证据）

| 项 | 实测 | 证据 |
|---|---:|---|
| `build()` 调用数 | 27 | `build.sh` L58–61、L64–81、L110–112、L121–122 |
| `build_solver()` 调用数 | 8 | `build.sh` L100–105、L107、L109 |
| **`build.sh` 实际编译程序数** | **35** | 27 + 8 |
| 手写 `gcc` 行（额外程序） | 2 | `critlag`、`critprobe`（L115–118） |
| **合计可产出二进制** | **37** | 35 + 2 —— 与 README "37/37" 吻合 |
| 需第三方源码（`build_3rd.sh`） | 6 | `bench` `crossarch` `crossarch2` `wallbench` `costbreak` `jzrun` |
| **源码中存在但 `build.sh` 完全不编译** | **1** | ⚠️ **`tools/exact_bench.c`** |
| `build.sh` 出现 `tools/` 字样次数 | **0** | 全文检索 |

### 🔴 结论 1（阻断级）：`tools/exact_bench.c` 未被任何脚本编译

`README.md`（L11、L18、L21、L136）把 `exact_bench` 当作环境自检与**"零容忍"判据**，
`REPRODUCE.md`（L14–15）把它作为第一件事。
但 `build.sh` 全文不含 `tools/`，`build_3rd.sh` 也不含。
随包的 `bin/exact_bench` 是**没有构建脚本可复现的** Linux ELF 二进制。

**后果**：克隆仓库的审稿人执行 `bash build.sh` 后，`bin/exact_bench` **不存在**，
`README.md` 教的"三分钟上手第一条命令"必然失败。

**已修**：新增 `build_windows.ps1` 显式编译它（并完成源码级实测，见 §三）。

### 其他发现

* `core/consolidate_sp.c`、`core/sp_impl.inc`、`core/sp_impl2.inc` 是**头文件式引用**
  （被 `#include` 进 `sp2/sp3/abl2/abl3/keycombo/verify_sp/...`），不是独立程序 —— 属正常设计。
* `README.md` 与 `build.sh` 末尾都提示了这 6 个需第三方的程序 —— 一致，无问题。

---

## 二、Windows/MSVC 编译矩阵

用新增的 `build_windows.ps1`（`-Clean`），共尝试 **38** 个不依赖第三方的程序：

| 结果 | 数量 | 说明 |
|---|---:|---|
| ✅ 编译成功 | **36** | 含 `exact_bench`（`build.sh` 遗漏的那个） |
| ❌ 失败 | 2 | `canon_cv`、`keyscan` —— 均因 **C99 变长数组（VLA）**，MSVC 不支持（`error C2057`） |
| ⏸ 未尝试 | 6 | `bench` `crossarch` `crossarch2` `wallbench` `costbreak` `jzrun`（需第三方源码） |

### 编译过程中发现并修复的真实源码缺陷

| 文件 | 缺陷 | MSVC 表现 | GCC 表现 | 处置 |
|---|---|---|---|---|
| `exp/hcost.c` L34 | `COLOF[81]` 初始化列表有 **117** 个值 | `error C2078: 初始值设定项太多` | 静默截断，正常编译 | ✅ 截断到 81 |
| `exp/hcost2.c` L18 | 同上，117 个值 | 同上 | 同上 | ✅ 截断到 81 |
| `exp/keyscan.c` L25 | `COLOF[81]` 初始化列表有 **108** 个值 | 同上 | 同上 | ✅ 截断到 81 |

> 语义上无害（多余项本就不参与程序行为），但**会让 MSVC 用户直接编译失败**，
> 且暴露代码卫生问题。修复脚本 `out/fix_array_literals.py` 可复跑核对；
> 原始文件备份于 `logs/c2078_backup/`。

### 另外 15 个程序的链接回退

`abl2 abl3 keycombo verify_sp hcost_locked critmech bandbranch bandcrit bandcrit2
keyprof segscan abcmp cstar_dump exact_bench verify_thm` 这些 driver **自己在源码里
`#include "consolidate.c"`**，再链 `cons_lib.obj` 会 `LNK2005` 重复定义。
`build_windows.ps1` 实现了与 `build.sh` 相同的**带库失败→回退不带库**逻辑，全部通过。

### MSVC 兼容垫片（新增 `compat/`）

| 文件 | 作用 |
|---|---|
| `compat/msvc_compat.h` | `clock_gettime(CLOCK_MONOTONIC)` → `QueryPerformanceCounter`；`__builtin_ctz` / `__builtin_popcount` → MSVC 等价实现（SWAR 位计数，不依赖 POPCNT 指令）；`gettimeofday` 兜底 |
| `compat/sys/time.h` | 让 `#include <sys/time.h>` 在 MSVC 下可用（配合 `-I compat`），并避开 Windows SDK 的 `timeval` 重定义 |
| `compat/verify_thm_msvc.c` | `thm/verify_thm.c` 的 MSVC 移植版：原版末尾 4 处使用 GCC 语句表达式 `({...})`，MSVC 报 `C2059`。**逐行等价**，仅把语句表达式改成具名变量 |

> **只影响墙钟**，不触碰任何搜索/决策逻辑 —— 这一点由 §三 的 guesses 逐位一致反过来证明。

---

## 三、✅ 零容忍判据与非零容忍判据实测

| # | 判据（来源） | 期望 | 实测 | 结论 |
|---|---|---|---:|---|
| 1 | `exact_bench data/sample5000.txt 5000 30`（README L18） | AM = 29.1144 | **29.1144** | ✅ 逐位 |
| 2 | 同上 GM | 20.9580 | **20.9580** | ✅ 逐位 |
| 3 | 同上 填格 AM | 188.68 | **188.68** | ✅ 逐位 |
| 4 | `exact_bench data/sample5000.txt 5000 -1`（README L21） | AM = 30.7382 | **30.7382** | ✅ 逐位 |
| 5 | 同上 GM | 21.6869 | **21.6869** | ✅ 逐位 |
| 6 | 同上 填格 AM | 191.18 | **191.18** | ✅ 逐位 |
| 7 | `verify_thm data/indep600.txt 100` 非对角块(crit>0,minF≠2)（REPRODUCE L38） | **0** | **0** | ✅ |
| 8 | 同上 非对角块(crit=0,minF=2)（REPRODUCE L39） | **0** | **0** | ✅ |
| 9 | `exact_bench data/top1465_clean.txt 1465 30`（REPRODUCE L80） | 3.6758 / GM 2.6388 | **3.6758 / 2.6388** | ✅ 逐位 |
| 10 | `exact_bench data/top1465_clean.txt 1465 -1`（REPRODUCE L81） | 3.7891 / GM 2.6705 | **3.7891 / 2.6705** | ✅ 逐位 |
| 11 | §6.2.4 ρ 三元组（REPRODUCE L111–113） | 0.123 / 0.095 / 0.080 | **0.1233 / 0.0952 / 0.0796** | ✅ 原始日志 `logs/recompute_crossarch.txt` |
| 12 | 同上 τ_b | 0.085 / 0.065 / 0.054 | **0.0846 / 0.0646 / 0.0541** | ✅ 同上日志 |
| 13 | 5 个数据集 MD5（THIRD_PARTY §3） | 见文档 | **全部逐位一致** | ✅ |

**判据 7、8 全机核对量**：沿正确路径扫描 28,700 个空格（N=100 题），
两项非对角块计数均为 **0**，两项对角块分别为 14,953 与 13,747。

> ⭐ **判据 1–6、9–10 是最强证据**：guesses 是**机器无关量**，
> 由 MSVC/Windows 独立重编后与制作方的 gcc/Linux 构建**逐位一致到小数点后 4 位**，
> 说明算法行为不含未定义的平台依赖。**墙钟不同是预期的**（本机 365.56 µs/题 vs 论文 429.98 µs/题），
> 论文已声明墙钟仅同构建自比有效。

### 🔴 结论 2（阻断级）：`results/crossarch.tsv` 与文档不符

| 文件 | 实际 N | `our` 列均值 | 按 `REPRODUCE.md` 指定命令复算得到的 ρ |
|---|---:|---:|---|
| `results/crossarch.tsv`（**文档说这里是 sample5000**） | **1465** ❌ | 3.7891 | 0.3243 / 0.2970 / 0.3599 ❌ |
| `results/out_s5k.tsv`（文档**没提**这个文件） | **5000** ✅ | 30.7382 | **0.1233 / 0.0952 / 0.0796** ✅ |

* `crossarch.tsv` 的 `q` 列取值 0…1464，与 `sample5000` **零交集**；
  `our` 均值 3.7891 恰等于 `REPRODUCE.md` 里 **top1465 静态基线**的值。
* 即两个结果文件的**文件名与内容错配**。论文的 0.123/0.095/0.080 **本身是正确的**，
  只是躺在文档没提到的那个文件里 —— 审稿人按文档跑**必然复现失败**。
* **已修**：交换文件名（`crossarch.tsv` ← N=5000；原 1465 数据改名
  `top1465_crossarch.tsv`），并同步改正 `REPRODUCE.md` §三，
  新增不依赖 scipy 的复算脚本 `out/recompute_crossarch.py`。

### 🟡 结论 3：论文主打值缺逐题产物

`results/crossarch.tsv` 的 `our` 列是**静态基线**（`ALT_REM=-1`，30.7382）那一路的 guesses。
论文主打的分层反转值 **29.1144**（`ALT_REM=30`）**在包内没有逐题产物**，只有汇总值。
审稿人只能现跑 `exact_bench`（已实测一致，sample5000 约 15 分钟/路）。
建议后续补 `results/out_s5k_alt30.tsv`。

---

## 四、全工具冒烟矩阵（36 个已编译程序）

完整逐工具日志见 `logs/smoke/`（每工具一个文件）与 `logs/smoke/index.txt`。

| 工具 | 结果 | 说明 |
|---|---|---|
| `verify_thm` | ✅ | 定理 1 / 引理 1 独立复核通过（见 §三 判据 7、8） |
| `canon_test` `collide` | ✅ | 正常运行出数 |
| `abl2` | ✅ | L2 下逐键消融正常 |
| `abl3` | ✅ | ⚠️ **用硬编码文件名**，须在 `data/` 下运行（`smoke_all.ps1` 已自动处理） |
| `keyscan` | ❌ | MSVC 无法编译（VLA）；Linux 二进制在 `bin/` |
| `keycombo` | ✅ | ⚠️ 同上，须在 `data/` 下运行，参数为「数据集数 配置数」 |
| `sp2` `sp3` `sweep_sp` `verify_sp` | ✅ | 传播谱系与可靠性交叉验证正常 |
| `hcost` `hcost2` `hcost_locked` | ✅ | 人类成本模型正常 |
| `critmech` `critwhy` | ✅ | `critwhy` 用法是 `critwhy <micro\|decide\|struct> <题库>` |
| `bandbranch` `bandcrit` `bandcrit2` | ✅ | band 分支对齐实验正常 |
| `syminv` `row1exp` | ✅ | 对称不变性 / 首行规范化正常 |
| `keyprof` `segscan` `abcmp` | ✅ | `abcmp` 用法 `abcmp <题库> <N> <子集>`（子集 0/1/2） |
| `cstar_dump` | ✅ | 正常导出 | 
| `cstar_learn` | ⚠️ | 需要 `cstar.bin` 输入（由 `cstar_dump` 先生成），非缺陷 |
| `critlag` `critprobe` | ✅ | 探针变体正常 |
| `exact_bench` | ✅ | 零容忍判据工具，工作正常（**`build.sh` 遗漏的那个**） |
| `consolidate` + 7 个变体 | ✅ | 主求解器与全部历史变体均正常 |
| `canon_cv` | ❌ | MSVC 无法编译（VLA）；Linux 二进制在 `bin/` |

**小计（2026-09-11 修正后重跑）：已编译的 36 个工具中 35 个冒烟通过、0 个非零退出、
1 个跳过（`keyscan` —— MSVC 下因 C99 变长数组编译失败，故无可执行文件可跑）。**
另外 `cstar_learn` **未纳入冒烟**（它需要先由 `cstar_dump` 生成 `cstar.bin` 输入）。
索引见 `logs/smoke/index.txt`。

> 更正记录：早先一版把 `critwhy` / `abcmp` / `keycombo` / `abl3` 用错了参数
> （前两个缺 mode/子集参数，后两个需在 `data/` 目录下运行），得到 OK=31、非零 5；
> 当时文档写成"34/36 通过"，与日志不符。现已修正调用方式并**重跑**，结果如上，
> 且脚本 `out/smoke_all.ps1` 会自动处理这两个工具的 CWD。

### 噪声模式（不影响结论，但值得记录）

部分工具通过 `cmd.exe` 启动时会打印 `cmd.exe : ...` 前缀 —— 这是因为程序把
进度信息写到 **stderr**（`fflush(stdout)` 之外的诊断输出），cmd 会加前缀标注来源。
**不是错误**，退出码为 0。

---

## 五、本环境无法验证的项（诚实声明）

1. **`build.sh` 本身**：无 `bash`/`gcc`（WSL 被沙箱拒绝），未能执行。
   本次用 MSVC 重编同源码替代，**不能等同**于验证了 `build.sh` 在 Linux 上的行为。
2. **需第三方源码的 6 个程序与其数字**：§6.2.1（vs tdoku / fsss / fsss2）、
   §6.2.3（vs jczsolve）的 0.471 / 52.9% / 0.865 / 0.294 / 0.390 / 68.45 等 —— 未实测。
3. **随包 Linux ELF 的实际运行**：40 个文件均为 Ubuntu GCC 11.4.0 编出的 Linux ELF
   （0 个 PE），本机无法运行；其行为未验证。
4. **`results/` 之外的论文表格**：只复核了 §6.2.4 的 ρ/τ 与零容忍判据涉及的数字。
5. **CPU 微架构型号**：未记录。
6. **数据集与 tdoku 官方 `data.zip` 的哈希同一性**：未核验（论文 A.3 已记为待办）。
