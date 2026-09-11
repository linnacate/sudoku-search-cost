# 数独引信格策略 — 开源复现工具包

> 配套论文：**《数独求解中的引信格策略》** v8（2026-09-11）
> 用途：供审稿人 / 审核者独立复现论文中的**可计算结论**，并为开源发布做准备。
>
> ⚠️ **发布前请先读 [`CHANGELOG.md`](CHANGELOG.md)**：它记录了 2026-09-11 发布前测试
> 发现的全部问题与修复，其中包含一处**结果文件与文档错配**（会导致 §6.2.4 无法复现，
> 现已修正）。测试全过程见 [`TEST_REPORT.md`](TEST_REPORT.md)。
>
> 发布相关文档：[`TEST_REPORT.md`](TEST_REPORT.md)（测试报告）·
> [`DATA_NOTICE.md`](DATA_NOTICE.md)（数据来源与再分发风险）·
> [`PACKAGING.md`](PACKAGING.md)（仓库/附件/Zenodo 怎么切）·
> [`README_EN.md`](README_EN.md)（英文版）· [`MANIFEST.sha256`](MANIFEST.sha256)（全包哈希）
>
> ⚠️ **两个标识符尚未生成，切勿当作已注册**：
> arXiv 编号未申请；Zenodo DOI 未注册（论文与本文档中出现的一律是占位符
> `10.5281/zenodo.XXXXXXX`）。拿到后需同步替换论文 Data availability 段与 `CITATION.cff`。

---

## 一、三分钟上手

```bash
bash build.sh                       # Linux/gcc，37/37 通过（已实测）
```

```powershell
# Windows（需 Visual Studio Build Tools 的 C++ 工作负载）
powershell -NoProfile -ExecutionPolicy Bypass -File build_windows.ps1
```

> ⚠️ **`build.sh` 的一个已知缺口**：它编译 35 个程序（27 × `build()` + 8 × `build_solver()`）
> 加 2 个手写 `gcc` 行 = **37**，与"37/37"吻合，但**不含 `tools/exact_bench.c`**
> （脚本全文无 `tools/` 字样）—— 而这个工具正是下面"零容忍"判据所用的。
> `build_windows.ps1` 已补上；用 `build.sh` 的用户请自行加一行：
> `build exact_bench tools/exact_bench.c`

### ⚠️ 第一步：先取得数据（本包**不含**题库）

```bash
python out/fetch_data.py     # 下载 tdoku 官方 data.zip → 在本地逐字节重建 5 个数据集
```

约 73 MB 下载，重建后逐文件校验 MD5（**全部通过才退出 0**）。
**为什么不含数据**：上游未声明许可，再分发有版权风险 —— 详见
[`DATA_NOTICE.md`](DATA_NOTICE.md)。脚本已实测可从上游**逐字节重建**全部 5 个文件。

最快验证（约 30 秒）：

```bash
# 论文 §6.3.2 核心数字 —— 应精确复现 AM = 29.1144
./bin/exact_bench data/sample5000.txt 5000 30

# 同一数据集的静态基线 —— 应精确复现 AM = 30.7382
./bin/exact_bench data/sample5000.txt 5000 -1
```

**这两条是本包的"零容忍"判据**：

| 配置 | 论文值 | 本包实测值 |
|---|---:|---:|
| 分层反转策略（`ALT_REM=30`） | **29.1144** | **29.1144** ✅ |
| 静态基线（`ALT_REM=-1`） | **30.7382** | **30.7382** ✅ |
| 差值 | −5.28% | **−5.28%** ✅ |

若这两条对不上，说明编译选项或环境有误，**不要继续**。

---

## 二、工具 → 论文章节映射

### §3 理论结果

| 论文位置 | 主张 | 工具 | 运行 | 判据 |
|---|---|---|---|---|
| §3.2 引理 1 推论 | $\mathrm{crit}(i)>0 \iff \mathrm{minF}(i)=2$ | `verify_thm` | `./bin/verify_thm data/indep600.txt 100` | "非对角块"计数**必须 = 0** |
| §3.3 定理 2 | 规范形式唯一 ⟹ CV **精确 0** | `canon_cv` | `./bin/canon_cv data/indep600.txt 100` | `diffCnt = 0` |
| §3.3 附 | 规范化的性能代价 | `canon_test` | 同上数据集 | 规范化/原盘 ≈ 0.954（非 1.00）|
| §3.4 命题 3 | UA 集互换对求解冗余 | `collide` | `./bin/collide data/forum_hardest_1905_11plus.txt` | 需**全量**跑；小样本出 0 对属正常 |

### §5 实验

| 论文位置 | 内容 | 工具 | 判据 |
|---|---|---|---|
| §5.2 消融 | L2 下逐键消融 | `abl2` / `abl3` | 去掉 K1(crit) 显著变差 |
| §5.3 键扫描 | 19 个等变打分键系统扫描 | `keyscan` | 见源码头部 `KT` 表 |
| §5.6 locked | 机器 −26% / 人类 +30% | `hcost_locked` / `hcost` / `hcost2` | 模型 $T=\text{rounds}\cdot\text{scan}+\text{guesses}\cdot\text{try}$ |
| §5.7 谱系 | L0→L4 传播边际增益 | `sp2` / `sp3` / `sweep_sp` | **L2 是最优停止点** |
| §5.7 可靠性 | 双传播交叉验证 | `verify_sp` | 解不同/非法**必须全 0** |
| §5.8 四维裁决 | 键组合 × 传播配置 | `keycombo` | 全 5 键在 4 个配置下均最优 |

### §6 核心结果

| 论文位置 | 内容 | 工具 | 说明 |
|---|---|---|---|
| **§6.2.1** | vs fsss = 0.471（同粒度硬证据）| `h2h/crossarch2.cc` + `crossarch_report.py` | 需第三方源码 |
| **§6.2.2** | band 分支对齐实验 | `bandbranch` / `bandcrit` / `bandcrit2` | 结论：粗粒度有害，BCC 恶化 3.43× |
| **§6.2.3** | jczsolve 补充基线 | `exp/jzrun.c` | ⚠️ 需自取源码，**禁止再分发** |
| **§6.2.4** | 跨架构相关性 ρ | `crossarch_report.py` | ρ(tdoku,fsss)=**0.080** 最低 ⭐ |
| **§6.3.1** | crit 机制分解 $G=P+N_{top}c_{top}$ | `critmech` / `critwhy` | 73% 来自失败通道 |
| **§6.3.2** | **分层反转策略** | `exact_bench` / `consolidate_alt` | ⭐ 论文主打结果 |
| **§6.3.3** | 新启发式首轮（6 候选全灭）| `consolidate_hkey` / `keyprof` / `segscan` / `abcmp` | 负结果 |
| **§6.3.4** | 一次猜多格的盈亏线 | 理论（§2.122 标度律实证）| `row1exp` |

### §2 研究过程（研究总览）

| 主题 | 工具 |
|---|---|
| §2.111 crit 时效性（冻结 +89%）| `consolidate_critstatic` |
| §2.112 更新前后 crit 配对检验 | `critprobe` |
| §2.114/2.115 深度定位（交叉点 depth 6）| `critlag` / `consolidate_dsw` |
| §2.118 新启发式首轮 | `consolidate_hkey` |
| §2.119 e-wdeg 前置判据（解释集 ≈9）| `consolidate_ewdeg` |
| §2.121 对称不变性（G 会变但不可利用）| `syminv` |
| §2.122 首行规范化 / 标度律 | `row1exp` |

### §8 开放问题

| 内容 | 工具 |
|---|---|
| $c^*$ 可学习性 | `cstar_dump` / `cstar_learn` |

---

## 三、数据集

> ⚠️ **本包不含 `data/` 目录**。上游未声明许可，再分发有版权风险。
> 用 `python out/fetch_data.py` 从 tdoku 官方 `data.zip` 下载并在本地**逐字节重建**
> （实测 5/5 MD5 匹配），完整说明见 [`DATA_NOTICE.md`](DATA_NOTICE.md)。

| 文件（由 `fetch_data.py` 重建到 `data/`） | 题数 | 说明 |
|---|---:|---|
| `forum_hardest_1905_11plus.txt` | 48,766 | 主库，SER≥11 |
| `sample5000.txt` | 5,000 | SER≥11 最硬抽样（论文主数据集）|
| `top1465_clean.txt` | 1,465 | ✅ **唯一真正独立的种群** |
| `indep600.txt` | 600 | ⚠️ forum 内同源子集 |
| `f20k.txt` | 20,000 | ⚠️ forum 前 20,000 的**副本** |

> ⚠️ **口径红线**：除 top1465 外**全部同源**。以 f20k / indep600 作"独立测试"的结论
> 只算同分布 held-out，**不是跨种群泛化**。

**MD5 与逐字节重建规则见 [`DATA_NOTICE.md`](DATA_NOTICE.md) §1、§3；**
第三方许可状态见 `THIRD_PARTY.md` §3。

---

## 四、第三方依赖

**本包不含任何第三方源码。** 需使用者自行获取：

```bash
bash fetch_third_party.sh        # 获取说明与脚本
```

| 组件 | 许可证 | 随包 | 注意 |
|---|---|---|---|
| tdoku | BSD 2-Clause | ❌ | 必须 `-mavx2 -mbmi2`，**勿用** `-march=native` |
| fsss | 非标准声明 | ❌ | 商业用途受限，见 THIRD_PARTY.md |
| fsss2 | 未声明 | ❌ | 仅作参考 |
| **jczsolve** | **无（"copyright is not specified"）** | ❌ | **禁止再分发** |

完整许可状态、风险提示与署名要求见 **`THIRD_PARTY.md`**。

---

## 五、目录结构

```
.
├── LICENSE              # MIT（仅覆盖原创代码）
├── NOTICE               # 第三方归属声明
├── CITATION.cff         # 引用元数据
├── THIRD_PARTY.md       # ⭐ 许可状态表（审核者优先看）
├── README.md            # 本文件（中文）
├── README_EN.md         # 英文版
├── CHANGELOG.md         # ⭐ 发布前测试与修复记录
├── TEST_REPORT.md       # ⭐ 发布前测试报告（编译矩阵 + 冒烟矩阵）
├── DATA_NOTICE.md       # ⭐ 数据来源与再分发风险
├── PACKAGING.md         # 仓库 / Release 附件 / Zenodo 三层怎么切
├── MANIFEST.sha256      # 全包 SHA256 清单
├── REPRODUCE.md         # 逐数字复现清单
├── build.sh             # Linux/gcc 编译原创工具
├── build_windows.ps1    # Windows/MSVC 编译（含 build.sh 遗漏的 exact_bench）
├── build_3rd.sh         # 编译需第三方的对比 driver
├── fetch_third_party.sh # 获取第三方（不自动下载 jczsolve）
├── compat/              # MSVC 兼容垫片（新）
├── core/                # 求解器主体 consolidate.c (v11) + 历史版本
├── thm/                 # §3 理论验证
├── exp/                 # §5/§6 实验 + h2h/ 第三方对比 driver
├── open/                # §8 开放问题
├── tools/               # exact_bench 等辅助工具
├── out/                 # 可复跑的核查脚本（含 fetch_data.py 取数）
├── data/                # ⚠️ **不随包**；用 out/fetch_data.py 重建（见 DATA_NOTICE.md）
├── results/             # 实测原始输出（供核对，见 results/README.md）
└── bin/                 # 预编译 Linux x86-64 二进制
```

---

## 六、已知限制

1. **墙钟数字不可跨机比较**。CPU 微架构未记录，论文已将其声明为适用边界；
   架构级结论一律用机器无关量（分支粒度 / 决策次数 / BCC）。
2. **fsss 有两个数字**（sample5000 上 66.86、forum 全集上 68.45），引用时必须标数据集。
3. **tdoku 的 guesses 是 band 决策**（一次定 3 格），与本文 cell 分支**跨粒度**，
   raw 比值 0.865 不可直接解读；归一化 BCC 比为 0.294。
4. `build.sh` 未覆盖需第三方的 6 个程序（见脚本末尾提示）。
5. **论文主打值 29.1144 没有逐题产物**。`results/crossarch.tsv` 的 `our` 列是
   **静态基线**（`ALT_REM=-1`，30.7382）那一路的 guesses；分层反转那一路只存了汇总值。
   复核需现跑 `./bin/exact_bench data/sample5000.txt 5000 30`（约 15 分钟，已实测一致）。
6. **`abl3` 与 `keycombo` 用硬编码文件名**从**当前目录**加载题库，须先进入 `data/`：
   `cd data && ../build_win/keycombo.exe 1 1`（Linux 同理：`cd data && ../bin/keycombo 1 1`）。
7. **`cstar_learn` 需要 `cstar.bin`**，须先用 `cstar_dump` 生成。
8. **两个工具在 MSVC 下编译失败**（`canon_cv`、`keyscan`，因使用 C99 变长数组）；
   Linux/gcc 路径不受影响。详见 `TEST_REPORT.md`。
9. **需第三方源码的数字未复核**：§6.2.1（vs tdoku/fsss/fsss2）、§6.2.3（vs jczsolve）。
