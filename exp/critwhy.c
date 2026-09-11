/* critwhy.c — critmrv 成功机制研究 (2026-09-03)
 *
 * 核心谜题:
 *   2.15: 级联 91% 由 hidden single 触发, 条件是 freed(u,d) 从 2 降到 1
 *   2.25: 但 hidden single 中只有 6.9~13.3% 的 (u,d) 原本 freedom==2
 *   -> crit>0 (在引信上) 的格, 为何能带来 21% 改善?
 *
 * 三个模式:
 *   模式1 (micro)  : 候选级微观 —— 按 crit 分组, 统计填入后传播的详细后果
 *   模式2 (decide) : 决策级对照 —— 搜索每步, MRV vs critmrv 选的格, 分别展开比较
 *   模式3 (struct) : 结构分析   —— crit>0 的格到底有什么特征
 *
 * 编译: gcc -O3 -march=native -o critwhy critwhy.c
 * 用法: ./critwhy micro  puzzles.txt
 *       ./critwhy decide puzzles.txt
 *       ./critwhy struct puzzles.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#define ALL 0x1FFu
#define MAXT 2000000

static const int ROWOF[81] = {
 0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2,2,2,
 3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,4, 5,5,5,5,5,5,5,5,5,
 6,6,6,6,6,6,6,6,6, 7,7,7,7,7,7,7,7,7, 8,8,8,8,8,8,8,8,8};
static const int COLOF[81] = {
 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8,
 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8,
 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8};
static const int BOXOF[81] = {
 0,0,0,1,1,1,2,2,2, 0,0,0,1,1,1,2,2,2, 0,0,0,1,1,1,2,2,2,
 3,3,3,4,4,4,5,5,5, 3,3,3,4,4,4,5,5,5, 3,3,3,4,4,4,5,5,5,
 6,6,6,7,7,7,8,8,8, 6,6,6,7,7,7,8,8,8, 6,6,6,7,7,7,8,8,8};

static int UNITS[27][9];
static int PEERS[81][20];
static int NPEER[81];

static void init_tables(void) {
    for (int r = 0; r < 9; r++) for (int c = 0; c < 9; c++) UNITS[r][c] = r*9+c;
    for (int c = 0; c < 9; c++) for (int r = 0; r < 9; r++) UNITS[9+c][r] = r*9+c;
    for (int b = 0; b < 9; b++) {
        int k = 0;
        for (int i = 0; i < 81; i++) if (BOXOF[i] == b) UNITS[18+b][k++] = i;
    }
    for (int i = 0; i < 81; i++) {
        char seen[81] = {0};
        int n = 0;
        for (int u = 0; u < 27; u++) {
            int inU = 0;
            for (int k = 0; k < 9; k++) if (UNITS[u][k] == i) { inU = 1; break; }
            if (!inU) continue;
            for (int k = 0; k < 9; k++) {
                int j = UNITS[u][k];
                if (j != i && !seen[j]) { seen[j] = 1; PEERS[i][n++] = j; }
            }
        }
        NPEER[i] = n;
    }
}

static unsigned cand[81];
static unsigned rowm[9], colm[9], boxm[9];
typedef struct { short idx; unsigned oldcand; short oldr, oldc, oldb; char iscell; } Trail;
static Trail trail[MAXT];
static int tlen = 0;
static int g_grid[81];

static void reset_state(void) {
    tlen = 0;
    memset(rowm, 0, sizeof(rowm)); memset(colm, 0, sizeof(colm)); memset(boxm, 0, sizeof(boxm));
    for (int i = 0; i < 81; i++)
        if (g_grid[i]) {
            unsigned b = 1u << (g_grid[i]-1);
            cand[i] = 0;
            rowm[ROWOF[i]] |= b; colm[COLOF[i]] |= b; boxm[BOXOF[i]] |= b;
        }
    for (int i = 0; i < 81; i++)
        if (!g_grid[i]) cand[i] = ALL & ~(rowm[ROWOF[i]] | colm[COLOF[i]] | boxm[BOXOF[i]]);
}

static inline void undo_to(int mark) {
    while (tlen > mark) {
        Trail *t = &trail[--tlen];
        cand[t->idx] = t->oldcand;
        if (t->iscell) {
            rowm[ROWOF[t->idx]] = t->oldr;
            colm[COLOF[t->idx]] = t->oldc;
            boxm[BOXOF[t->idx]] = t->oldb;
        }
    }
}

static inline int assign(int i, int d) {
    unsigned bit = 1u << (d-1);
    Trail *t = &trail[tlen++];
    t->idx = i; t->oldcand = cand[i];
    t->oldr = rowm[ROWOF[i]]; t->oldc = colm[COLOF[i]]; t->oldb = boxm[BOXOF[i]];
    t->iscell = 1;
    cand[i] = 0;
    rowm[ROWOF[i]] |= bit; colm[COLOF[i]] |= bit; boxm[BOXOF[i]] |= bit;
    for (int k = 0; k < NPEER[i]; k++) {
        int j = PEERS[i][k];
        if (cand[j] & bit) {
            Trail *u = &trail[tlen++];
            u->idx = j; u->oldcand = cand[j]; u->iscell = 0;
            cand[j] &= ~bit;
            if (cand[j] == 0) return 0;
        }
    }
    return 1;
}

static short freed[27][9];
static int crit[81];
static int critdig[81][10];

static void compute_freed(void) {
    for (int uid = 0; uid < 27; uid++) {
        unsigned base = uid < 9 ? rowm[uid] : (uid < 18 ? colm[uid-9] : boxm[uid-18]);
        for (int d = 0; d < 9; d++) {
            if (base & (1u << d)) { freed[uid][d] = -1; continue; }
            unsigned bit = 1u << d;
            int c = 0;
            for (int k = 0; k < 9; k++) if (cand[UNITS[uid][k]] & bit) c++;
            freed[uid][d] = (short)c;
        }
    }
}

static void compute_crit(void) {
    for (int i = 0; i < 81; i++) { crit[i] = 0; for (int d = 0; d < 10; d++) critdig[i][d] = 0; }
    for (int uid = 0; uid < 27; uid++)
        for (int d = 0; d < 9; d++) {
            if (freed[uid][d] != 2) continue;
            unsigned bit = 1u << d;
            for (int k = 0; k < 9; k++) {
                int i = UNITS[uid][k];
                if (cand[i] & bit) { crit[i]++; critdig[i][d+1]++; }
            }
        }
}

/* ==== 详细传播: 记录 naked / hidden / 触发的 f2 -> f1 ==== */
typedef struct {
    int n_naked, n_hidden;
    int hs_from_f2;      /* hidden single 中, (u,d) 原本 freedom==2 */
    int f2_to_f1;        /* freedom 从 2 变 1 的次数 (由本候选的 3 个 unit 直接造成) */
    int f2_new;          /* 传播中新形成的 freed==2 项数 */
    int filled;
} PropDetail;

static short freed_before[27][9];

static int propagate_detail(PropDetail *pd) {
    int progress = 1;
    if (pd) { pd->n_naked = pd->n_hidden = pd->hs_from_f2 = pd->f2_to_f1 = pd->filled = 0; }
    int nf = 0, nna = 0, nhi = 0, nhf = 0;
    while (progress) {
        progress = 0;
        for (int i = 0; i < 81; i++) {
            unsigned c = cand[i];
            if (c && (c & (c-1)) == 0) {
                nna++;
                if (!assign(i, __builtin_ctz(c)+1)) {
                    if (pd) { pd->n_naked = nna; pd->n_hidden = nhi; pd->hs_from_f2 = nhf; pd->filled = nf; }
                    return -1;
                }
                nf++; progress = 1;
            }
        }
        for (int uid = 0; uid < 27; uid++) {
            unsigned once = 0, twice = 0, all = 0;
            for (int k = 0; k < 9; k++) {
                unsigned c = cand[UNITS[uid][k]];
                if (!c) continue;
                twice |= once & c; once |= c; all |= c;
            }
            unsigned placed = uid < 9 ? rowm[uid] : (uid < 18 ? colm[uid-9] : boxm[uid-18]);
            if ((all | placed) != ALL) {
                if (pd) { pd->n_naked = nna; pd->n_hidden = nhi; pd->hs_from_f2 = nhf; pd->filled = nf; }
                return -1;
            }
            unsigned hid = once & ~twice & ~placed;
            while (hid) {
                unsigned bit = hid & (~hid + 1);
                hid ^= bit;
                int tgt = -1, cnt = 0;
                for (int k = 0; k < 9; k++) {
                    int i = UNITS[uid][k];
                    if (cand[i] & bit) { tgt = i; cnt++; }
                }
                if (cnt == 1) {
                    int d = __builtin_ctz(bit);
                    nhi++;
                    /* 触发时, 该 (u,d) 的 freedom 应当已经是 1 (定义)
                       我们关心它【在本次传播开始前】是多少 */
                    if (freed_before[uid][d] == 2) nhf++;
                    if (!assign(tgt, d+1)) {
                        if (pd) { pd->n_naked = nna; pd->n_hidden = nhi; pd->hs_from_f2 = nhf; pd->filled = nf; }
                        return -1;
                    }
                    nf++; progress = 1;
                }
            }
        }
    }
    /* 把局部计数写回 pd (bug 修复: 此前只在 return -1 分支写, 正常返回时未写) */
    if (pd) {
        pd->filled = nf; pd->n_naked = nna; pd->n_hidden = nhi; pd->hs_from_f2 = nhf;
        compute_freed();
        pd->f2_new = 0;
        for (int u = 0; u < 27; u++) for (int d = 0; d < 9; d++)
            if (freed[u][d] == 2 && freed_before[u][d] != 2) pd->f2_new++;
        return nf;
    }
    return nf;
}

static uint64_t rs = 88172645463325252ULL;
static inline uint64_t xr(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static inline void seed(uint64_t s) { rs = s ? s : 88172645463325252ULL; }

static int pick_cell_mrv(void) {
    int best = -1, bc = 99;
    for (int i = 0; i < 81; i++) {
        if (!cand[i]) continue;
        int n = __builtin_popcount(cand[i]);
        if (n < bc) { bc = n; best = i; }
    }
    return best;
}

static int pick_cell_critmrv(void) {
    compute_freed(); compute_crit();
    int best = -1, bc = 99;
    for (int i = 0; i < 81; i++)
        if (cand[i] && crit[i] > 0) {
            int n = __builtin_popcount(cand[i]);
            if (n < bc) { bc = n; best = i; }
        }
    if (best >= 0) return best;
    return pick_cell_mrv();
}

static void order_vals(int i, int *ds, int nd) {
    if (nd <= 1) return;
    int w[9];
    for (int k = 0; k < nd; k++) {
        unsigned bit = 1u << (ds[k]-1);
        int cnt = 0;
        for (int p = 0; p < NPEER[i]; p++) if (cand[PEERS[i][p]] & bit) cnt++;
        w[k] = cnt;
    }
    for (int a = 1; a < nd; a++) {
        int kd = ds[a], kw = w[a], b = a-1;
        while (b >= 0 && w[b] < kw) { ds[b+1] = ds[b]; w[b+1] = w[b]; b--; }
        ds[b+1] = kd; w[b+1] = kw;
    }
}

static long guesses;
#define CAP 400000L
static int STRAT;   /* 0=MRV, 1=critmrv */
static long la_calls, la_pruned;

static int dfs(void) {
    if (propagate_detail(NULL) < 0) return 0;
    int any = 0;
    for (int i = 0; i < 81; i++) if (cand[i]) { any = 1; break; }
    if (!any) return 1;
    int i = STRAT ? pick_cell_critmrv() : pick_cell_mrv();
    if (i < 0) return 1;
    unsigned cc = cand[i];
    int ds[9], nd = 0;
    for (int d = 1; d <= 9; d++) if (cc & (1u << (d-1))) ds[nd++] = d;
    order_vals(i, ds, nd);
    for (int k = 0; k < nd; k++) {
        if (guesses >= CAP) return -1;
        int mark0 = tlen;
        la_calls++;
        int ok = assign(i, ds[k]);
        int f = ok ? propagate_detail(NULL) : -1;
        if (f < 0) { undo_to(mark0); la_pruned++; continue; }
        guesses++;
        int r = dfs();
        undo_to(mark0);
        if (r == 1) return 1;
        if (r == -1) return -1;
    }
    return 0;
}

/* ============ 模式1: 候选级微观 ============ */
/* 按 crit 值分组, 统计填入后的传播后果 */
#define MAXC 20
static long grp_n[MAXC], grp_contra[MAXC], grp_filled[MAXC];
static long grp_naked[MAXC], grp_hidden[MAXC], grp_hsf2[MAXC], grp_f2new[MAXC];
static long grp_hs_all = 0, grp_hsf2_all = 0;

static void mode_micro(const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) { fprintf(stderr, "打不开 %s\n", fn); return; }
    char line[512]; int idx = 0;
    for (int c = 0; c < MAXC; c++)
        grp_n[c]=grp_contra[c]=grp_filled[c]=grp_naked[c]=grp_hidden[c]=grp_hsf2[c]=grp_f2new[c]=0;
    grp_hs_all = grp_hsf2_all = 0;

    while (fgets(line, sizeof(line), f)) {
        int n = 0;
        for (char *p = line; *p && n < 81; p++) {
            if (*p >= '1' && *p <= '9') g_grid[n++] = *p - '0';
            else if (*p == '.' || *p == '0') g_grid[n++] = 0;
        }
        if (n != 81) continue;
        if (++idx > 1000) break;
        reset_state();
        propagate_detail(NULL);
        compute_freed(); compute_crit();

        for (int i = 0; i < 81; i++) {
            if (!cand[i]) continue;
            int c0 = crit[i]; if (c0 >= MAXC) c0 = MAXC-1;
            unsigned cc = cand[i];
            for (int d = 1; d <= 9; d++) {
                if (!(cc & (1u << (d-1)))) continue;
                /* 记录传播前的 freed */
                memcpy(freed_before, freed, sizeof(freed));
                int mark = tlen;
                int ok = assign(i, d);
                PropDetail pd;
                int res = ok ? propagate_detail(&pd) : -1;
                undo_to(mark);
                /* 恢复 freed (propagate_detail 内部改过) */
                memcpy(freed, freed_before, sizeof(freed));

                grp_n[c0]++;
                if (res < 0) { grp_contra[c0]++; continue; }
                grp_filled[c0] += pd.filled;
                grp_naked[c0] += pd.n_naked;
                grp_hidden[c0] += pd.n_hidden;
                grp_hsf2[c0] += pd.hs_from_f2;
                grp_f2new[c0] += pd.f2_new;
                grp_hs_all += pd.n_hidden;
                grp_hsf2_all += pd.hs_from_f2;
            }
        }
    }
    fclose(f);
    printf("模式1: 候选级微观 (N=%d 题, 初盘传播后)\n", idx);
    printf("=""""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""\n\n");
    printf("%-8s %8s %9s %9s %8s %8s %9s %9s\n",
           "crit值", "候选数", "矛盾率", "平均填格", "裸单", "隐单", "其中原f2", "新生成f2");
    for (int c = 0; c < MAXC; c++) {
        if (!grp_n[c]) continue;
        long nsucc = grp_n[c] - grp_contra[c];
        printf("%-8d %8ld %9.4f %9.3f %8.3f %8.3f %9.3f %9.3f\n",
               c, grp_n[c], (double)grp_contra[c]/grp_n[c],
               nsucc ? (double)grp_filled[c]/nsucc : 0,
               nsucc ? (double)grp_naked[c]/nsucc : 0,
               nsucc ? (double)grp_hidden[c]/nsucc : 0,
               nsucc ? (double)grp_hsf2[c]/nsucc : 0,
               nsucc ? (double)grp_f2new[c]/nsucc : 0);
    }
    printf("\n全样本: hidden single 中 (u,d) 原本 freedom==2 的比例 = %.4f  (%ld/%ld)\n",
           grp_hs_all ? (double)grp_hsf2_all/grp_hs_all : 0, grp_hsf2_all, grp_hs_all);
}

/* ============ 模式2: 决策级对照 ============ */
/* 在搜索每步, 比较 MRV 与 critmrv 的选择 */
static long step_same = 0, step_diff = 0;
static long when_diff_mrv_crit0 = 0;      /* 不同时, MRV 选的格 crit==0 的次数 */
static long when_diff_critmrv_ok = 0;     /* 不同时, critmrv 选的格 crit>0 */
static long degrade_steps = 0, total_steps = 0;   /* 过滤器退化的步数 */
static long critset_size_sum = 0;
static long ncrit_pos_sum = 0, nempty_sum = 0;
/* 不同时, 分别展开一步看结果 */
static long diff_mrv_contra = 0, diff_cm_contra = 0;
static long diff_mrv_filled = 0, diff_cm_filled = 0, diff_n = 0;
static long diff_mrv_hs = 0, diff_cm_hs = 0;

static void mode_decide(const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) { fprintf(stderr, "打不开 %s\n", fn); return; }
    char line[512]; int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        int n = 0;
        for (char *p = line; *p && n < 81; p++) {
            if (*p >= '1' && *p <= '9') g_grid[n++] = *p - '0';
            else if (*p == '.' || *p == '0') g_grid[n++] = 0;
        }
        if (n != 81) continue;
        if (++idx > 1000) break;
        reset_state();
        seed(20260903u);
        /* 沿 critmrv 的搜索路径走 */
        guesses = 0;
        int depth = 0;
        while (depth < 60) {
            if (propagate_detail(NULL) < 0) break;
            int any = 0;
            for (int i = 0; i < 81; i++) if (cand[i]) { any = 1; break; }
            if (!any) break;
            compute_freed(); compute_crit();

            int ncrit = 0, nemp = 0, cset = 0;
            for (int i = 0; i < 81; i++) if (cand[i]) { nemp++; if (crit[i] > 0) { ncrit++; cset++; } }
            total_steps++; ncrit_pos_sum += ncrit; nempty_sum += nemp;
            critset_size_sum += cset;
            if (ncrit == 0) degrade_steps++;

            int im = pick_cell_mrv();
            int ic = pick_cell_critmrv();
            if (im == ic) step_same++;
            else {
                step_diff++;
                if (crit[im] == 0) when_diff_mrv_crit0++;
                if (crit[ic] > 0) when_diff_critmrv_ok++;
                /* 分别展开一步 */
                unsigned cc = cand[im];
                int ds[9], nd = 0;
                for (int d = 1; d <= 9; d++) if (cc & (1u << (d-1))) ds[nd++] = d;
                order_vals(im, ds, nd);
                int m = tlen, ok1 = assign(im, ds[0]);
                PropDetail pd1; int r1 = ok1 ? propagate_detail(&pd1) : -1;
                undo_to(m);
                memcpy(freed, freed_before, sizeof(freed));
                compute_freed(); compute_crit();

                cc = cand[ic]; nd = 0;
                for (int d = 1; d <= 9; d++) if (cc & (1u << (d-1))) ds[nd++] = d;
                order_vals(ic, ds, nd);
                m = tlen; int ok2 = assign(ic, ds[0]);
                PropDetail pd2; int r2 = ok2 ? propagate_detail(&pd2) : -1;
                undo_to(m);
                memcpy(freed, freed_before, sizeof(freed));

                diff_n++;
                if (r1 < 0) diff_mrv_contra++; else { diff_mrv_filled += pd1.filled; diff_mrv_hs += pd1.n_hidden; }
                if (r2 < 0) diff_cm_contra++;  else { diff_cm_filled += pd2.filled; diff_cm_hs += pd2.n_hidden; }
            }
            /* 沿 critmrv 走一步 */
            compute_freed(); compute_crit();
            ic = pick_cell_critmrv();
            if (ic < 0) break;
            unsigned cc2 = cand[ic]; int ds2[9], nd2 = 0;
            for (int d = 1; d <= 9; d++) if (cc2 & (1u << (d-1))) ds2[nd2++] = d;
            order_vals(ic, ds2, nd2);
            int done = 0;
            for (int k = 0; k < nd2; k++) {
                int m = tlen;
                int ok = assign(ic, ds2[k]);
                int ff = ok ? propagate_detail(NULL) : -1;
                if (ff < 0) { undo_to(m); continue; }
                done = 1; break;
            }
            if (!done) break;
            depth++;
        }
    }
    fclose(f);
    printf("模式2: 决策级对照 (N=%d 题, 沿 critmrv 路径, 每步最多 60 层)\n", idx);
    printf("=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""\n\n");
    printf("总步数 = %ld\n", total_steps);
    printf("  crit>0 的格平均占比 = %.4f  (%ld/%ld)\n",
           (double)ncrit_pos_sum/nempty_sum, ncrit_pos_sum, nempty_sum);
    printf("  过滤器退化 (crit>0 集合为空) = %ld 步 (%.4f)\n",
           degrade_steps, (double)degrade_steps/total_steps);
    printf("\nMRV 与 critmrv 的选择:\n");
    printf("  相同 = %ld (%.4f)\n", step_same, (double)step_same/(step_same+step_diff));
    printf("  不同 = %ld (%.4f)\n", step_diff, (double)step_diff/(step_same+step_diff));
    printf("  不同时, MRV 选的格 crit==0 = %ld (%.4f)\n",
           when_diff_mrv_crit0, step_diff?(double)when_diff_mrv_crit0/step_diff:0);
    printf("\n不同时, 各自展开【第一个 vo=1 值】一步的结果 (N=%ld):\n", diff_n);
    printf("  %-24s %12s %12s\n", "", "MRV选的格", "critmrv选的格");
    printf("  %-24s %12.4f %12.4f\n", "首值矛盾率",
           (double)diff_mrv_contra/diff_n, (double)diff_cm_contra/diff_n);
    long mok = diff_n - diff_mrv_contra, cok = diff_n - diff_cm_contra;
    printf("  %-24s %12.3f %12.3f\n", "不矛盾时平均填格",
           mok?(double)diff_mrv_filled/mok:0, cok?(double)diff_cm_filled/cok:0);
    printf("  %-24s %12.3f %12.3f\n", "不矛盾时平均 hidden single",
           mok?(double)diff_mrv_hs/mok:0, cok?(double)diff_cm_hs/cok:0);
}

/* ============ 模式3: 结构分析 ============ */
static void mode_struct(const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) { fprintf(stderr, "打不开 %s\n", fn); return; }
    char line[512]; int idx = 0;
    /* 分组统计 */
    long ncell[MAXC] = {0};
    double candsum[MAXC] = {0};
    double peersum[MAXC] = {0};       /* 空 peer 中含该格候选的总数 */
    double f2peer[MAXC] = {0};        /* 该格参与的 unit 中 freedom==2 的个数 (即 crit, 冗余) */
    double minf[MAXC] = {0};          /* 候选的最小自由度 */
    double deg[MAXC] = {0};           /* 所属 unit 的"紧张度": Σ over units of (9-已填) */
    long ncell_all = 0;
    /* crit 与候选数的联合分布 */
    long joint[MAXC][10];
    memset(joint, 0, sizeof(joint));
    /* backdoor 相关: crit>0 的格与 crit=0 的格, 在 K=3 backdoor 中的频率 (由 bdcrit 已测, 此处略) */

    while (fgets(line, sizeof(line), f)) {
        int n = 0;
        for (char *p = line; *p && n < 81; p++) {
            if (*p >= '1' && *p <= '9') g_grid[n++] = *p - '0';
            else if (*p == '.' || *p == '0') g_grid[n++] = 0;
        }
        if (n != 81) continue;
        if (++idx > 2000) break;
        reset_state();
        propagate_detail(NULL);
        compute_freed(); compute_crit();
        for (int i = 0; i < 81; i++) {
            if (!cand[i]) continue;
            int c0 = crit[i]; if (c0 >= MAXC) c0 = MAXC-1;
            int nc = __builtin_popcount(cand[i]);
            ncell[c0]++; ncell_all++;
            candsum[c0] += nc;
            if (nc <= 9) joint[c0][nc]++;
            /* 空 peer 中含候选的总数 */
            int ps = 0;
            for (int p = 0; p < NPEER[i]; p++) {
                int j = PEERS[i][p];
                if (cand[j]) ps += __builtin_popcount(cand[i] & cand[j]);
            }
            peersum[c0] += ps;
            /* 候选的最小自由度 */
            int mn = 99;
            for (int d = 1; d <= 9; d++) if (cand[i] & (1u<<(d-1))) {
                int a=freed[ROWOF[i]][d-1], b=freed[9+COLOF[i]][d-1], c=freed[18+BOXOF[i]][d-1];
                int m = a<b?(a<c?a:c):(b<c?b:c);
                if (m < mn) mn = m;
            }
            if (mn < 99) minf[c0] += mn;
            /* 所属 3 个 unit 的紧张度 */
            int u1=ROWOF[i], u2=9+COLOF[i], u3=18+BOXOF[i];
            int tight = 0;
            int us[3] = {u1,u2,u3};
            for (int q = 0; q < 3; q++) {
                int u = us[q];
                unsigned placed = u<9?rowm[u]:(u<18?colm[u-9]:boxm[u-18]);
                tight += (9 - __builtin_popcount(placed));
            }
            deg[c0] += tight;
        }
    }
    fclose(f);
    printf("模式3: 结构分析 (N=%d 题, 初盘传播后, 只看空格)\n", idx);
    printf("=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""=""\n\n");
    printf("%-8s %8s %9s %10s %10s %10s\n",
           "crit值", "格数", "占比", "平均候选", "平均minF", "unit紧张度");
    for (int c = 0; c < MAXC; c++) {
        if (!ncell[c]) continue;
        printf("%-8d %8ld %9.4f %10.3f %10.3f %10.3f\n",
               c, ncell[c], (double)ncell[c]/ncell_all,
               candsum[c]/ncell[c], minf[c]/ncell[c], deg[c]/ncell[c]);
    }
    printf("\n【crit 值 × 候选数 联合分布】(行=crit, 列=候选数, 值为格数)\n");
    printf("%-6s", "crit\\nc");
    for (int nc = 1; nc <= 9; nc++) printf("%7d", nc);
    printf("\n");
    for (int c = 0; c < 8; c++) {
        int any = 0;
        for (int nc = 1; nc <= 9; nc++) if (joint[c][nc]) any = 1;
        if (!any) continue;
        printf("%-6d", c);
        for (int nc = 1; nc <= 9; nc++) printf("%7ld", joint[c][nc]);
        printf("\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "用法: %s micro|decide|struct puzzles.txt\n", argv[0]);
        return 1;
    }
    init_tables();
    const char *mode = argv[1];
    if (strcmp(mode, "micro") == 0) mode_micro(argv[2]);
    else if (strcmp(mode, "decide") == 0) mode_decide(argv[2]);
    else if (strcmp(mode, "struct") == 0) mode_struct(argv[2]);
    else { fprintf(stderr, "未知模式\n"); return 1; }
    return 0;
}
