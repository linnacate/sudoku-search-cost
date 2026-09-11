#!/usr/bin/env bash
# 编译本 artifact 的全部原创工具
#
# 用法: bash build.sh
# 产物: bin/
#
# 说明:
#   - 本脚本只编译**原创**代码，不含任何第三方源码
#   - 需要 tdoku/fsss 的程序（bench, crossarch*, wallbench, costbreak）
#     见 build_3rd.sh —— 须先 bash fetch_third_party.sh
set -u
cd "$(dirname "$0")"

mkdir -p bin
OK=0; FAIL=0
declare -a FAILED=()

# ---------- 基础库 ----------
# consolidate.c 必须用 gcc 编（g++ 会报 -fpermissive: void*→long*）
# 且必须带 -DOUR_SOLVER_LIB —— OurResult 的 typedef 在该 #ifdef 块内（坑 74）
echo "==> 构建基础库"
if gcc -O2 -DOUR_SOLVER_LIB -c core/consolidate.c -o bin/cons_lib.o 2>/dev/null; then
    echo "    cons_lib.o OK"
else
    echo "    cons_lib.o 失败！"; exit 1
fi

# critlag / critprobe 依赖带探针的变体，不是主 consolidate.c
if gcc -O2 -DOUR_SOLVER_LIB -c exp/consolidate_critprobe.c -o bin/cons_critprobe.o 2>/dev/null; then
    echo "    cons_critprobe.o OK"
else
    echo "    cons_critprobe.o 失败！"; exit 1
fi

# ---------- 编译单个工具 ----------
# 参数: $1=源文件名(无扩展) $2=源文件路径 $3=额外 .o
# 注: 源码分目录存放后，#include "consolidate.c" 需 -I core 才能解析
#     部分文件含 extern "C"（spscan/bench），必须 g++ —— 故 gcc 失败后回退 g++
# -DOUR_SOLVER_LIB: OurResult 的 typedef 在该 #ifdef 块内（坑 74），
#                   所有调用 our_solve() 的 driver 都必须带此宏
build() {
    local name="$1" src="$2" extra="${3:-}"
    local D="-DOUR_SOLVER_LIB"
    if gcc -O2 $D -I core -o "bin/$name" "$src" bin/cons_lib.o $extra -lm 2>/dev/null; then
        OK=$((OK+1))
    elif gcc -O2 $D -I core -o "bin/$name" "$src" $extra -lm 2>/dev/null; then
        OK=$((OK+1))
    elif g++ -O2 $D -I core -o "bin/$name" "$src" bin/cons_lib.o $extra -lm 2>/dev/null; then
        OK=$((OK+1))
    elif g++ -O2 $D -I core -o "bin/$name" "$src" $extra -lm 2>/dev/null; then
        OK=$((OK+1))
    else
        FAIL=$((FAIL+1)); FAILED+=("$name")
    fi
}

# thm/ — §3 理论结果
build verify_thm  thm/verify_thm.c
build canon_cv    thm/canon_cv.c
build canon_test  thm/canon_test.c
build collide     thm/collide.c

# exp/ — §5/§6 实验
build abl2        exp/abl2.c
build abl3        exp/abl3.c
build keyscan     exp/keyscan.c
build keycombo    exp/keycombo.c
build sp2         exp/sp2.c
build sp3         exp/sp3.c
build verify_sp   exp/verify_sp.c
build sweep_sp    exp/sweep_sp.c
build hcost       exp/hcost.c
build hcost2      exp/hcost2.c
build hcost_locked exp/hcost_locked.c
build critmech    exp/critmech.c
build critwhy     exp/critwhy.c
build bandbranch  exp/bandbranch.c
build bandcrit    exp/bandcrit.c
build bandcrit2   exp/bandcrit2.c
build syminv      exp/syminv.c
build row1exp     exp/row1exp.c
# ---- 独立求解器变体 ----
# 这些文件自带 main（位于 #ifndef OUR_SOLVER_LIB 内），故**不能**加该宏，
# 也不能链 cons_lib.o（否则符号重复）。它们用于对照实验：
#   alt        = §6.3.2 分层反转策略（已并入主 consolidate.c）
#   dsw        = §2.115 深度定位
#   hkey       = §2.118 新启发式首轮
#   ewdeg      = §2.119 e-wdeg 前置判据
#   critstatic = §2.111 crit 时效性
build_solver() {
    local name="$1" src="$2"
    if gcc -O2 -I core -o "bin/$name" "$src" -lm 2>/dev/null; then
        OK=$((OK+1))
    elif g++ -O2 -I core -o "bin/$name" "$src" -lm 2>/dev/null; then
        OK=$((OK+1))
    else
        FAIL=$((FAIL+1)); FAILED+=("$name")
    fi
}
build_solver consolidate_alt        exp/consolidate_alt.c
build_solver consolidate_dsw        exp/consolidate_dsw.c
build_solver consolidate_hkey       exp/consolidate_hkey.c
build_solver consolidate_ewdeg      exp/consolidate_ewdeg.c
build_solver consolidate_critstatic exp/consolidate_critstatic.c
build_solver consolidate_k6         exp/consolidate_k6.c
# 主求解器本体（论文最终配方所在，v11）
build_solver consolidate            core/consolidate.c
# v10 备份，用于复现"分层策略之前"的基线
build_solver consolidate_v10        core/consolidate_v10_backup.c
build keyprof     exp/keyprof.c
build segscan     exp/segscan.c
build abcmp       exp/abcmp.c

# 需 cons_critprobe.o 的（不能链主库）
gcc -O2 -I core -o bin/critlag   exp/critlag.c   bin/cons_critprobe.o -lm 2>/dev/null \
    && OK=$((OK+1)) || { FAIL=$((FAIL+1)); FAILED+=("critlag"); }
gcc -O2 -I core -o bin/critprobe exp/critprobe.c bin/cons_critprobe.o -lm 2>/dev/null \
    && OK=$((OK+1)) || { FAIL=$((FAIL+1)); FAILED+=("critprobe"); }

# open/ — §8 开放问题
build cstar_dump  open/cstar_dump.c
build cstar_learn open/cstar_learn.c

# ---------- 汇总 ----------
echo
echo "=============== 编译结果 ==============="
echo "成功: $OK   失败: $FAIL"
if [ $FAIL -gt 0 ]; then
    echo "失败项: ${FAILED[*]}"
fi
echo
echo "需第三方源码（bash fetch_third_party.sh 后运行 build_3rd.sh）:"
echo "  bench · crossarch · crossarch2 · wallbench · costbreak · jzrun"
