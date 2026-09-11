# build_windows.ps1 — Windows/MSVC 全量构建脚本（build.sh 的 Windows 等价物）
#
# 用法: powershell -NoProfile -ExecutionPolicy Bypass -File build_windows.ps1 [-Clean]
#
# 约定与 build.sh 严格一致（坑 74 / 74b）:
#   * 调用 our_solve() 的 driver  => -DOUR_SOLVER_LIB；源码若已 #include "consolidate.c"
#     则**不能**再链 cons_lib.obj（LNK2005 重复定义），故带库失败后自动回退到不带库重试
#   * 自带 main() 的独立求解器变体 => **不**加该宏，也**不**链 cons_lib.obj
#   * 兼容垫片 compat/ 只影响墙钟（clock_gettime / gettimeofday），不触碰决策逻辑
#
# 与 build.sh 的差异（有意为之）:
#   * 补上 build.sh 遗漏的 tools/exact_bench.c（README 的"零容忍"判据工具）
#   * 用 compat/verify_thm_msvc.c 代替 thm/verify_thm.c（后者用了 GCC 语句表达式）
#   * 不依赖 bash/gcc/make
#
# 踩过的坑（都写进注释，避免后人重踩）:
#   1) Windows PowerShell 5.1 在 -File 模式下 $PSScriptRoot 作 param 默认值不可靠 -> 显式解析
#   2) 5.1 会把 "a && b" 当成语句分隔符报错 -> 字符串里用 `& 转义
#   3) 5.1 读无 BOM 的 UTF-8 会按 ANSI 解释，中文注释乱码并导致引号配对失败 -> 本文件必须带 BOM
#   4) cl 由 cmd /c 启动，相对路径相对 cmd 的当前目录解析 -> 所有路径一律绝对
#   5) MSVC 安装路径含空格，经 cmd /c 拼串会被拆成多个参数 -> 用 8.3 短路径
#   6) '/I' '.' 里的空格同样会被拆分 -> 不使用（cl 默认已含当前目录）
#   7) 函数参数名不要用会被调用方同名变量覆盖的名字（曾导致日志文件名变成对象路径）
[CmdletBinding()]
param(
    [switch]$Clean,
    [string]$Root = ''
)

$ErrorActionPreference = 'Continue'
if (-not $Root) {
    if ($PSScriptRoot) { $Root = $PSScriptRoot }
    elseif ($MyInvocation.MyCommand.Path) { $Root = Split-Path -Parent $MyInvocation.MyCommand.Path }
    else { $Root = (Get-Location).Path }
}
$Root = (Resolve-Path -LiteralPath $Root).Path
$Out  = Join-Path $Root 'build_win'
$Log  = Join-Path $Root 'build_windows.log'
$report = New-Object System.Collections.Generic.List[string]
function Say([string]$m) { Write-Output $m; $report.Add($m) }

function Find-VcVars64 {
    $c = @()
    $pf86 = ${env:ProgramFiles(x86)}
    if (-not $pf86) { $pf86 = 'C:\Program Files (x86)' }
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        try {
            $inst = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
            if ($inst) { $c += (Join-Path ($inst | Select-Object -First 1).Trim() 'VC\Auxiliary\Build\vcvars64.bat') }
        } catch { }
    }
    foreach ($ver in @('18','17','16')) {
        $c += "C:\Program Files (x86)\Microsoft Visual Studio\$ver\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
        $c += "C:\Program Files\Microsoft Visual Studio\$ver\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
        $c += "C:\Program Files\Microsoft Visual Studio\$ver\Community\VC\Auxiliary\Build\vcvars64.bat"
        $c += "C:\Program Files\Microsoft Visual Studio\$ver\Professional\VC\Auxiliary\Build\vcvars64.bat"
        $c += "C:\Program Files\Microsoft Visual Studio\$ver\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    }
    foreach ($x in $c) { if ($x -and (Test-Path -LiteralPath $x)) { return $x } }
    return $null
}

$vcvars = Find-VcVars64
if (-not $vcvars) { Say "错误: 找不到 vcvars64.bat（需 Visual Studio Build Tools 的 C++ 工作负载）"; exit 2 }
Say "==> 编译器环境: $vcvars"

$dump = cmd /c "call `"$vcvars`" >nul 2>&1 `& set"
$loaded = 0
foreach ($line in $dump) {
    if ($line -match '^([^=]+)=(.*)$') {
        try { Set-Item -Path ("Env:" + $Matches[1]) -Value $Matches[2] -ErrorAction Stop; $loaded++ } catch { }
    }
}
Say "    已导入环境变量 $loaded 项"

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $msvcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC'
    $clExe = Get-ChildItem -LiteralPath $msvcRoot -Recurse -Filter 'cl.exe' -ErrorAction SilentlyContinue |
             Where-Object { $_.FullName -match 'Hostx64\\x64' } | Select-Object -First 1
    if ($clExe) { $env:PATH = $clExe.DirectoryName + ';' + $env:PATH; Say "    兜底: 已把 $($clExe.DirectoryName) 加入 PATH" }
}
$clCmd = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clCmd) { Say "错误: 找不到 cl.exe"; $report | Set-Content -LiteralPath $Log -Encoding UTF8; exit 2 }
$Cl = $clCmd.Source
try {
    $sh = (New-Object -ComObject Scripting.FileSystemObject).GetFile($Cl).ShortPath
    if ($sh -and (Test-Path -LiteralPath $sh)) { $Cl = $sh }
} catch { }
Say "    cl.exe = $Cl"

if ($Clean -and (Test-Path -LiteralPath $Out)) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$objDir = Join-Path $Out '_obj'
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$compatDir = Join-Path $Root 'compat'
$compat    = Join-Path $compatDir 'msvc_compat.h'
if (-not (Test-Path -LiteralPath $compat)) { Say "错误: 缺少 compat\msvc_compat.h"; $report | Set-Content -LiteralPath $Log -Encoding UTF8; exit 2 }

# -I compat 让 <sys/time.h> 命中垫片；-I core 解析 #include "consolidate.c"
$common = @('/nologo','/O2','/utf-8','/W3','/wd4996','/wd4244','/wd4267','/wd4018','/wd4101','/wd4189',
            "/FI`"$compat`"", '/I', (Join-Path $Root 'core'), '/I', $compatDir)

$script:OK = 0; $script:FAIL = 0
$failed = New-Object System.Collections.Generic.List[string]
$fallbacks = New-Object System.Collections.Generic.List[string]

function Invoke-ClOnce {
    param([string[]]$ClArgs, [string]$LogFile)
    $line = $ClArgs -join ' '
    $out = & cmd /c "$line" 2>&1
    $exit = $LASTEXITCODE
    $text = ($out | Out-String)
    try { [System.IO.File]::WriteAllText($LogFile, $text, (New-Object System.Text.UTF8Encoding($false))) } catch { }
    return @{ Exit = $exit; Text = $text }
}

function Build-Tool {
    param([string]$Tool, [string]$Src, [string]$ExtraObj = '')
    $srcAbs = Join-Path $Root $Src
    $libObj = Join-Path $objDir 'cons_lib.obj'
    $exe    = Join-Path $Out ($Tool + '.exe')
    $logBase = Join-Path $Out $Tool
    if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue }

    $plan = @()
    if ($ExtraObj) {
        $plan += @{ obj = $ExtraObj; tag = '' }
    } else {
        $plan += @{ obj = $libObj;  tag = '' }
        $plan += @{ obj = '';       tag = '_nolib' }
    }

    $lastText = ''
    foreach ($p in $plan) {
        $argv = @($Cl) + $common + @('/DOUR_SOLVER_LIB', "/Fe:$exe", $srcAbs)
        if ($p.obj) { $argv += $p.obj }
        $argv += @('/link', '/SUBSYSTEM:CONSOLE')
        $r = Invoke-ClOnce -ClArgs $argv -LogFile ($logBase + $p.tag + '.log')
        $lastText = $r.Text
        if ($r.Exit -eq 0 -and (Test-Path -LiteralPath $exe)) {
            $script:OK++
            if ($p.tag -eq '_nolib') { $fallbacks.Add($Tool); $suffix = '  (回退: 源码自带 consolidate.c，不链 cons_lib.obj)' }
            else { $suffix = '' }
            Say ("    [OK]   {0}{1}" -f $Tool, $suffix)
            return
        }
    }
    $script:FAIL++; $failed.Add($Tool)
    $firstErr = ($lastText -split "`n" | Where-Object { $_ -match 'error|fatal' } | Select-Object -First 1)
    Say ("    [FAIL] {0}  :: {1}" -f $Tool, ($firstErr -replace '\s+$',''))
}

function Build-Solver {
    param([string]$Tool, [string]$Src)
    $srcAbs = Join-Path $Root $Src
    $exe = Join-Path $Out ($Tool + '.exe')
    if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue }
    $argv = @($Cl) + $common + @("/Fe:$exe", $srcAbs, '/link', '/SUBSYSTEM:CONSOLE')
    $r = Invoke-ClOnce -ClArgs $argv -LogFile (Join-Path $Out ($Tool + '.log'))
    if ($r.Exit -eq 0 -and (Test-Path -LiteralPath $exe)) {
        $script:OK++; Say ("    [OK]   {0}" -f $Tool)
    } else {
        $script:FAIL++; $failed.Add($Tool)
        $firstErr = ($r.Text -split "`n" | Where-Object { $_ -match 'error|fatal' } | Select-Object -First 1)
        Say ("    [FAIL] {0}  :: {1}" -f $Tool, ($firstErr -replace '\s+$',''))
    }
}

Say "==> 基础库"
$r = Invoke-ClOnce -ClArgs (@($Cl) + $common + @('/c','/DOUR_SOLVER_LIB',(Join-Path $Root 'core\consolidate.c'),"/Fo$objDir\cons_lib.obj")) -LogFile (Join-Path $Out 'cons_lib.log')
if ($r.Exit -ne 0) { Say "    cons_lib.obj 失败"; $report | Set-Content -LiteralPath $Log -Encoding UTF8; exit 1 }
Say "    cons_lib.obj OK"
$r = Invoke-ClOnce -ClArgs (@($Cl) + $common + @('/c','/DOUR_SOLVER_LIB',(Join-Path $Root 'exp\consolidate_critprobe.c'),"/Fo$objDir\cons_critprobe.obj")) -LogFile (Join-Path $Out 'cons_critprobe.log')
if ($r.Exit -ne 0) { Say "    cons_critprobe.obj 失败"; $report | Set-Content -LiteralPath $Log -Encoding UTF8; exit 1 }
Say "    cons_critprobe.obj OK"

Say "==> 工具（thm/ exp/ open/ tools/）"
# verify_thm 用 compat 下的 MSVC 移植版（原版含 GCC 语句表达式）
Build-Tool verify_thm   'compat\verify_thm_msvc.c'
Build-Tool canon_cv     'thm\canon_cv.c'
Build-Tool canon_test   'thm\canon_test.c'
Build-Tool collide      'thm\collide.c'
Build-Tool abl2         'exp\abl2.c'
Build-Tool abl3         'exp\abl3.c'
Build-Tool keyscan      'exp\keyscan.c'
Build-Tool keycombo     'exp\keycombo.c'
Build-Tool sp2          'exp\sp2.c'
Build-Tool sp3          'exp\sp3.c'
Build-Tool verify_sp    'exp\verify_sp.c'
Build-Tool sweep_sp     'exp\sweep_sp.c'
Build-Tool hcost        'exp\hcost.c'
Build-Tool hcost2       'exp\hcost2.c'
Build-Tool hcost_locked 'exp\hcost_locked.c'
Build-Tool critmech     'exp\critmech.c'
Build-Tool critwhy      'exp\critwhy.c'
Build-Tool bandbranch   'exp\bandbranch.c'
Build-Tool bandcrit     'exp\bandcrit.c'
Build-Tool bandcrit2    'exp\bandcrit2.c'
Build-Tool syminv       'exp\syminv.c'
Build-Tool row1exp      'exp\row1exp.c'
Build-Tool keyprof      'exp\keyprof.c'
Build-Tool segscan      'exp\segscan.c'
Build-Tool abcmp        'exp\abcmp.c'
Build-Tool cstar_dump   'open\cstar_dump.c'
Build-Tool cstar_learn  'open\cstar_learn.c'
Build-Tool exact_bench  'tools\exact_bench.c'

Say "==> 独立求解器变体（无 OUR_SOLVER_LIB / 不链基础库）"
Build-Solver consolidate_alt        'exp\consolidate_alt.c'
Build-Solver consolidate_dsw        'exp\consolidate_dsw.c'
Build-Solver consolidate_hkey       'exp\consolidate_hkey.c'
Build-Solver consolidate_ewdeg      'exp\consolidate_ewdeg.c'
Build-Solver consolidate_critstatic 'exp\consolidate_critstatic.c'
Build-Solver consolidate_k6         'exp\consolidate_k6.c'
Build-Solver consolidate            'core\consolidate.c'
Build-Solver consolidate_v10        'core\consolidate_v10_backup.c'

Say "==> 依赖 critprobe 变体的工具"
Build-Tool critlag   'exp\critlag.c'   (Join-Path $objDir 'cons_critprobe.obj')
Build-Tool critprobe 'exp\critprobe.c' (Join-Path $objDir 'cons_critprobe.obj')

$nExe = (Get-ChildItem -LiteralPath $Out -Filter '*.exe' -ErrorAction SilentlyContinue | Measure-Object).Count
Say ""
Say "=============== Windows 编译结果 ==============="
Say ("成功: {0}   失败: {1}   产物 exe: {2}" -f $script:OK, $script:FAIL, $nExe)
if ($script:FAIL -gt 0) { Say ("失败项: " + ($failed -join ' ')) }
if ($fallbacks.Count -gt 0) { Say ("回退到不链库的项 (" + $fallbacks.Count + "): " + ($fallbacks -join ' ')) }
Say "需第三方源码（fetch_third_party.sh + build_3rd.sh）: bench crossarch crossarch2 wallbench costbreak jzrun"
Say "产物目录: $Out"
Say "本日志: $Log"

$report | Set-Content -LiteralPath $Log -Encoding UTF8
if ($script:FAIL -gt 0) { exit 1 } else { exit 0 }
