/* cstar_dump.c — 导出 (传播后特征, backdoor 关键格 c* 及其值, 基线代价)
 *
 * 背景:
 *   "单个最关键信息" = c* (加上它的真解值后剩余猜测数最小的格)
 *   完美知道 c* 的上界 = 0.064x (降 93.6%), 远高于其他候选信息
 *   (对比: 最优策略 60选1 只有 0.320x; "是否切换策略" 二分类只有 0.979x)
 *
 *   方案A(只需位置, 强制第一分支点 = c*): 命中 0.484x, 未命中 1.166x
 *   => top-1 盈亏线 = 24.3%, 随机基线 ~2%
 *
 * 用法: ./cstar_dump <题库> <N> <SP> <out.bin>
 *   N  = 导出题数 (默认 2000)
 *   SP = 传播级别 0/1/2 (默认 2 = locked+naked, 我们的最优配置)
 *
 * 输出二进制:
 *   header: int[3] = { nq, NF=13, 81 }
 *   每题:  81*13 bytes 特征 + 1 byte c*位置 + 1 byte c*值 + 4 byte 基线B(int32)
 *   = 1059 bytes/题
 *
 * 特征布局 (每格 13 维, 共 81 格):
 *   [0..8]   候选位 d=1..9      (传播到不动点后)
 *   [9]      crit(i)
 *   [10]     pc(i) = |C(i)|
 *   [11]     prodF(i)*100       (截断到 255)
 *   [12]     nEmptyPeer(i)
 *
 * ⚠ 坑 56: 特征是"块布局"(先 81*9 候选, 再 81 个 crit, ...),
 *          逐格交错解析会全串位. 本程序按块写, cstar_learn.c 按块读.
 * ⚠ 坑 45/50: 每次 our_solve 前必须 our_set_strategy 重置;
 *             且 our_prop_only 必须在最后一次 our_solve 之后调用
 *             (our_solve 会把盘面回溯到初始态)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define OUR_SOLVER_LIB
#include "consolidate.c"

#define MAXQ 60000
#define NF   13
#define ROW  1059          /* 81*13 + 1 + 1 + 4 */

static char gr[MAXQ][82];

static int loadq(const char *fn, int maxq, int *nq) {
    FILE *f = fopen(fn, "r"); if (!f) return 0;
    char line[512]; *nq = 0;
    while (fgets(line, sizeof line, f) && *nq < maxq) {
        int n = 0;
        for (char *p = line; *p && n < 81; p++) {
            if (*p >= '1' && *p <= '9')      gr[*nq][n++] = *p;
            else if (*p == '.' || *p == '0') gr[*nq][n++] = '.';
        }
        if (n == 81) { gr[*nq][81] = 0; (*nq)++; }
    }
    fclose(f); return *nq;
}

int main(int argc, char **argv) {
    const char *fn  = argc > 1 ? argv[1] : "forum_hardest_1905_11plus.txt";
    int N  = argc > 2 ? atoi(argv[2]) : 2000;
    int SP = argc > 3 ? atoi(argv[3]) : 2;
    const char *out = argc > 4 ? argv[4] : "cstar.bin";

    int nqd = 0;
    if (!loadq(fn, N, &nqd)) { fprintf(stderr, "load fail: %s\n", fn); return 1; }
    printf("loaded %d puzzles\n", nqd); fflush(stdout);

    our_init(); our_set_splevel(SP); our_set_limit(1);
    our_set_oracle_val(0, NULL, -1); our_set_force_cell(-1, 0);
    int k0[3] = { 0, 3, 4 };

    FILE *fo = fopen(out, "wb");
    if (!fo) { fprintf(stderr, "open fail: %s\n", out); return 1; }
    int hdr[3] = { 0, NF, 81 };
    fwrite(hdr, sizeof(int), 3, fo);

    unsigned char row[ROW];
    int ndone = 0;
    long sumB = 0, sumK = 0;

    for (int q = 0; q < nqd; q++) {
        our_set_strategy(10, 1, 3, k0, 1);
        OurResult R0 = our_solve(gr[q]);
        if (!R0.solved) continue;
        char sol[82]; our_last_sol(sol);
        long B = R0.guesses;

        int empt[81], ne = 0;
        for (int i = 0; i < 81; i++) if (gr[q][i] == '.') empt[ne++] = i;
        if (ne < 2) continue;

        /* 找 c*: 加它的真解值后代价最小的格 */
        char hint[82]; strcpy(hint, gr[q]);
        long bestK = B; int bc = -1;
        for (int t = 0; t < ne; t++) {
            int j = empt[t];
            hint[j] = sol[j];
            our_set_strategy(10, 1, 3, k0, 1);
            OurResult R = our_solve(hint);
            hint[j] = '.';
            if (R.solved && R.guesses < bestK) { bestK = R.guesses; bc = j; }
        }
        if (bc < 0) continue;

        /* 特征必须在最后一次 our_solve 之后重新计算 */
        if (our_prop_only(gr[q]) < 0) continue;

        int p = 0;
        for (int i = 0; i < 81; i++)
            for (int d = 1; d <= 9; d++)
                row[p++] = (unsigned char)((cand[i] & (1u << (d - 1))) ? 1 : 0);
        for (int i = 0; i < 81; i++) row[p++] = (unsigned char)our_crit_at(i);
        for (int i = 0; i < 81; i++) row[p++] = (unsigned char)our_pc_at(i);
        for (int i = 0; i < 81; i++) {
            double v = our_prodF_at(i) * 100.0;
            if (v > 255) v = 255; if (v < 0) v = 0;
            row[p++] = (unsigned char)(v + 0.5);
        }
        for (int i = 0; i < 81; i++) row[p++] = (unsigned char)our_nEmptyPeer_at(i);
        row[p++] = (unsigned char)bc;
        row[p++] = (unsigned char)(sol[bc] - '0');
        int Bi = (int)B; memcpy(row + p, &Bi, 4); p += 4;
        if (p != ROW) { fprintf(stderr, "ROW mismatch %d\n", p); return 1; }

        fwrite(row, 1, p, fo);
        ndone++; sumB += B; sumK += bestK;
        if (ndone % 200 == 0) {
            printf("  dump %d/%d  avgB=%.2f avgKappa=%.2f\n",
                   ndone, nqd, (double)sumB/ndone, (double)sumK/ndone);
            fflush(stdout);
        }
    }

    fseek(fo, 0, SEEK_SET);
    hdr[0] = ndone; fwrite(hdr, sizeof(int), 3, fo);
    fclose(fo);
    printf("DUMP done nq=%d avgB=%.3f avgKappa=%.3f ratio=%.4f -> %s\n",
           ndone, (double)sumB/ndone, (double)sumK/ndone,
           (double)sumK/(double)sumB, out);
    return 0;
}
