# 第三方组件许可状态表

> **审核要点**：本仓库**不包含任何第三方源码**。所有第三方组件需由使用者自行获取。
> 本文件列出每个组件在论文中的用途、许可状态、获取方式与署名要求。

---

## 一、总览

| 组件 | 论文章节 | 许可证 | 随包? | 商业使用 | 再分发 |
|---|---|---|---|---|---|
| **tdoku** | §5.4, §5.9, §6.2.1, §6.2.4 | BSD 2-Clause © 2019 Tom Dillon | ❌ 否 | ✅ 允许 | ✅ 允许（保留声明）|
| **fsss** | §5.4, §6.2.1, §6.2.4 | 非标准声明（见下）| ❌ 否 | ⚠️ **受限** | ⚠️ 见下 |
| **fsss2** | §6.2.1（基线升级）| 未明确声明 | ❌ 否 | ⚠️ 不明 | ⚠️ 不明 |
| **jczsolve** | §6.2.3 | **无（源码注明 "copyright is not specified"）** | ❌ **否** | ❌ **不明** | ❌ **禁止** |

---

## 二、逐项说明

### 2.1 tdoku

- **用途**：论文主要对比基线。§5.4（head-to-head）、§5.9（墙钟分解）、§6.2.1（分支粒度）、§6.2.4（跨架构相关性 ρ=0.123）
- **许可证**：BSD 2-Clause
- **版权**：Copyright (c) 2019, Tom Dillon
- **获取**：
  ```bash
  git clone https://github.com/t-dillon/tdoku
  ```
- **署名要求**：再分发源码须保留版权声明、条件列表与免责声明；二进制形式须在文档/随附材料中复现
- **⚠️ 编译注意事项（实测）**：
  1. 必须用 `-mavx2 -mbmi2`，**不要**用 `-march=native`——后者可能编出当前 CPU 不支持的
     `avx512vbmi`，运行时报 `Illegal instruction`
  2. 除 `solver_dpll_triad_simd.cc` 外，还需链接 `util.o` 与 `grid_lib.o`，
     否则出现 `undefined reference to Util::Permutation`

### 2.2 fsss（A Fast Simple Sudoku Solver）

- **用途**：论文**最主要**的同粒度基线。§6.2.1 中 vs fsss = 0.471（少 52.9%）是论文核心硬证据
- **许可证**：源码头部为一段**非标准**声明，原文如下：
  > "For commercial users the actions allowed to this file are limited to erasing.
  > No limitations to other users, but citing somewhere my name is welcome.
  > There are parts of code taken from bb_sudoku by Brian Turner. Thank you, Brian!"
- **版权**：Mladen Dobrichev；部分代码取自 Brian Turner 的 bb_sudoku
- **获取**：可经 tdoku 仓库的 `other/fsss/fsss.cc` 取得
- **⚠️ 风险提示**：该声明**不是**任何标准开源许可证。其中"商业用户仅限删除"一句含义不明，
  且 bb_sudoku 部分的授权未单独说明。**建议使用者在商业场景中避免依赖此组件，
  或自行联系原作者取得明确授权。**
- **署名**：原作者欢迎（但非强制）署名 Mladen Dobrichev

### 2.3 fsss2

- **用途**：§6.2.1 基线升级实验
- **许可证**：源码仅标注作者（Mladen Dobrichev, 2014），**未附任何许可声明**
- **⚠️ 风险提示**：无明确授权即意味着默认受版权保护，**不得假定为可自由使用**
- **注意**：论文 §6.2.1 已如实说明 fsss2 属 **D=Digits 分支**，与本文 cell 分支**跨粒度**，
  其比值仅作参考，不构成主证据

### 2.4 jczsolve（JCZSolve）— ⚠️ 最需要审核注意的一项

- **用途**：§6.2.3 补充基线
- **许可证**：**无**。源码头部明确写道：
  > "The copyright is not specified."
  且该声明在文件头**重复出现三次**（分别针对 zhouyundong_2012、champagne、JasonLion 三位贡献者）
- **来源**：`forum.enjoysudoku.com` 论坛帖子（3.77us solver 主题），经三位作者陆续改作
- **⚠️ 处置结论**：
  1. **不得随本仓库分发**——版权归属不明确，再分发存在侵权风险
  2. **不得在商业闭源产品中使用**
  3. 本仓库仅提供 `exp/jzrun.c`——**我们自己编写的 driver**，
     它调用使用者自行获取的 JCZSolve，不含任何第三方代码片段
  4. 论文中引用 jczsolve 的**实测数字**（如 77.99、13.24）属**事实性观察结果**，
     不受版权保护，可自由引用；但**源码本身不可再分发**
- **获取（使用者自行承担合规责任）**：
  源码历史版本可在 gzSudoku 仓库的 `./others/jczslover/JCZSolve.c` 找到
  （注意该目录名拼写为 `jczslover`，非 `jczsolve`）

---

## 三、数据集许可

> ⚠️ **本节曾只有"来源"而没有"许可结论"，属于口径不一致**：本文件对第三方**源码**
> （jczsolve）采用"版权未指定即禁止再分发"的最严尺度，对同级的第三方**数据**却按
> "社区公开题集"的最松尺度处理。**现已统一为最严尺度。**

| 数据集 | 题数 | 来源 | 说明 |
|---|---:|---|---|
| forum_hardest_1905_11plus | 48,766 | forum.enjoysudoku.com "hardest" 主题帖 | 社区公开题集 |
| sample5000 | 5,000 | 上者 SER≥11 抽样 | 与本机侧互验同源 |
| top1465 | 1,465 | 公开 "top1465" 难题集 | ✅ **唯一真正独立的种群** |
| indep600 | 600 | forum 内子集 | ⚠️ **同源，非独立留出集** |
| f20k | 20,000 | forum 前 20,000 条的**副本** | ⚠️ **同源副本** |

### 许可状态：未解决（且按最严尺度处理）

* 上游发布者**未附任何许可证、授权声明或再分发条件**。
* 因此**不得假定可自由再分发**——与本文件对 jczsolve 的标准一致。
* **不援引**"题面字符串属于事实数据"作为依据：`sample5000` 的 SER≥11 抽样、
  `top1465` 的整理都可能构成**数据库权/汇编权**，且结论因法域而异。
  在同行评审场景下，来源未经确认本身即为风险，与法律结论无关。
* **暴露面**：约 6.0 MB、75,831 实例、50,231 道不同题面；
  真正独立来源的只有 `top1465_clean.txt`（按体积 2.0%）。
* **分发处置**：公开仓库**不含** `data/*.txt`（见 `.gitignore`）；
  数据只随 Release 附件 / Zenodo 归档分发，并附完整来源与逐文件摘要。
  完整说明与三档缓解方案见 **`DATA_NOTICE.md`**。

> **论文口径红线**：除 top1465 外，其余数据集**全部同源**。
> 以 f20k / indep600 作"独立测试"所得的任何泛化结论均**无效**——只能是同分布 held-out。

**数据集哈希（供核验，2026-09-11 已逐位重算确认）**：
```
7f6d9ce557606915988cb5ee9474a78f  data/f20k.txt
700ce0bf1f6c5516ed772712006e107d  data/forum_hardest_1905_11plus.txt
c60b01ba018eca011d82a181da280def  data/indep600.txt
c13b13654affa0f79d68c4ea93bb9a04  data/sample5000.txt
57645b96f47d9549b393e89e3e3aa8e6  data/top1465_clean.txt
```

---

## 四、给审核者的快速判断

| 问题 | 回答 |
|---|---|
| 本仓库是否含第三方源码？ | **否**。45 个源文件全部为原创 |
| 是否含"版权未指定"的代码？ | **否**。jczsolve 源码未纳入；仅提供自研 driver |
| 非标准许可的组件是否被依赖为**主证据**？ | **否**。主证据 vs fsss 的数字是**实测观察结果**，可自由引用；fsss 源码本身不随包 |
| 若审核要求移除 fsss 相关数字？ | 可行。移除后主结论仍由 vs jczsolve（同为 cell 分支，§6.2.3）与
| | top1465 独立种群验证支撑，但**证据强度会下降**，建议保留并注明许可状态 |
