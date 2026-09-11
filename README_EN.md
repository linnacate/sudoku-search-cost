# Sudoku Fuze-Cell Strategy — Open Reproduction Artifact

Companion code and data for the paper:

> **Structure, Irreducibility, and Search Cost in Sudoku: Backdoors, Solver-Relative
> Difficulty, and Adaptive Variable Selection**
> Kangyu Lin — Independent Researcher, Xinxiang, Henan, China
> ORCID: [0009-0000-3125-3754](https://orcid.org/0009-0000-3125-3754)

This artifact lets a reviewer independently reproduce the **computable claims** of the paper.
It contains the solver sources, the benchmark instances, the per-instance result files, and
the scripts that regenerate every table.

* Repository: <https://github.com/linnacate/sudoku-search-cost>
* Archived release: `10.5281/zenodo.XXXXXXX` (placeholder — filled in at release time)
* Chinese documentation: [`README.md`](README.md)

> **Before anything else, read [`CHANGELOG.md`](CHANGELOG.md)** — it records everything found
> and fixed during pre-release testing on 2026-09-11, including a result-file mix-up that
> would have made §6.2.4 impossible to reproduce.

---

## 1. Quick start

### 1.1 Build

```bash
# Linux / macOS (gcc + g++)
bash build.sh
```

```powershell
# Windows (requires the C++ workload of Visual Studio Build Tools)
powershell -NoProfile -ExecutionPolicy Bypass -File build_windows.ps1
```

### 1.2 The zero-tolerance gate (~2 minutes)

```bash
./bin/exact_bench data/sample5000.txt 5000 30     # expect AM = 29.1144
./bin/exact_bench data/sample5000.txt 5000 -1     # expect AM = 30.7382
```

| Configuration | Paper value | This artifact | Status |
|---|---:|---:|---|
| Layered reversal (`ALT_REM=30`) | **29.1144** | **29.1144** | ✅ |
| Static baseline (`ALT_REM=-1`) | **30.7382** | **30.7382** | ✅ |
| Difference | −5.28% | **−5.28%** | ✅ |

**These two numbers are the gate.** `guesses` is a machine-independent quantity: it was
independently reproduced here to four decimal places under a *different compiler and
operating system* (MSVC 19.51 / Windows) than the authoring build (gcc / Linux). If your
values do not match, the compiler flags or the environment are wrong — **stop and fix that
before continuing**.

Wall-clock numbers, by contrast, are **not** comparable across machines or builds (this
build: ~365 µs/puzzle; the paper reports ~430 µs/puzzle). The paper declares wall clock
valid only for self-comparison within one build. Use branch granularity, decision counts,
and BCC for any architecture-level claim.

---

## 2. Tool → paper-section mapping

### §3 Theoretical results

| Paper | Claim | Tool | Run | Criterion |
|---|---|---|---|---|
| §3.2 Cor. to Lemma 1 | `crit(i) > 0 ⇔ minF(i) = 2` | `verify_thm` | `./bin/verify_thm data/indep600.txt 100` | both "off-diagonal block" counts **must be 0** |
| §3.3 Theorem 2 | canonical form is unique ⇒ CV exactly 0 | `canon_cv` | `./bin/canon_cv data/indep600.txt 100` | `diffCnt = 0` |
| §3.3 App. | cost of canonicalisation | `canon_test` | same dataset | canonicalised/original ≈ 0.954 (not 1.00) |
| §3.4 Prop. 3 | UA-set swap is irrelevant to solving | `collide` | `./bin/collide data/forum_hardest_1905_11plus.txt` | needs the **full** corpus; 0 pairs on a small sample is normal |

### §5 Experiments

| Paper | Content | Tool | Criterion |
|---|---|---|---|
| §5.2 Ablation | per-key ablation at L2 | `abl2` / `abl3` | removing K1 (crit) degrades markedly |
| §5.3 Key scan | systematic scan of 19 equivariant scoring keys | `keyscan` | see the `KT` table in the source header |
| §5.6 locked | machine −26% / human +30% | `hcost_locked` / `hcost` / `hcost2` | model `T = rounds·scan + guesses·try` |
| §5.7 Lineage | marginal gain L0→L4 propagation | `sp2` / `sp3` / `sweep_sp` | **L2 is the optimal stopping point** |
| §5.7 Reliability | dual-propagation cross-validation | `verify_sp` | differing/illegal solutions must be **all zero** |
| §5.8 Four-way adjudication | key combination × propagation config | `keycombo` | all 5 keys optimal under all 4 configs |

### §6 Core results

| Paper | Content | Tool | Note |
|---|---|---|---|
| **§6.2.1** | vs fsss = 0.471 (same granularity) | `h2h/crossarch2.cc` + `crossarch_report.py` | needs third-party sources |
| **§6.2.2** | band-branch alignment experiment | `bandbranch` / `bandcrit` / `bandcrit2` | coarse granularity hurts: BCC degrades 3.43× |
| **§6.2.3** | jczsolve supplementary baseline | `exp/jzrun.c` | ⚠️ fetch the source yourself; **do not redistribute** |
| **§6.2.4** | cross-architecture ρ | `out/recompute_crossarch.py` | ρ(tdoku, fsss) = **0.080**, the lowest ⭐ |
| **§6.3.1** | crit mechanism decomposition `G = P + N_top·c_top` | `critmech` / `critwhy` | 73% comes from the failure channel |
| **§6.3.2** | **layered reversal strategy** | `exact_bench` / `consolidate_alt` | ⭐ the paper's headline result |
| **§6.3.3** | new-heuristic first round (all 6 candidates fail) | `consolidate_hkey` / `keyprof` / `segscan` / `abcmp` | negative result |
| **§6.3.4** | break-even of guessing several cells at once | theory (§2.122 scaling law), `row1exp` | — |

### §2 Research process

| Topic | Tool |
|---|---|
| §2.111 crit staleness (frozen ⇒ +89%) | `consolidate_critstatic` |
| §2.112 paired test before/after update | `critprobe` |
| §2.114/2.115 depth localisation (crossover at depth 6) | `critlag` / `consolidate_dsw` |
| §2.118 new-heuristic first round | `consolidate_hkey` |
| §2.119 e-wdeg precondition probe (explanation set ≈ 9) | `consolidate_ewdeg` |
| §2.121 symmetry invariance (G changes but is not exploitable) | `syminv` |
| §2.122 first-row normalisation / scaling law | `row1exp` |

### §8 Open problems

| Content | Tool |
|---|---|
| Learnability of `c*` | `cstar_dump` / `cstar_learn` |

Full per-command detail: [`REPRODUCE.md`](REPRODUCE.md).

---

## 3. Datasets

> ⚠️ **This package ships no dataset files.** The upstream publishers attached no licence,
> so redistributing them would carry copyright risk. Instead, run
> `python out/fetch_data.py` to download the upstream tdoku `data.zip` and **rebuild all five
> datasets locally, byte for byte** (verified: 5/5 MD5 match). Full provenance, digests and
> the exact reconstruction rules are in [`DATA_NOTICE.md`](DATA_NOTICE.md).

| File (rebuilt into `data/` by `fetch_data.py`) | Instances | Note |
|---|---:|---|
| `forum_hardest_1905_11plus.txt` | 48,766 | main corpus, SER ≥ 11 |
| `sample5000.txt` | 5,000 | hardest sample with SER ≥ 11 (the paper's main dataset) |
| `top1465_clean.txt` | 1,465 | ✅ **the only genuinely independent population** |
| `indep600.txt` | 600 | ⚠️ same-source subset of the forum corpus |
| `f20k.txt` | 20,000 | ⚠️ a **copy** of the first 20,000 forum entries |

> ⚠️ **Red line on interpretation**: apart from `top1465_clean.txt`, **all datasets are
> same-source**. Any generalisation claim built on `f20k` / `indep600` as an "independent
> test" is invalid — it is same-distribution held-out data, **not** cross-population
> generalisation.

**Getting the data** (all five files verified byte-for-byte against their published MD5):

```bash
python out/fetch_data.py              # download + rebuild + MD5 check (exit 0 only if 5/5 match)
python out/fetch_data.py --check-only # verify files already present in data/
python out/fetch_data.py --zip FILE   # offline: use a data.zip you already have
```

The download is authenticated by its **git blob SHA1**
(`2ae6e4f8d021d2198069814c7db18bf11fcd9591`, matching the GitHub API), and the rebuild rules
are stated exactly in `DATA_NOTICE.md` §3. This also closes the open TODO in the paper's §A.3
about the hash identity of these datasets against tdoku's official `data.zip`:
the puzzle strings are **100 % contained** in it and can be reconstructed exactly.

MD5 digests for all five files are in [`DATA_NOTICE.md`](DATA_NOTICE.md) §1 and
[`THIRD_PARTY.md`](THIRD_PARTY.md) §3. Full SHA-256 listing of the shipped files:
[`MANIFEST.sha256`](MANIFEST.sha256) (which contains no `data/` entries, since no data ships).

**Data redistribution:** neither the repository nor the release archive ships the
forum-derived corpora. Read [`DATA_NOTICE.md`](DATA_NOTICE.md) before redistributing anything;
if your review machine has no network access, contact the author.

---

## 4. Third-party dependencies and licences

**This repository contains no third-party source code.** Obtain these yourself:

```bash
bash fetch_third_party.sh     # instructions and helper script
bash build_3rd.sh             # build the comparison drivers
```

| Component | Licence | Bundled? | Note |
|---|---|---|---|
| tdoku | BSD 2-Clause | ❌ | must use `-mavx2 -mbmi2`; **do not** use `-march=native` |
| fsss | non-standard declaration | ❌ | commercial use restricted — see `THIRD_PARTY.md` |
| fsss2 | none declared | ❌ | reference only |
| **jczsolve** | **none ("The copyright is not specified")** | ❌ | **must not be redistributed** |

### Scope of `MANIFEST.sha256` (**read this first if you are checking the git repo**)

`MANIFEST.sha256` covers the files in the **release zip** (the Release asset / Zenodo deposit).
The git repository deliberately omits some of them:

| In the manifest, not in the repo | Count | Why |
|---|---:|---|
| `bin/*` | 40 | prebuilt binaries are `.gitignore`d |
| `logs/build_win/*.log` | 60 | the `*.log` rule |
| `results/*.tsv` | 2 | per-instance results are `.gitignore`d |

**So running `python out/verify_manifest.py .` inside a git clone reports
`missing on disk: 102` and exits 1 — that is expected, not a broken package.**
For the full check, extract the release zip first:

```bash
python out/verify_manifest.py .
# expected: VERDICT: FULL MATCH (all 229 entries)
```

Read [`THIRD_PARTY.md`](THIRD_PARTY.md) for the full licence-status table, the exact
declaration texts, and the risk notes.

---

## 5. Reproducing the cross-architecture correlations (§6.2.4)

```bash
python3 out/recompute_crossarch.py results/
```

Expected output on `results/crossarch.tsv` (N = 5000), using no SciPy dependency:

| Solver pair | Spearman ρ | Kendall τ_b | Hardest-1% (50) overlap |
|---|---:|---:|---:|
| ours (A) — tdoku | **+0.1233** | +0.0846 | 5 |
| ours (A) — fsss | **+0.0952** | +0.0646 | 6 |
| **tdoku — fsss** | **+0.0796** | +0.0541 | 3 |

The paper reports these to three decimals as 0.123 / 0.095 / 0.080, τ as
0.085 / 0.065 / 0.054. The recomputed values agree to within ±0.0004.

> ⭐ The paper's core point here: **two mature, independent third-party solvers correlate
> with each other (0.080) less than either does with ours** — there is no cross-solver
> consensus on difficulty ordering.

`results/crossarch.tsv` holds N = 5000; `results/top1465_crossarch.tsv` holds N = 1465.
The two files share column names but their `q` column is a per-dataset row index and
**must not be aligned across files**. See [`results/README.md`](results/README.md).

---

## 6. Pre-release testing

[`TEST_REPORT.md`](TEST_REPORT.md) records the full pre-release test pass
(2026-09-11, Windows / MSVC 19.51 x64). Headline facts:

* **36 of 38** non-third-party programs compile under MSVC. The two failures
  (`canon_cv`, `keyscan`) are C99 **variable-length arrays**, which MSVC does not support;
  the Linux/gcc path is unaffected.
* `build.sh` compiles **35** programs directly plus 2 by hand = 37 (matching its "37/37"
  claim) — but it does **not** compile `tools/exact_bench.c`, the very tool the README calls
  the zero-tolerance gate. `build_windows.ps1` builds it.
* 14 drivers `#include "consolidate.c"` directly, so linking `cons_lib.obj` as well causes
  `LNK2005` duplicate symbols; both build scripts implement the
  "try with the library, fall back without it" logic.
* Three `COLOF[81]` array initialisers were **over-long** (117, 117 and 108 values).
  GCC silently truncates; MSVC errors out. They were truncated to 81
  (`out/fix_array_literals.py`).
* Zero-tolerance results: both gate values reproduce to four decimals, and `verify_thm`
  reports **0** for both off-diagonal-block counts.

---

## 7. Known limitations

1. **Wall-clock figures are not comparable across machines.** The CPU microarchitecture is
   not recorded; the paper declares this an explicit boundary. Architecture-level claims
   use machine-independent quantities only (branch granularity, decision counts, BCC).
2. **The headline value 29.1144 has no per-instance result file.** `results/crossarch.tsv`
   stores the *static baseline* path; the layered-reversal path is only stored as an
   aggregate. Reproduce it by running `exact_bench` (about 15 minutes for 5,000 instances).
3. **fsss has two figures** (66.86 on sample5000, 68.45 on the full forum corpus). Always
   state the dataset when citing it.
4. **tdoku's `guesses` are band decisions** (three cells at once), crossing granularity with
   this paper's cell branching. The raw ratio 0.865 must not be read directly; the
   normalised BCC ratio is 0.294.
5. **`build.sh` does not cover the 6 programs that need third-party sources**
   (see the note at the end of the script).
6. **`abl3` and `keycombo` load hard-coded dataset filenames from the current directory** —
   run them from inside `data/`, e.g.
   `cd data && ../build_win/keycombo.exe 1 1`.
7. **`cstar_learn` requires `cstar.bin`**, which must first be produced by `cstar_dump`.
8. **Numbers requiring third-party sources were not verified in this pass** — §6.2.1
   (vs tdoku / fsss / fsss2) and §6.2.3 (vs jczsolve).

---

## 8. Repository layout

```
.
├── README.md / README_EN.md   # Chinese / English documentation
├── CHANGELOG.md               # ⭐ pre-release test findings and fixes
├── REPRODUCE.md               # per-number reproduction list
├── TEST_REPORT.md             # ⭐ pre-release test report
├── DATA_NOTICE.md             # ⭐ data provenance and redistribution risk
├── PACKAGING.md               # repo / release asset / Zenodo split
├── THIRD_PARTY.md             # licence-status table (reviewers: read this early)
├── MANIFEST.sha256            # SHA-256 of every shipped file
├── LICENSE / NOTICE           # MIT (original code only) + attribution
├── CITATION.cff               # citation metadata
├── build.sh                   # Linux/gcc build
├── build_windows.ps1          # Windows/MSVC build
├── build_3rd.sh               # third-party comparison drivers
├── fetch_third_party.sh       # how to obtain third-party sources
├── compat/                    # MSVC compatibility shims (new)
├── core/                      # solver body: consolidate.c (v11) + history
├── thm/                       # §3 theoretical verification
├── exp/                       # §5/§6 experiments + h2h/ third-party drivers
├── open/                      # §8 open problems
├── tools/                     # exact_bench and helpers
├── out/                       # re-runnable check scripts (new)
├── data/                      # 5 benchmark datasets (⚠️ see DATA_NOTICE.md)
├── results/                   # per-instance result files
└── bin/                       # prebuilt Linux x86-64 binaries
```

> `bin/` holds Linux ELF executables (Ubuntu, gcc 11.4.0). They will not run on Windows;
> the `.gitignore` excludes them from the repository, but they are kept in the release
> archive so that Linux users can reproduce without a compiler.

---

## 9. Citation

If you use this software or these data, please cite the paper.
Machine-readable metadata is in [`CITATION.cff`](CITATION.cff).

```bibtex
@software{lin_sudoku_search_cost_2026,
  author  = {Lin, Kangyu},
  title   = {Sudoku Fuze-Cell Strategy: Open Reproduction Artifact},
  year    = {2026},
  url     = {https://github.com/linnacate/sudoku-search-cost},
  note    = {Companion artifact for "Structure, Irreducibility, and Search Cost in Sudoku"}
}
```

---

## 10. Licence

MIT for the original code in this repository — see [`LICENSE`](LICENSE).
Attribution and third-party notices: [`NOTICE`](NOTICE) and [`THIRD_PARTY.md`](THIRD_PARTY.md).
Third-party components retain their own terms and are **not** covered by the MIT licence.
The benchmark datasets are **not** covered by the MIT licence — see
[`DATA_NOTICE.md`](DATA_NOTICE.md).
