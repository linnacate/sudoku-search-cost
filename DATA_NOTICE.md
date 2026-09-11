# 数据来源与再分发说明（DATA NOTICE）

> **本发布包不含任何题库数据。** 这是**有意为之**：上游未声明许可，再分发有版权风险。
> 数据由使用者从上游获取后在本地**逐字节重建**——已实测可行，见 §3。
>
> 本文件回应一个发布前必须回答的问题：随包的数据集能不能公开再分发？
> 结论：**不随包分发**；改为提供重建脚本 + 逐文件哈希校验。

---

## 1. 论文使用的 5 个数据集

| 文件 | 题数 | MD5（逐字节校验用） |
|---|---:|---|
| `forum_hardest_1905_11plus.txt` | 48,766 | `700ce0bf1f6c5516ed772712006e107d` |
| `f20k.txt` | 20,000 | `7f6d9ce557606915988cb5ee9474a78f` |
| `sample5000.txt` | 5,000 | `c13b13654affa0f79d68c4ea93bb9a04` |
| `indep600.txt` | 600 | `c60b01ba018eca011d82a181da280def` |
| `top1465_clean.txt` | 1,465 | `57645b96f47d9549b393e89e3e3aa8e6` |

**合计 75,831 行 / 50,231 道不同题面 / 约 5.9 MB**（原先随包分发，现改为本地重建）。

---

## 2. 来源与许可状态

| 文件 | 来源 |
|---|---|
| 前四个 | tdoku 官方 `data.zip` 中的 `data/puzzles5_forum_hardest_1905_11+`（源自 `forum.enjoysudoku.com` "hardest" 主题帖） |
| `top1465_clean.txt` | tdoku 官方 `data.zip` 中的 `data/puzzles3_magictour_top1465`（源自 magictour.free.fr） |

**许可状态：未解决。**
上游发布者**未附**任何许可证、授权声明或再分发条件。
本包对第三方**源码**（jczsolve，源码注明 "The copyright is not specified."）采用
"版权未指定即**禁止再分发**"的最严尺度；对同级的第三方**数据**沿用同一尺度，
故**不随包分发**。

不援引"题面字符串属于事实数据"作为依据：`sample5000` 的抽样、`top1465` 的整理都可能
构成数据库权/汇编权，且结论因法域而异。在同行评审场景下，来源未经确认本身就是风险。

> **论文口径红线**（与许可无关，但同样重要）：除 `top1465_clean.txt` 外，
> 其余数据集**全部同源**。以 `f20k` / `indep600` 作"独立测试"所得的任何泛化结论**无效**，
> 只能是同分布 held-out。

---

## 3. 怎么拿到数据（已实测：可逐字节重建）

```bash
python out/fetch_data.py              # 下载 tdoku data.zip → 重建 → 逐文件校验 MD5
python out/fetch_data.py --check-only # 只校验 data/ 里已有的文件
python out/fetch_data.py --zip FILE   # 用本地已有的 data.zip（离线）
```

脚本会：

1. 从 `https://raw.githubusercontent.com/t-dillon/tdoku/master/data.zip`
   （备用 `https://github.com/t-dillon/tdoku/raw/master/data.zip`）下载约 73 MB；
2. 用 **git blob SHA1** 校验下载件身份
   —— `2ae6e4f8d021d2198069814c7db18bf11fcd9591`，与 GitHub API 声明**逐位一致**；
3. 按下列规则重建 5 个文件，并**逐字节比对 MD5**（全部通过才退出 0）。

### 重建规则（2026-09-11 实测，5/5 逐字节匹配）

| 目标文件 | 上游来源 | 规则 |
|---|---|---|
| `forum_hardest_1905_11plus.txt` | `data/puzzles5_forum_hardest_1905_11+` | **原样** |
| `sample5000.txt` | 同上 | **前 5000 行** |
| `f20k.txt` | 同上 | **前 20000 行** |
| `indep600.txt` | 同上 | **第 30001–30600 行**（`lines[30000:30600]`） |
| `top1465_clean.txt` | `data/puzzles3_magictour_top1465` | **去掉开头的 `#` 注释行**，取 1465 行题面 |

> 这同时**补上了论文 §A.3 的待办**（"数据集与 tdoku 官方 `data.zip` 的哈希同一性未核验"）：
> 5 个数据集的题面 **100% 包含**在 `data.zip` 内，且可按上表**逐字节重建**。

---

## 4. 可核验性

```bash
# 五个数据集的 MD5（与本文件 §1 逐一比对）
# Windows:
certutil -hashfile data\sample5000.txt MD5
# Linux/macOS:
md5sum data/*.txt

# 一步到位（下载 + 重建 + 校验）
python out/fetch_data.py

# 全包逐文件 SHA256 清单（不含 data/ —— 因为包内没有数据）
# 见 MANIFEST.sha256
```

> **离线审稿场景**：若审稿机器无法联网，请联系作者按审稿用途单独提供数据；
> 公开渠道只提供本重建脚本，不提供数据集本身。

### 4.1 自检：包里确实没有数据

发布前已用 `out/scan_data_leak.py` 复核过：把随包**全部 229 个文件**逐字节扫描，
抽出所有"81 字符且仅含 1–9 与 `.`"的题面串，与 5 个数据集的 **50,231 道不同题面**求交 ——

```
扫描 229 个文件，其中含题库题面的:  *** 无 ***
```

> 注意：`results/*.tsv` **不构成泄漏** —— 它只有题号（`q`）与各求解器的猜测次数，
> 不含题面本身，无法据此重建题目。
