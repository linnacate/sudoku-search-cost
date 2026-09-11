#!/usr/bin/env bash
# 编译依赖第三方源码的工具
#
# 前置: bash fetch_third_party.sh
# 用法: bash build_3rd.sh
#
# ⚠️ 编译 tdoku 的两个硬约束（实测踩过）:
#    1. 必须 -mavx2 -mbmi2，不要用 -march=native
#       （后者可能编出 avx512vbmi，运行时 Illegal instruction）
#    2. 除 solver_dpll_triad_simd.cc 外，还需 util.o 与 grid_lib.o
#       （否则 undefined reference to Util::Permutation）
set -u
cd "$(dirname "$0")"

TD=third_party/tdoku
[ -d "$TD" ] || { echo "未找到 $TD —— 请先运行: bash fetch_third_party.sh"; exit 1; }

mkdir -p bin
CXX="g++ -O3 -mavx2 -mbmi2"

echo "==> 编译 tdoku"
$CXX -c $TD/src/solver_dpll_triad_simd.cc -o bin/tdoku_simd.o 2>/dev/null \
  || $CXX -c $TD/solver_dpll_triad_simd.cc -o bin/tdoku_simd.o 2>/dev/null \
  || { echo "    tdoku 源码编译失败（路径可能变动，请检查 $TD 结构）"; exit 1; }
$CXX -c $TD/src/util.cc       -o bin/tdoku_util.o 2>/dev/null || \
$CXX -c $TD/util/util.cc      -o bin/tdoku_util.o 2>/dev/null || echo "    util.cc 未找到"
$CXX -c $TD/src/grid_lib.cc   -o bin/tdoku_grid.o 2>/dev/null || \
$CXX -c $TD/util/grid_lib.cc  -o bin/tdoku_grid.o 2>/dev/null || echo "    grid_lib.cc 未找到"
echo "    tdoku_simd.o OK"

echo "==> 编译 fsss"
# 注意: fsss 许可证为非标准声明，商业用途受限，详见 THIRD_PARTY.md
$CXX -c $TD/other/fsss/fsss.cc -o bin/fsss.o 2>/dev/null \
  && echo "    fsss.o OK" || echo "    fsss.cc 未找到（跳过）"

echo "==> 编译我们的库（供对比 driver 链接）"
# consolidate.c 必须 gcc 编 + -DOUR_SOLVER_LIB
gcc -O2 -DOUR_SOLVER_LIB -c core/consolidate.c -o bin/ourlib.o || exit 1

echo "==> 编译对比 driver"
ok=0; fail=0
for d in crossarch crossarch2 wallbench costbreak; do
    if $CXX -o bin/$d exp/h2h/$d.cc bin/ourlib.o bin/tdoku_simd.o \
            bin/tdoku_util.o bin/tdoku_grid.o bin/fsss.o -lm 2>/dev/null; then
        echo "    $d OK"; ok=$((ok+1))
    else
        echo "    $d 失败（可能缺某个 .o 或头文件路径不同）"; fail=$((fail+1))
    fi
done

# bench: 需 tdoku
if $CXX -o bin/bench exp/bench.c bin/ourlib.o bin/tdoku_simd.o \
        bin/tdoku_util.o bin/tdoku_grid.o -lm 2>/dev/null; then
    echo "    bench OK"; ok=$((ok+1))
else
    echo "    bench 失败"; fail=$((fail+1))
fi

echo "==> jczsolve"
echo "    ⚠️ 版权未指定，本脚本不自动获取。"
echo "       若已自行取得 JCZSolve.c，放入 third_party/ 后:"
echo "       gcc -O2 -o bin/jzrun exp/jzrun.c third_party/JCZSolve.c -lm"

echo
echo "成功: $ok   失败: $fail"
