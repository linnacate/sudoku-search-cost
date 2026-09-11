# smoke_all.ps1 — 对全部已编译工具做最小可运行冒烟测试，并落盘日志
#
# 用法: powershell -NoProfile -ExecutionPolicy Bypass -File out\smoke_all.ps1 [-Root <仓库根>]
#
# 说明:
#   * 本脚本位于 out\，但**仓库根**才是它的工作目录；用 -Root 指定，
#     不指定时会自动把"脚本所在目录的上一级"当作仓库根（脚本在 out\ 下时正是如此）。
#   * abl3 / keycombo 用**硬编码文件名**从当前目录加载题库，故这两个必须 cd 到 data\ 再跑
#     （见脚本内 $NEED_DATA_DIR）。这不是缺陷，但文档里要写清楚。
#   * 输出: <Root>\logs\smoke\<tool>.txt 与 <Root>\logs\smoke\index.txt
[CmdletBinding()]
param([string]$Root = '')

$ErrorActionPreference = 'Continue'
if (-not $Root) {
    if ($PSScriptRoot) {
        $parent = Split-Path -Parent $PSScriptRoot
        if (Test-Path -LiteralPath (Join-Path $parent 'build_win')) { $Root = $parent }
        elseif (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'build_win')) { $Root = $PSScriptRoot }
        else { $Root = $parent }
    } elseif ($MyInvocation.MyCommand.Path) {
        $Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
    } else { $Root = (Get-Location).Path }
}
$Root = (Resolve-Path -LiteralPath $Root).Path
$Bin = Join-Path $Root 'build_win'
$Smoke = Join-Path $Root 'logs\smoke'
New-Item -ItemType Directory -Force -Path $Smoke | Out-Null

if (-not (Test-Path -LiteralPath $Bin)) {
    Write-Output "错误: 找不到 $Bin —— 请先运行 build_windows.ps1（或 build.sh 得到 bin/）"
    exit 2
}
Write-Output "仓库根: $Root"

$IND = 'data\indep600.txt'
$NEED_DATA_DIR = @('abl3', 'keycombo')

$CASES = [ordered]@{
    'verify_thm'             = @($IND, '30')
    'canon_test'             = @($IND, '20')
    'collide'                = @($IND, '20')
    'abl2'                   = @($IND, '20')
    'keyscan'                = @($IND, '10')
    'sp2'                    = @($IND, '20')
    'sp3'                    = @($IND, '20')
    'verify_sp'              = @($IND, '20')
    'sweep_sp'               = @($IND, '20')
    'hcost'                  = @($IND, '20')
    'hcost2'                 = @($IND, '20')
    'hcost_locked'           = @($IND, '20')
    'critmech'               = @($IND, '20')
    'critwhy'                = @('micro', $IND)
    'bandbranch'             = @($IND, '20')
    'bandcrit'               = @($IND, '20')
    'bandcrit2'              = @($IND, '20')
    'syminv'                 = @($IND, '20')
    'row1exp'                = @($IND, '20')
    'keyprof'                = @($IND, '20')
    'segscan'                = @($IND, '20')
    'abcmp'                  = @($IND, '20', '0')
    'cstar_dump'             = @($IND, '20')
    'critlag'                = @($IND, '20')
    'critprobe'              = @($IND, '20')
    'exact_bench'            = @($IND, '100', '30')
    'consolidate'            = @($IND, '20')
    'consolidate_alt'        = @($IND, '20')
    'consolidate_dsw'        = @($IND, '20')
    'consolidate_hkey'       = @($IND, '20')
    'consolidate_ewdeg'      = @($IND, '20')
    'consolidate_critstatic' = @($IND, '20')
    'consolidate_k6'         = @($IND, '20')
    'consolidate_v10'        = @($IND, '20')
    'keycombo'               = @('1', '1')
    'abl3'                   = @('20')
}

$lines = New-Object System.Collections.Generic.List[string]
$ok = 0; $bad = 0; $skip = 0

foreach ($name in $CASES.Keys) {
    $exe = Join-Path $Bin "$name.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        $lines.Add(("{0}`tSKIP`t-`t未编译成功（见 logs\build_win\{0}*.log）" -f $name)); $skip++
        continue
    }
    $argv = $CASES[$name]
    if ($NEED_DATA_DIR -contains $name) {
        $wd = Join-Path $Root 'data'
        $rel = '..\build_win\' + $name + '.exe'
    } else {
        $wd = $Root
        $rel = 'build_win\' + $name + '.exe'
    }
    $cmdline = 'cd /d "' + $wd + '" && ' + $rel + ' ' + ($argv -join ' ')
    $out = & cmd /c $cmdline 2>&1
    $code = $LASTEXITCODE
    $txt = ($out | Out-String)
    [System.IO.File]::WriteAllText((Join-Path $Smoke "$name.txt"), $txt,
        (New-Object System.Text.UTF8Encoding($false)))
    $firstLine = ($txt -split "`n" | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1)
    if ($null -eq $firstLine) { $firstLine = '(无输出)' }
    $verdict = if ($code -eq 0) { 'OK' } else { "EXIT=$code" }
    if ($code -eq 0) { $ok++ } else { $bad++ }
    $lines.Add(("{0}`t{1}`t{2}`t{3}" -f $name, $verdict, ($argv -join ' '), $firstLine.Trim()))
    Write-Output ("  {0,-24} {1,-10} {2}" -f $name, $verdict, $firstLine.Trim())
}

$summary = @("工具`t结果`t参数`t首行输出") + $lines + @(
    "",
    ("合计: OK={0}  非零退出={1}  跳过(未编译)={2}" -f $ok, $bad, $skip),
    "说明: cstar_learn 未跑（需先用 cstar_dump 生成 cstar.bin）；canon_cv / keyscan 在 MSVC 下编译失败（C99 变长数组）。",
    "底噪: 部分工具把进度写到 stderr，经 cmd 启动会带 'cmd.exe : ' 前缀，退出码仍为 0。"
)
[System.IO.File]::WriteAllLines((Join-Path $Smoke 'index.txt'), $summary,
    (New-Object System.Text.UTF8Encoding($false)))
Write-Output ""
Write-Output ("合计: OK={0}  非零退出={1}  跳过={2}；索引: logs\smoke\index.txt" -f $ok, $bad, $skip)
if ($bad -gt 0) { exit 1 } else { exit 0 }
