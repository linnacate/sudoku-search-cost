# results/ — 逐题结果说明

本目录保存论文中**可逐题复核**的结果文件。列名在各文件间一致，但**行下标不可跨文件对齐**。

| 文件 | 数据集 | N | `our` 列均值 | 用途 |
|---|---|---:|---:|---|
| `crossarch.tsv` | `sample5000` | **5000** | 30.7382 | ⭐ 论文 §6.2.4 的 ρ 三元组就来自这里 |
| `top1465_crossarch.tsv` | `top1465_clean` | 1465 | 3.7891 | top1465 独立种群的同一实验（旁证，非主证据） |

## 列定义

| 列 | 含义 |
|---|---|
| `q` | 该数据集内部的题号（0 起）。**不是全局题号，不可跨文件对齐** |
| `our` | 本文求解器 A 的 guesses |
| `tdoku` | tdoku 的 guesses（⚠️ **粒度不同**：tdoku 是 band 决策，一次定 3 格） |
| `fsss` | fsss 的 guesses |

> ⚠️ **粒度红线**：`tdoku` 与 `our`/`fsss` **不是同一分支粒度**，
> raw 比值不可直接解读（论文 §6.2.1 已用 BCC 归一化，归一化比为 0.294）。

## `our` 列属于哪一路配置

`our` 列是**静态基线**（`ALT_REM=-1`，AM=30.7382）的逐题 guesses。

论文主打的**分层反转**配置（`ALT_REM=30`，AM=29.1144）**没有保存逐题产物**，
只能现跑复核：

```bash
./bin/exact_bench data/sample5000.txt 5000 30    # 期望 AM = 29.1144
```

## 复算 §6.2.4

```bash
python3 out/recompute_crossarch.py results/
# 期望（crossarch.tsv, N=5000）:
#   A-tdoku     rho=+0.1233  tau_b=+0.0846  top1%(50)overlap=5
#   A-fsss      rho=+0.0952  tau_b=+0.0646  top1%(50)overlap=6
#   tdoku-fsss  rho=+0.0796  tau_b=+0.0541  top1%(50)overlap=3
```

> **历史提醒**：2026-09-11 之前，这两个文件的**文件名与内容曾经错配**
> （`crossarch.tsv` 装的是 N=1465 的数据），导致按文档复现会得到 0.324/0.297/0.360。
> 详见 `CHANGELOG.md`。现在已修正。
