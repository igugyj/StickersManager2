# Regression: preview window must close on middle-click / double-click
$ErrorActionPreference = "Stop"
Set-Location (Join-Path (Split-Path $PSScriptRoot -Parent) 'build')

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class M {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lp);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT p);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint PostMessage(IntPtr hWnd, uint msg, IntPtr w, IntPtr l);
  public struct RECT { public int Left, Top, Right, Bottom; }
  public struct POINT { public int X, Y; }
}
"@

$pass = @(); $fail = @()
function Check($n, $c) { if ($c) { $script:pass += $n } else { $script:fail += $n } }
function Ts { (Get-Date).ToString("HH:mm:ss.fff") }
function WinTitle([IntPtr]$h) { $sb = New-Object System.Text.StringBuilder 256; [void][M]::GetWindowText($h, $sb, 256); return $sb.ToString() }

# visible top-level windows of our pid (excluding main = titled window)
function AppWins([uint32]$procId) {
  $script:aw = @()
  $cb = [M+EnumWindowsProc] { param($h, $l)
    $tp = 0; [void][M]::GetWindowThreadProcessId($h, [ref]$tp)
    if ($tp -eq $procId -and [M]::IsWindowVisible($h)) { $script:aw += $h }
    return $true }
  [void][M]::EnumWindows($cb, [IntPtr]::Zero)
  return $script:aw
}
function Cabinets {
  $script:cl = @()
  $cb = [M+EnumWindowsProc] { param($h, $l)
    $cls = New-Object System.Text.StringBuilder 128; [void][M]::GetClassName($h, $cls, 128)
    if ($cls.ToString() -eq 'CabinetWClass' -and [M]::IsWindowVisible($h)) { $script:cl += $h }
    return $true }
  [void][M]::EnumWindows($cb, [IntPtr]::Zero)
  return $script:cl
}
function Rect([IntPtr]$h) { $r = New-Object M+RECT; [void][M]::GetWindowRect($h, [ref]$r); return $r }
function Hit($r, $x, $y) { return ($x -ge $r.Left -and $x -le $r.Right -and $y -ge $r.Top -and $y -le $r.Bottom) }
function ClickAt($x, $y, $down, $up) {
  [void][M]::SetCursorPos($x, $y); Start-Sleep -Milliseconds 250
  [M]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 70
  [M]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}

# log helpers: use counts (avoid timestamp-format ambiguity), strip ANSI
function CountPat($pat) {
  $n = 0
  foreach ($l in (Get-Content .\log\log.log -Encoding utf8)) {
    $c = $l -replace '\x1b\[[0-9;]*m', ''
    if ($c -match $pat) { $n++ }
  }
  return $n
}
function LastPat($pat) {
  $last = ''
  foreach ($l in (Get-Content .\log\log.log -Encoding utf8)) {
    $c = $l -replace '\x1b\[[0-9;]*m', ''
    if ($c -match $pat) { $last = $c }
  }
  return $last
}

# ---- start ----
Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process .\StickersManager.exe -WorkingDirectory (Get-Location) -RedirectStandardError "$env:TEMP\sm_pv_err.txt" -PassThru
$pid2 = $p.Id
"[$(Ts)] started pid=$pid2"
Start-Sleep -Seconds 10
$wsh = New-Object -ComObject WScript.Shell
$wsh.SendKeys('+^d')
$main = [IntPtr]::Zero
for ($i = 0; $i -lt 20 -and $main -eq [IntPtr]::Zero; $i++) {
  $script:found = [IntPtr]::Zero
  $cb = [M+EnumWindowsProc] { param($h, $l)
    $tp = 0; [void][M]::GetWindowThreadProcessId($h, [ref]$tp)
    if ($tp -eq $pid2 -and (WinTitle $h) -like '*ddemo*' -and [M]::IsWindowVisible($h)) { $script:found = $h; return $false }
    return $true }
  [void][M]::EnumWindows($cb, [IntPtr]::Zero)
  $main = $script:found
  if ($main -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 700 }
}
if ($main -eq [IntPtr]::Zero) { "FATAL: ddemo window not found"; exit 1 }
"[$(Ts)] main hwnd=$main"
[void][M]::SetForegroundWindow($main); Start-Sleep -Milliseconds 1000
$r = New-Object M+RECT; [void][M]::GetClientRect($main, [ref]$r)
$pt = New-Object M+POINT; $pt.X = 0; $pt.Y = 0; [void][M]::ClientToScreen($main, [ref]$pt)
$cx = $pt.X; $cy = $pt.Y
"client $($r.Right)x$($r.Bottom) at ($cx,$cy)"

$cands = @(@(300,220), @(170,220), @(430,220), @(300,100), @(300,340), @(170,100), @(430,100), @(170,340), @(430,340), @(460,220), @(300,160))

# ---- open preview by right-click ----
$win0 = AppWins $pid2
"[$(Ts)] app windows before right-click: " + $win0.Count
ClickAt ($cx + 300) ($cy + 220) 0x0008 0x0010   # RIGHTDOWN / RIGHTUP
Start-Sleep -Seconds 3
$win1 = AppWins $pid2
$newRaw = @($win1 | Where-Object { $_ -notin $win0 })
"[$(Ts)] app windows after right-click: " + $win1.Count + " new=" + $newRaw.Count
foreach ($h in $win1) { $q = Rect $h; "   win hwnd=$h title='" + (WinTitle $h) + "' rect=($($q.Left),$($q.Top))-($($q.Right),$($q.Bottom))" }
$newWins = @($newRaw | Where-Object { (WinTitle $_) -eq 'Stickers Manager' })  # preview dialog title = app name
if ($newWins.Count -lt 1) { $newWins = $newRaw }               # fallback: any brand-new window
Check "A0 right-click opens preview" ($newWins.Count -ge 1)
if ($newWins.Count -lt 1) { "abort: no preview"; Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force; exit 1 }
$preview = $newWins[0]
$pr = Rect $preview
$pcx = [int](($pr.Left + $pr.Right) / 2); $pcy = [int](($pr.Top + $pr.Bottom) / 2)
function IsPreviewLike([IntPtr]$h) {
  if (-not [M]::IsWindowVisible($h)) { return $false }
  $t = WinTitle $h
  if ($t -like '*- ddemo*') { return $false }                      # main window
  if ($t -ne '' -and $t -ne 'Stickers Manager') { return $false }  # preview dialog title = app name
  $q = Rect $h; if ($q.Right - $q.Left -lt 50) { return $false }   # skip 0-size helper windows
  $cx2 = [int](($q.Left + $q.Right) / 2); $cy2 = [int](($q.Top + $q.Bottom) / 2)
  return ([Math]::Abs($cx2 - $script:pcx) -le 300 -and [Math]::Abs($cy2 - $script:pcy) -le 300)  # centered dialog
}
"preview rect L=$($pr.Left) T=$($pr.Top) R=$($pr.Right) B=$($pr.Bottom) title='" + (WinTitle $preview) + "'"
"all app windows: " + ((AppWins $pid2 | ForEach-Object { "'" + (WinTitle $_) + "'" }) -join ', ')

# pick sticker cells NOT covered by the preview: A (first) and B (second, for follow test)
$pick = $null; $pick2 = $null
foreach ($c in $cands) {
  $x = $cx + $c[0]; $y = $cy + $c[1]
  if (-not (Hit $pr $x $y)) {
    if ($null -eq $pick) { $pick = @($c) }
    elseif ($null -eq $pick2) { $pick2 = @($c); break }
  }
}
if ($null -eq $pick) { "FATAL: every candidate cell is covered by preview"; Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force; exit 1 }
$px = $cx + $pick[0]; $py = $cy + $pick[1]
"picked cell A client ($($pick[0]),$($pick[1])) -> ($px,$py) (outside preview)"
if ($null -ne $pick2) { "picked cell B client ($($pick2[0]),$($pick2[1])) -> ($($cx+$pick2[0]),$($cy+$pick2[1])) (outside preview)" }

# ==== T0b: preview follows a left-click on another cell ====
if ($null -ne $pick2) {
  $bx = $cx + $pick2[0]; $by = $cy + $pick2[1]
  $pv0 = CountPat 'onPreviewFileChanged'
  ClickAt $bx $by 0x0002 0x0004   # LEFTDOWN / LEFTUP
  Start-Sleep -Seconds 3
  $pv1 = CountPat 'onPreviewFileChanged'
  $lastPv = LastPat 'onPreviewFileChanged'
  $lastHl = LastPat 'setHighlightedPath'
  $pClick = if ($lastHl -match 'setHighlightedPath: "([^"]+)"') { $matches[1] } else { '' }
  $pPv = if ($lastPv -match 'onPreviewFileChanged: "([^"]+)"') { $matches[1] } else { '' }
  $alive0 = ([M]::IsWindowVisible($preview)) -and ((AppWins $pid2) -contains $preview)
  Check "T0b left-click switches preview to that cell" (($pv1 -gt $pv0) -and $pClick -and ($pClick -eq $pPv))
  Check "T0b preview stays open after switching" ($alive0)
  "  previewSwitch $pv0->$pv1  cellPath='" + (Split-Path $pClick -Leaf) + "'  previewPath='" + (Split-Path $pPv -Leaf) + "'"
}

# ==== T1: middle-click while preview open => preview stays + explorer opens + highlight ====
$t = Ts
$hl0 = CountPat 'setHighlightedPath'
$cab0 = Cabinets
"[$(Ts)] cabinets baseline: " + $cab0.Count
[void][M]::SetForegroundWindow($main); Start-Sleep -Milliseconds 600
ClickAt $px $py 0x0020 0x0040   # MIDDLEDOWN / MIDDLEUP
"[$(Ts)] middle clicked"
Start-Sleep -Seconds 4
$previewAlive = ([M]::IsWindowVisible($preview)) -and ((AppWins $pid2) -contains $preview)
$cab1 = Cabinets
$newCab = @($cab1 | Where-Object { $_ -notin $cab0 })
$hl1 = CountPat 'setHighlightedPath'
$hlLast = LastPat 'setHighlightedPath'
$warn = CountPat '\[MiddleClick\]'
# 设计（用户改口）：中键也关预览（closePreview 必须在高亮之前，否则清高亮会覆盖）
Check "T1 preview closes on middle-click" (-not $previewAlive)
Check "T1 middle-click opens Explorer" ($newCab.Count -ge 1)
Check "T1 middle-click highlights cell (non-empty path)" (($hl1 -gt $hl0) -and ($hlLast -match 'setHighlightedPath: "[^"]+'))
Check "T1 no MiddleClick warning" ($warn -eq 0)
"  previewAlive=$previewAlive  newExplorer=$($newCab.Count)  hlLines=$hl0->$hl1  warn=$warn"
"  last: " + $hlLast.Trim()

# ==== T2: reopen preview, then double-click => main hides => preview closes via hideEvent ====
# 中键已把预览关掉 → 这里右键重新开一个
[void][M]::SetForegroundWindow($main); Start-Sleep -Milliseconds 800
$winA = AppWins $pid2
ClickAt $px $py 0x0008 0x0010
Start-Sleep -Seconds 3
$winB = AppWins $pid2
$new2 = @($winB | Where-Object { IsPreviewLike $_ })
Check "T2 setup: preview reopens after middle-click closed it" ($new2.Count -ge 1)
if ($new2.Count -ge 1) {
  $preview2 = $new2[0]
  $cop0 = CountPat 'Successfully copied file to clipboard'
  [void][M]::SetForegroundWindow($main); Start-Sleep -Milliseconds 600
  # double left click
  [void][M]::SetCursorPos($px, $py); Start-Sleep -Milliseconds 250
  [M]::mouse_event(0x0002,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [M]::mouse_event(0x0004,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [M]::mouse_event(0x0002,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [M]::mouse_event(0x0004,0,0,0,[UIntPtr]::Zero)
  "[$(Ts)] double-left clicked"
  Start-Sleep -Seconds 3
  $p2alive = ([M]::IsWindowVisible($preview2)) -and ((AppWins $pid2) -contains $preview2)
  $mainVis = [M]::IsWindowVisible($main)
  $cop1 = CountPat 'Successfully copied file to clipboard'
  Check "T2 preview closes (via hideEvent) on double-click" (-not $p2alive)
  Check "T2 main window hides on double-click" (-not $mainVis)
  Check "T2 double-click copies file" ($cop1 -gt $cop0)
  "  preview2Alive=$p2alive  mainVisible=$mainVis  copied=$cop0->$cop1"
}

# ==== T3: no preview open => left click does not open explorer ====
# main window was hidden by the double-click: toggle it back via its global hotkey
$wsh.SendKeys('+^d')
$main3 = [IntPtr]::Zero
for ($i = 0; $i -lt 20 -and $main3 -eq [IntPtr]::Zero; $i++) {
  $script:found = [IntPtr]::Zero
  $cb = [M+EnumWindowsProc] { param($h, $l)
    $tp = 0; [void][M]::GetWindowThreadProcessId($h, [ref]$tp)
    if ($tp -eq $pid2 -and (WinTitle $h) -like '*ddemo*' -and [M]::IsWindowVisible($h)) { $script:found = $h; return $false }
    return $true }
  [void][M]::EnumWindows($cb, [IntPtr]::Zero)
  $main3 = $script:found
  if ($main3 -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 700 }
}
Check "T3 main window restores via hotkey" ($main3 -ne [IntPtr]::Zero)
if ($main3 -ne [IntPtr]::Zero) {
  $main = $main3
  [void][M]::SetForegroundWindow($main); Start-Sleep -Milliseconds 800
  $c2 = (Cabinets).Count
  ClickAt $px $py 0x0002 0x0004
  Start-Sleep -Seconds 2
  Check "T3 left-click does not open Explorer" ((Cabinets).Count -eq $c2)
  Check "T3 no preview window appears" (@(AppWins $pid2 | Where-Object { IsPreviewLike $_ }).Count -eq 0)
}

# ---- cleanup ----
foreach ($h in $newCab) { [void][M]::PostMessage($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }  # WM_CLOSE
Start-Sleep -Milliseconds 800
Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force

""
"=== PASS ($($pass.Count)) ==="; $pass | ForEach-Object { "  [ok] $_" }
if ($fail.Count) { "=== FAIL ($($fail.Count)) ==="; $fail | ForEach-Object { "  [X] $_" } } else { "=== FAIL: 0 ===" }
"leftover cabinets: " + (Cabinets).Count
"leftover app procs: " + (Get-Process StickersManager -EA SilentlyContinue | Measure-Object).Count
if ($fail.Count) { exit 1 }
