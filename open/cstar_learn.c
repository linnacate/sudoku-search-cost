/* cstar_learn.c — 从 cstar.bin 学习 c* 位置, 扫训练量, 输出学习曲线
 *
 * 核心问题 (用户提出):
 *   "主动创造过拟合" (把代价转移到离线) vs "学到泛化指标"
 *   判别方法: 看 held-out 准确率是否随训练量 N 上升
 *     - 上升     -> 真泛化, 模型可以远小于题目空间
 *     - 平坦     -> 纯记忆, 需要表 ∝ N ("覆盖即泛化" 的唯一形式)
 *     - train 高 / test 低 -> gap 大, 记忆而非泛化
 *
 * 模型: pointwise logistic ranking (13 维共享权重 + 偏置)
 *   对所有候选格打分, 取 top-k; 正样本 = c*, 负样本 = 其余候选格
 *   正负极度不平衡 (~1:50), 正样本按 (ncand-1) 加权平衡总梯度
 *
 * 用法: ./cstar_learn <bin> <ntest> [epochs] [lr]
 *   ntest = held-out 测试题数 (从末尾取, 永不参与训练), 默认 2000
 *
 * 盈亏线 (方案A: 只需 c* 位置, 强制第一分支点):
 *   命中 0.484x, 未命中 1.166x  =>  top-1 准确率需 > 24.3%
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NF  13
#define ROW 1059

static unsigned char *feat;     /* [nq][81][NF] */
static unsigned char *lab;      /* [nq] */
static unsigned char *labv;     /* [nq] */
static int           *B;        /* [nq] */
static int nq = 0;

static unsigned myrand_s = 12345;
static unsigned myr(void) { myrand_s = myrand_s * 1103515245u + 12345u; return myrand_s >> 8; }

static int loadbin(const char *fn) {
    FILE *f = fopen(fn, "rb"); if (!f) return 0;
    int hdr[3];
    if (fread(hdr, sizeof(int), 3, f) != 3) { fclose(f); return 0; }
    nq = hdr[0];
    if (hdr[1] != NF || hdr[2] != 81 || nq <= 0) { fclose(f); return 0; }
    feat = (unsigned char*)malloc((size_t)nq * 81 * NF);
    lab  = (unsigned char*)malloc((size_t)nq);
    labv = (unsigned char*)malloc((size_t)nq);
    B    = (int*)malloc((size_t)nq * sizeof(int));
    if (!feat || !lab || !labv || !B) { fprintf(stderr, "oom\n"); exit(1); }
    unsigned char *row = (unsigned char*)malloc(ROW);
    for (int q = 0; q < nq; q++) {
        if (fread(row, 1, ROW, f) != ROW) { fclose(f); return 0; }
        memcpy(feat + (size_t)q * 81 * NF, row, 81 * NF);
        lab[q]  = row[81*NF];
        labv[q] = row[81*NF + 1];
        memcpy(&B[q], row + 81*NF + 2, 4);
    }
    free(row); fclose(f); return nq;
}

/* 候选格: popcount(cand[i]) >= 2 —— 传播到不动点后仍未确定的格 */
static int cand_list(const unsigned char *f, int *out) {
    int n = 0;
    for (int i = 0; i < 81; i++) {
        int pc = 0;
        for (int d = 0; d < 9; d++) pc += f[d * 81 + i];   /* 块布局: 候选位 d 在 [d*81 + i] */
        if (pc >= 2) out[n++] = i;
    }
    return n;
}
static double fv(const unsigned char *f, int i, int k) {
    /* 块布局: k<9 -> [k*81+i]; k>=9 -> [(81*9) + (k-9)*81 + i] */
    if (k < 9) return (double)f[k * 81 + i];
    return (double)f[81 * 9 + (k - 9) * 81 + i];
}

static int rank_of(const unsigned char *f, int cell, const double *w, double b,
                   const double *mu, const double *sd, int *ncand_out) {
    int cand[81], nc = cand_list(f, cand);
    if (ncand_out) *ncand_out = nc;
    double xs[NF], sstar = b;
    for (int k = 0; k < NF; k++) {
        xs[k] = (fv(f, cell, k) - mu[k]) / sd[k];
        sstar += w[k] * xs[k];
    }
    int better = 0;
    for (int t = 0; t < nc; t++) {
        int i = cand[t]; if (i == cell) continue;
        double s = b;
        for (int k = 0; k < NF; k++) s += w[k] * (fv(f, i, k) - mu[k]) / sd[k];
        if (s > sstar) better++;
    }
    return better + 1;      /* rank, 1 = 命中 */
}

int main(int argc, char **argv) {
    const char *fn = argc > 1 ? argv[1] : "cstar.bin";
    int ntest  = argc > 2 ? atoi(argv[2]) : 2000;
    int epochs = argc > 3 ? atoi(argv[3]) : 8;
    double lr0 = argc > 4 ? atof(argv[4]) : 0.05;

    if (!loadbin(fn)) { fprintf(stderr, "load fail %s\n", fn); return 1; }
    printf("loaded nq=%d\n", nq);
    if (ntest >= nq) { fprintf(stderr, "ntest too large\n"); return 1; }
    int ntrain_pool = nq - ntest;
    printf("train pool=%d  held-out test=%d\n\n", ntrain_pool, ntest);

    /* 随机基线: 平均 1/ncand (在测试集上) */
    double sumInv = 0; int cnt = 0;
    for (int q = ntrain_pool; q < nq; q++) {
        int cand[81], nc = cand_list(feat + (size_t)q*81*NF, cand);
        if (nc >= 2) { sumInv += 1.0 / nc; cnt++; }
    }
    double rand_base = sumInv / cnt;
    printf("random top-1 baseline = %.4f  (avg ncand = %.1f)\n\n", rand_base, 1.0/rand_base);

    int sizes[] = { 60, 120, 240, 500, 1000, 2000, 4000, 8000, 16000, 32000, 0 };
    int ns = sizeof(sizes)/sizeof(sizes[0]);

    printf("%8s | %22s | %22s | %6s | %6s | %6s\n",
           "Ntrain", "in-sample t1/t3/t5/t10", "held-out t1/t3/t5/t10", "gap_t1", "vsB", "rand");
    printf("---------+------------------------+------------------------+--------+--------+--------\n");

    double recN[16], recT1[16]; int nrec = 0;
    int prevN = -1;
    for (int si = 0; si < ns; si++) {
        int N = sizes[si];
        if (N == 0) N = ntrain_pool;
        if (N > ntrain_pool) continue;
        if (N == prevN) continue;          /* 阶梯值与满池值撞车时去重 */
        prevN = N;

        /* 归一化统计量 (仅用训练集) */
        double mu[NF], sd[NF], sums[NF] = {0}, sums2[NF] = {0};
        long cntf = 0;
        for (int q = 0; q < N; q += 7) {          /* 抽样算, 够用 */
            const unsigned char *f = feat + (size_t)q*81*NF;
            for (int i = 0; i < 81; i++) {
                for (int k = 0; k < NF; k++) {
                    double v = fv(f, i, k);
                    sums[k] += v; sums2[k] += v*v;
                }
                cntf++;
            }
        }
        for (int k = 0; k < NF; k++) {
            mu[k] = sums[k] / cntf;
            double var = sums2[k] / cntf - mu[k]*mu[k];
            sd[k] = var > 1e-9 ? sqrt(var) : 1.0;
        }

        double w[NF] = {0}, b = 0;
        double xs[NF];
        int *idx = (int*)malloc(N * sizeof(int));
        for (int i = 0; i < N; i++) idx[i] = i;

        for (int ep = 0; ep < epochs; ep++) {
            double lr = lr0 / (1.0 + 0.5 * ep);
            for (int i = N - 1; i > 0; i--) { int j = myr() % (i+1); int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
            int nc_seen = 0; double wpos_seen = 0;
            for (int t = 0; t < N; t++) {
                int q = idx[t];
                const unsigned char *f = feat + (size_t)q*81*NF;
                int cand[81], nc = cand_list(f, cand);
                if (nc < 2) continue;
                nc_seen++; wpos_seen += (nc - 1);
                double wpos = (double)(nc - 1);
                for (int c = 0; c < nc; c++) {
                    int i = cand[c];
                    double y = (i == lab[q]) ? 1.0 : 0.0;
                    double s = b;
                    for (int k = 0; k < NF; k++) { xs[k] = (fv(f,i,k) - mu[k]) / sd[k]; s += w[k]*xs[k]; }
                    double p = 1.0 / (1.0 + exp(-s));
                    double wt = y > 0.5 ? wpos : 1.0;
                    double g = (p - y) * wt;
                    for (int k = 0; k < NF; k++) w[k] -= lr * g * xs[k];
                    b -= lr * g * 0.1;
                }
            }
        }
        free(idx);

        /* 评估: in-sample (前 min(N,2000) 题) 与 held-out */
        int nEv = N < 2000 ? N : 2000;
        double h_tr[11] = {0}, h_te[11] = {0};
        for (int q = 0; q < nEv; q++) {
            int nc; int r = rank_of(feat + (size_t)q*81*NF, lab[q], w, b, mu, sd, &nc);
            for (int k = 1; k <= 10; k++) if (r <= k) h_tr[k]++;
        }
        for (int q = ntrain_pool; q < nq; q++) {
            int nc; int r = rank_of(feat + (size_t)q*81*NF, lab[q], w, b, mu, sd, &nc);
            for (int k = 1; k <= 10; k++) if (r <= k) h_te[k]++;
        }
        double t1=h_te[1]/ntest, t3=h_te[3]/ntest, t5=h_te[5]/ntest, t10=h_te[10]/ntest;
        double i1=h_tr[1]/nEv, i3=h_tr[3]/nEv, i5=h_tr[5]/nEv, i10=h_tr[10]/nEv;
        double vsB = t1 * 0.484 + (1.0 - t1) * 1.166;
        printf("%8d | %5.3f %5.3f %5.3f %5.3f | %5.3f %5.3f %5.3f %5.3f | %6.3f | %6.3f | %6.3f\n",
               N, i1,i3,i5,i10, t1,t3,t5,t10, i1-t1, vsB, rand_base);
        fflush(stdout);
        if (nrec < 16) { recN[nrec] = (double)N; recT1[nrec] = t1; nrec++; }
    }

    /* 外推: 若 held-out top-1 随 log(N) 线性增长, 反推达到盈亏线所需 N */
    if (nrec >= 3) {
        double sx=0, sy=0, sxx=0, sxy=0;
        for (int i = 0; i < nrec; i++) {
            double x = log(recN[i]), y = recT1[i];
            sx += x; sy += y; sxx += x*x; sxy += x*y;
        }
        double den = nrec*sxx - sx*sx;
        if (fabs(den) > 1e-9) {
            double bb = (nrec*sxy - sx*sy) / den;
            double aa = (sy - bb*sx) / nrec;
            printf("\n外推 (t1 = %.4f + %.5f * ln N):\n", aa, bb);
            if (bb > 1e-6) {
                double need = exp((0.243 - aa) / bb);
                printf("  达到盈亏线 24.3%% 需要 N = %.3g   (当前池 %d)\n",
                       need, ntrain_pool);
                if (need > 1e12) printf("  >> 远超可用题目数, 靠覆盖也够不到\n");
            } else {
                printf("  斜率 <= 0: held-out 不随 N 上升, 外推无解\n");
                printf("  >> 学习曲线平坦 = 纯记忆, 无泛化\n");
            }
        }
    }

    printf("\n判读:\n");
    printf("  held-out top-1 随 N 单调上升 -> 真泛化 (模型 << 题目空间)\n");
    printf("  held-out top-1 平坦          -> 纯记忆 (只能靠覆盖, 表 ∝ N)\n");
    printf("  in-sample 高 / held-out 低   -> gap 大, 过拟合而非泛化\n");
    printf("  盈亏线: held-out top-1 > 24.3%% 且 vsB < 1.000 才盈利\n");
    return 0;
}
