# ============================================================
# test_reload_path.ps1 — 相邻 bug 验收：loadLibrary 失效路径 = update 而非 append
# 运行：powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\test_reload_path.ps1
# 路径按脚本位置自动推导（tests/ 的上一级 = 仓库根），无需修改即可运行
#
#   P1 配置里把 EMO 路径改成不存在的目录 → 启动弹选目录框 → 选回原 EMO 路径
#      断言：config 仍是 2 条（旧死路径被原地替换，不是追加）、ids [0,2]；
#            Ctrl+Shift+E 能开出 "Stickers Manager - emo" 窗口（键/标题取实际路径）
#   P2 重启 → 断言不再弹选目录框（旧 bug：死路径残留每次启动都弹）
#   P3 再把路径改死 → 启动弹框 → 选 ddemo（已有库）→ 断言弹
#      "Library Already Exists"、config 未被改动
# 结束还原备份
# ============================================================
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$build    = Join-Path $repoRoot 'build'
$tmpU     = ($env:TEMP -replace '\\', '/')
$dead1    = "$tmpU/sm_tests/libdead1"
$dead2    = "$tmpU/sm_tests/libdead2"
$cfgPath  = Join-Path $build '.stickersmanager\config.json'
$setPath  = Join-Path $build '.stickersmanager\settings.json'
$cfgBak   = $cfgPath + '.rlbak'
$setBak   = $setPath + '.rlbak'
$emo      = 'D:/Users/user/Pictures/EMO/emo'
$ddemo    = 'D:/Users/user/Pictures/ddemo'

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public class U {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public struct RECT { public int L, T, R, B; }
  public static string T(IntPtr h){ var sb=new StringBuilder(1024); GetWindowTextW(h,sb,1024); return sb.ToString(); }
  public static string C(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static RECT R(IntPtr h){ RECT r; GetWindowRect(h,out r); return r; }
  public static List<IntPtr> Pid(uint pid){ var l=new List<IntPtr>(); EnumWindows(delegate(IntPtr h, IntPtr p){ uint q; GetWindowThreadProcessId(h,out q); if(q==pid) l.Add(h); return true;},IntPtr.Zero); return l; }
  public static uint PidOf(IntPtr h){ uint q; GetWindowThreadProcessId(h,out q); return q; }
  public static IntPtr RootAt(int x, int y){ POINT p; p.X=x; p.Y=y; IntPtr h = WindowFromPoint(p); return GetAncestor(h, 2); }
}
"@
Add-Type -AssemblyName UIAutomationClient

$pass = 0; $fail = 0
function Check($name, $cond, $detail='') {
  if ($cond) { $script:pass++; "  [ok]   $name" + $(if ($detail) { "  | $detail" } else { ""}) }
  else       { $script:fail++; "  [FAIL] $name" + $(if ($detail) { "  | $detail" } else { ""}) }
}
function Norm($s) { if ($null -eq $s) { return '' }; ($s -replace '\\','/').TrimEnd('/').ToLower() }

function WaitWin($pid2, $like, $sec) {
  $deadline = (Get-Date).AddSeconds($sec)
  while ((Get-Date) -lt $deadline) {
    foreach ($h in [U]::Pid([uint32]$pid2)) {
      if ([U]::T($h) -like $like -and [U]::IsWindowVisible($h)) { return $h }
    }
    Start-Sleep -Milliseconds 200
  }
  return [IntPtr]::Zero
}
function WaitGone($pid2, $like, $sec) {
  $deadline = (Get-Date).AddSeconds($sec)
  while ((Get-Date) -lt $deadline) {
    if ((WaitWin $pid2 $like 0.3) -eq [IntPtr]::Zero) { return $true }
    Start-Sleep -Milliseconds 200
  }
  return $false
}

function ClickXY($pid2, $x, $y) {
  $root = [U]::RootAt($x, $y)
  $owner = if ($root -ne [IntPtr]::Zero) { [U]::PidOf($root) } else { 0 }
  if ($owner -ne $pid2) {
    "  !! click ($x,$y) would hit pid=$owner ('$([U]::T($root))') — ABORT click"
    return $false
  }
  [void][U]::SetCursorPos($x, $y); Start-Sleep -Milliseconds 150
  [U]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [U]::mouse_event(4,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 400
  return $true
}
function ClickEl($pid2, $el, $fg) {
  if ($fg -ne [IntPtr]::Zero) { [void][U]::SetForegroundWindow($fg); Start-Sleep -Milliseconds 350 }
  $r = $el.Current.BoundingRectangle
  if ($r.Width -le 0) { "  !! element has no rect: '$($el.Current.Name)'"; return $false }
  $x = [int]($r.X + $r.Width/2); $y = [int]($r.Y + $r.Height/2)
  $hit = [System.Windows.Automation.AutomationElement]::FromPoint((New-Object System.Windows.Point($x, $y)))
  $got = if ($hit) { $hit.Current.Name + ' (' + $hit.Current.ControlType.ProgrammaticName + ')' } else { '<null>' }
  "  click '$($el.Current.Name)' at ($x,$y) -> hits $got"
  return (ClickXY $pid2 $x $y)
}
function SendKey($vk) {
  [U]::keybd_event($vk,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 50
  [U]::keybd_event($vk,0,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 150
}
# hold modifiers, tap key, release (LL hook reads modifier state on key release)
function SendHotkey([int[]]$mods, [int]$key) {
  foreach ($m in $mods) { [U]::keybd_event([byte]$m,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 50 }
  [U]::keybd_event([byte]$key,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [U]::keybd_event([byte]$key,0,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80
  for ($i = $mods.Count - 1; $i -ge 0; $i--) { [U]::keybd_event([byte]$mods[$i],0,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 40 }
  Start-Sleep -Milliseconds 300
}

function UiaRoot($hwnd) { [System.Windows.Automation.AutomationElement]::FromHandle($hwnd) }
function UiaAll($root) {
  return ,$root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
}
function UiaFind($root, $ct, $nameLike) {
  foreach ($e in (UiaAll $root)) {
    if ($ct -and $e.Current.ControlType.ProgrammaticName -ne $ct) { continue }
    if ($nameLike -and $e.Current.Name -notlike $nameLike) { continue }
    return $e
  }
  return $null
}
function GetVal($e) {
  $p = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
  if ($p) { return $p.Current.Value }
  return $null
}
function SetVal($e, $text) {
  $p = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
  if (-not $p) { return $null }
  try { $p.SetValue($text); Start-Sleep -Milliseconds 300; return (GetVal $e) } catch { return $null }
}

# Qt 非原生目录对话框：fileNameEdit 填完整路径 + Enter（returnPressed -> accept() -> typedFiles 原样取值）
function SetFileDialogPath($pid2, $fdHwnd, $path) {
  $root = UiaRoot $fdHwnd
  $edits = @()
  foreach ($e in (UiaAll $root)) {
    if ($e.Current.ControlType.ProgrammaticName -eq 'ControlType.Edit') { $edits += $e }
  }
  if ($edits.Count -eq 0) { "  !! file dialog has no Edit control"; return $false }
  $edit = $edits | Sort-Object { $_.Current.BoundingRectangle.Y } -Descending | Select-Object -First 1
  if (-not (ClickEl $pid2 $edit $fdHwnd)) { return $false }
  $v = SetVal $edit $path
  if (-not $v -or (Norm $v) -ne (Norm $path)) { "  !! ValuePattern set failed (got '$v')"; return $false }
  "  edit value ok: $v"
  [void][U]::SetForegroundWindow($fdHwnd); Start-Sleep -Milliseconds 250
  SendKey 0x0D
  if (WaitGone $pid2 '*Select Stickers Library*' 8) { return $true }
  $open = UiaFind (UiaRoot $fdHwnd) 'ControlType.Button' '*Open*'
  if ($open) {
    [void](ClickEl $pid2 $open $fdHwnd)
    if (WaitGone $pid2 '*Select Stickers Library*' 6) { return $true }
  }
  return $false
}

function DismissMsgBox($pid2, $titleLike) {
  $mb = WaitWin $pid2 $titleLike 10
  if ($mb -eq [IntPtr]::Zero) { return $false }
  $ok = UiaFind (UiaRoot $mb) 'ControlType.Button' 'OK'
  if ($ok) { [void](ClickEl $pid2 $ok $mb) }
  else { [void][U]::SetForegroundWindow($mb); Start-Sleep -Milliseconds 300; SendKey 0x0D }
  return (WaitGone $pid2 $titleLike 6)
}

function Start-App {
  Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force
  Start-Sleep -Seconds 1
  $p = Start-Process -FilePath (Join-Path $build 'StickersManager.exe') -WorkingDirectory $build -PassThru
  return $p.Id
}
# rewrite one library's path in config.json (app must be stopped); no-BOM UTF-8 for QJsonDocument
function Set-CfgPath($from, $to) {
  $c = Get-Content $cfgPath -Raw -Encoding UTF8 | ConvertFrom-Json
  $hit = $false
  foreach ($l in $c.libraries) { if ((Norm $l.path) -eq (Norm $from)) { $l.path = $to; $hit = $true } }
  if (-not $hit) { throw "Set-CfgPath: '$from' not found in config" }
  $utf8 = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($cfgPath, ($c | ConvertTo-Json -Depth 100), $utf8)
}
function Read-Cfg { Get-Content $cfgPath -Raw -Encoding UTF8 | ConvertFrom-Json }

# ============================== main ==============================
"[$(Get-Date -f HH:mm:ss)] test_reload_path start"
Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Copy-Item $cfgPath $cfgBak -Force
Copy-Item $setPath $setBak -Force
"backed up config.json + settings.json"

try {
  # ================= P1: dead path -> pick the old folder again =================
  "`n== P1: dead path prompts; picking the old folder UPDATES in place =="
  Set-CfgPath $emo $dead1
  $pid2 = Start-App
  "  started pid=$pid2 with dead path $dead1"
  $fd = WaitWin $pid2 '*Select Stickers Library*' 15
  Check 'folder picker appears on launch (dead path)' ($fd -ne [IntPtr]::Zero)
  if ($fd -eq [IntPtr]::Zero) { throw 'no folder picker' }
  $ok = SetFileDialogPath $pid2 $fd $emo
  Check 'picked the original EMO folder' $ok
  Start-Sleep -Seconds 4
  Check 'no picker left open' ((WaitWin $pid2 '*Select Stickers Library*' 1) -eq [IntPtr]::Zero)

  $cfg = Read-Cfg
  $libs = @($cfg.libraries)
  $ids = @($libs | ForEach-Object { $_.id })
  $paths = @($libs | ForEach-Object { $_.path })
  "  config: count=$($libs.Count) ids=[$($ids -join ',')] paths=[$($paths -join ' | ')]"
  Check 'config still has 2 libraries (update, NOT append)' ($libs.Count -eq 2) "count=$($libs.Count)"
  Check 'ids unchanged [0,2]' (($ids -join ',') -eq '0,2') ($ids -join ',')
  Check 'dead path replaced by EMO in place' ((Norm $libs[0].path) -eq (Norm $emo)) $libs[0].path
  Check 'ddemo untouched' ((@($paths | Where-Object { (Norm $_) -eq (Norm $ddemo) }).Count) -eq 1)
  Check 'no dead path left in config' (@($paths | Where-Object { (Norm $_) -like '*libdead*' }).Count -eq 0)

  # window key/title must follow the REPLACED path -> hotkey Ctrl+Shift+E shows 'Stickers Manager - emo'
  SendHotkey @(0x11, 0x10) 0x45   # Ctrl+Shift+E
  $w = WaitWin $pid2 'Stickers Manager - *' 10
  Check 'EMO window shows via hotkey after path swap' ($w -ne [IntPtr]::Zero)
  if ($w -ne [IntPtr]::Zero) {
    $t = [U]::T($w)
    "  window title: '$t'"
    Check "window title is 'Stickers Manager - emo'" ($t -eq 'Stickers Manager - emo') $t
    Check 'title does not contain the dead path name' ($t -notlike '*libdead*')
  }

  # ================= P2: relaunch -> NO prompt again =================
  "`n== P2: relaunch must NOT prompt (old bug: stale dead entry) =="
  $pid2 = Start-App
  "  started pid=$pid2"
  Start-Sleep -Seconds 3
  $fd = WaitWin $pid2 '*Select Stickers Library*' 8
  Check 'no folder picker on relaunch' ($fd -eq [IntPtr]::Zero)
  SendHotkey @(0x11, 0x10) 0x45
  $w = WaitWin $pid2 'Stickers Manager - *' 10
  Check 'EMO window works on relaunch' ($w -ne [IntPtr]::Zero)
  if ($w -ne [IntPtr]::Zero) { Check "relaunch title 'Stickers Manager - emo'" (([U]::T($w)) -eq 'Stickers Manager - emo') ([U]::T($w)) }

  # ================= P3: picking an EXISTING library is refused =================
  "`n== P3: picker -> ddemo (existing) must refuse with a message =="
  Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force
  Start-Sleep -Seconds 1
  Set-CfgPath $emo $dead2
  $snap = Get-Content $cfgPath -Raw -Encoding UTF8
  $pid2 = Start-App
  "  started pid=$pid2 with dead path $dead2"
  $fd = WaitWin $pid2 '*Select Stickers Library*' 15
  Check 'folder picker appears again (dead path)' ($fd -ne [IntPtr]::Zero)
  if ($fd -ne [IntPtr]::Zero) {
    $ok = SetFileDialogPath $pid2 $fd $ddemo
    Check 'picked ddemo (already a library)' $ok
  }
  $mb = WaitWin $pid2 '*Library Already Exists*' 10
  Check '"Library Already Exists" shown' ($mb -ne [IntPtr]::Zero)
  if ($mb -ne [IntPtr]::Zero) { Check 'message box dismissed' (DismissMsgBox $pid2 '*Library Already Exists*') }
  Start-Sleep -Milliseconds 800
  Check 'config untouched by refused pick (still dead2 entry)' ((Get-Content $cfgPath -Raw -Encoding UTF8) -eq $snap)
} catch {
  "  EXCEPTION: $($_.Exception.Message)"
  $script:fail++
} finally {
  "`n== teardown =="
  Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force
  Start-Sleep -Seconds 1
  Copy-Item $cfgBak $cfgPath -Force
  Copy-Item $setBak $setPath -Force
  Remove-Item $cfgBak, $setBak -Force -EA SilentlyContinue
  "restored config.json + settings.json"
  "`n=== PASS ($pass) / FAIL ($fail) ==="
  if ($fail -gt 0) { exit 1 } else { exit 0 }
}
