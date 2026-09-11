#!/usr/bin/env bash
# 获取第三方源码（本仓库不随包分发第三方代码）
#
# 用法:  bash fetch_third_party.sh [all|tdoku|jczsolve]
#
# 注意:
#   1. 运行本脚本即表示你已阅读 THIRD_PARTY.md 并自行承担合规责任
#   2. jczsolve 版权未指定，**禁止再分发**；仅供个人研究使用
set -u

DEST="${1:-all}"
mkdir -p third_party

fetch_tdoku() {
    echo "==> tdoku (BSD 2-Clause)"
    if [ -d third_party/tdoku ]; then
        echo "    已存在，跳过"
    else
        git clone --depth 1 https://github.com/t-dillon/tdoku third_party/tdoku \
            || { echo "    克隆失败（无网络？）"; return 1; }
    fi
    # fsss / fsss2 源码位于 tdoku 仓库的 other/ 下
    [ -f third_party/tdoku/other/fsss/fsss.cc ] && echo "    fsss.cc: OK"
    [ -f third_party/tdoku/other/module_fsss2/fsss2.cpp ] && echo "    fsss2.cpp: OK"
    echo
    echo "    ⚠️ 编译 tdoku 必须用 -mavx2 -mbmi2，不要用 -march=native"
    echo "       （后者可能编出 avx512vbmi，运行时 Illegal instruction）"
}

fetch_jczsolve() {
    echo "==> jczsolve  ⚠️ 版权未指定，禁止再分发"
    echo "    源码历史版本位于 gzSudoku 仓库:"
    echo "      ./others/jczslover/JCZSolve.c   （注意目录名拼写为 jczslover）"
    echo "    或 forum.enjoysudoku.com 原帖 (3.77us solver 主题)"
    echo
    echo "    请手动下载到 third_party/JCZSolve.c 后再编译 exp/jzrun.c。"
    echo "    本脚本不自动下载——版权状态不明，自动获取与再分发均有风险。"
}

case "$DEST" in
    all)      fetch_tdoku; echo; fetch_jczsolve ;;
    tdoku)    fetch_tdoku ;;
    jczsolve) fetch_jczsolve ;;
    *)        echo "用法: $0 [all|tdoku|jczsolve]" ; exit 1 ;;
esac
