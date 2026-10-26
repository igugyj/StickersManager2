# ============================================================
# test_addlib.ps1 — TODO ③ 验收：添加仓库时索引到已有仓库路径
# 运行：powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\test_addlib.ps1
# 需要 powershell.exe 5.1（UIAutomationClient/System.Windows.Forms 程序集）
# 路径按脚本位置自动推导（tests/ 的上一级 = 仓库根），无需修改即可运行
#
# 流程：
#   A 新增库 → 断言卡片显示新路径 + 落盘 id=[0,2,3]、路径唯一、新库在末位
#   B 再次添加同一路径 → 断言弹 "Library Already Exists"、不落盘
#   C 把新卡片路径改成已有库路径 → Save 断言弹 "Duplicate Library Path"、不落盘
# 结束：还原 config.json / settings.json 备份
# ============================================================
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$build   = Join-Path $repoRoot 'build'
$target   = Join-Path $env:TEMP 'sm_tests\libtest3'
$targetU  = ($target -replace '\\', '/')
$emo      = 'D:/Users/user/Pictures/EMO/emo'
$ddemo    = 'D:/Users/user/Pictures/ddemo'
$cfgPath  = Join-Path $build '.stickersmanager\config.json'
$setPath  = Join-Path $build '.stickersmanager\settings.json'
$cfgBak   = $cfgPath + '.test3bak'
$setBak   = $setPath + '.test3bak'

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
  public static IntPtr RootAt(int x, int y){ POINT p; p.X=x; p.Y=y; IntPtr h = WindowFromPoint(p); return GetAncestor(h, 2); } // GA_ROOT
}
"@
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName System.Windows.Forms

$pass = 0; $fail = 0
function Check($name, $cond, $detail='') {
  if ($cond) { $script:pass++; "  [ok]   $name" + $(if ($detail) { "  | $detail" } else { ""}) }
  else       { $script:fail++; "  [FAIL] $name" + $(if ($detail) { "  | $detail" } else { ""}) }
}
function Norm($s) { if ($null -eq $s) { return '' }; ($s -replace '\\','/').TrimEnd('/').ToLower() }

# ---- wait for a top-level window of our pid whose title matches; returns hwnd or [IntPtr]::Zero
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
    $h = WaitWin $pid2 $like 0.3
    if ($h -eq [IntPtr]::Zero) { return $true }
    Start-Sleep -Milliseconds 200
  }
  return $false
}

# ---- mouse: click only if the point really belongs to our process (safety net) ----
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
  # what does UIA think sits at the click point?
  $hit = [System.Windows.Automation.AutomationElement]::FromPoint((New-Object System.Windows.Point($x, $y)))
  $got = if ($hit) { $hit.Current.Name + ' (' + $hit.Current.ControlType.ProgrammaticName + ')' } else { '<null>' }
  "  click '$($el.Current.Name)' at ($x,$y) -> hits $got"
  return (ClickXY $pid2 $x $y)
}
function SendKey($vk) {
  [U]::keybd_event($vk,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 50
  [U]::keybd_event($vk,0,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 150
}
function SendCtrlA {
  [U]::keybd_event(0x11,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 40
  SendKey 0x41
  [U]::keybd_event(0x11,0,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80
}

# ---- UIA ----
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
function DumpUia($root, $tag) {
  "  --- UIA dump: $tag ---"
  foreach ($e in (UiaAll $root)) {
    $r = $e.Current.BoundingRectangle
    $v = ''
    try { $v = (GetVal $e) } catch {}
    "    {0,-24} name='{1}' val='{2}' rect=({3:N0},{4:N0}) {5:N0}x{6:N0} off={7}" -f `
      $e.Current.ControlType.ProgrammaticName, $e.Current.Name, $v, $r.X, $r.Y, $r.Width, $r.Height, $e.Current.IsOffscreen
  }
}
# path-edit values currently exposed by the Settings dialog
function GetPathEditValues($settingsHwnd) {
  $root = UiaRoot $settingsHwnd
  $vals = @()
  foreach ($e in (UiaAll $root)) {
    if ($e.Current.ControlType.ProgrammaticName -ne 'ControlType.Edit') { continue }
    $v = GetVal $e
    if ($v -and $v -match '^[A-Za-z]:') { $vals += $v }
  }
  return ,$vals
}

function SendCtrlI {
  [U]::keybd_event(0x11,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  SendKey 0x49
  [U]::keybd_event(0x11,0,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 150
}

# Drive the Qt non-native directory dialog: set fileNameEdit to a full path, Enter.
# Qt semantics (qfiledialog.cpp): lineEdit returnPressed -> accept(); typed path (or the
# list selection that autoCompleteFileName sets from it) is an existing dir -> dialog accepts it.
function SetFileDialogPath($pid2, $fdHwnd, $path) {
  $root = UiaRoot $fdHwnd
  $edits = @()
  foreach ($e in (UiaAll $root)) {
    if ($e.Current.ControlType.ProgrammaticName -eq 'ControlType.Edit') { $edits += $e }
  }
  if ($edits.Count -eq 0) { "  !! file dialog has no Edit control"; DumpUia $root 'file dialog'; return $false }
  # file name edit = bottom-most edit (row above the Open/Cancel buttons)
  $edit = $edits | Sort-Object { $_.Current.BoundingRectangle.Y } -Descending | Select-Object -First 1
  "  file dialog edits=$($edits.Count), using name='$($edit.Current.Name)'"
  if (-not (ClickEl $pid2 $edit $fdHwnd)) { return $false }   # focus it
  $v = SetVal $edit $path
  if (-not $v -or (Norm $v) -ne (Norm $path)) {
    "  ValuePattern set failed (got '$v') — SendKeys fallback"
    [void][U]::SetForegroundWindow($fdHwnd); Start-Sleep -Milliseconds 300
    SendCtrlA
    [System.Windows.Forms.SendKeys]::SendWait($path)
    Start-Sleep -Milliseconds 500
    $v = GetVal $edit
  }
  if ((Norm $v) -ne (Norm $path)) { "  !! edit value is '$v', expected '$path'"; return $false }
  "  edit value ok: $v"
  [void][U]::SetForegroundWindow($fdHwnd); Start-Sleep -Milliseconds 250
  SendKey 0x0D   # Enter -> returnPressed -> accept()
  if (WaitGone $pid2 '*Select Sticker Library Folder*' 8) { return $true }
  "  Enter did not accept — trying Open button"
  $root2 = UiaRoot $fdHwnd
  $open = UiaFind $root2 'ControlType.Button' '*Open*'
  if (-not $open) { $open = UiaFind $root2 'ControlType.Button' '*OK*' }
  if ($open) {
    [void](ClickEl $pid2 $open $fdHwnd)
    if (WaitGone $pid2 '*Select Sticker Library Folder*' 6) { return $true }
  }
  DumpUia (UiaRoot $fdHwnd) 'file dialog still open'
  return $false
}

function DismissMsgBox($pid2, $titleLike) {
  $mb = WaitWin $pid2 $titleLike 10
  if ($mb -eq [IntPtr]::Zero) { return $false }
  $root = UiaRoot $mb
  $ok = UiaFind $root 'ControlType.Button' 'OK'
  if (-not $ok) { $ok = UiaFind $root 'ControlType.Button' '&' }
  if ($ok) { [void](ClickEl $pid2 $ok $mb) } else {
    [void][U]::SetForegroundWindow($mb); Start-Sleep -Milliseconds 300; SendKey 0x0D
  }
  return (WaitGone $pid2 $titleLike 6)
}

# ============================== main ==============================
"[$(Get-Date -f HH:mm:ss)] test_addlib start"
Get-Process StickersManager -EA SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Copy-Item $cfgPath $cfgBak -Force
Copy-Item $setPath $setBak -Force
"backed up config.json + settings.json"
New-Item -ItemType Directory -Path $target -Force | Out-Null

$p = Start-Process -FilePath (Join-Path $build 'StickersManager.exe') -WorkingDirectory $build -PassThru
$pid2 = $p.Id
"started pid=$pid2"
Start-Sleep -Seconds 8

try {
  # ---- open Settings ----
  SendCtrlI
  $settings = WaitWin $pid2 'Settings' 12
  Check 'Settings dialog opens on Ctrl+I' ($settings -ne [IntPtr]::Zero)
  if ($settings -eq [IntPtr]::Zero) { throw 'no Settings dialog' }
  $srect = [U]::R($settings)
  "  settings hwnd=$settings rect=($($srect.L),$($srect.T))-($($srect.R),$($srect.B))"

  $sroot = UiaRoot $settings
  # DumpUia $sroot 'settings (General tab)'

  # ---- switch to Libraries tab ----
  $tab = UiaFind $sroot 'ControlType.TabItem' 'Libraries'
  Check 'Libraries TabItem found' ($null -ne $tab)
  if ($null -eq $tab) { throw 'no Libraries tab' }
  [void](ClickEl $pid2 $tab $settings)
  Start-Sleep -Milliseconds 800
  $addBtn = UiaFind (UiaRoot $settings) 'ControlType.Button' '*Add Library*'
  Check '"+ Add Library" button visible after tab switch' ($null -ne $addBtn)
  if ($null -eq $addBtn) { $addBtn = UiaFind (UiaRoot $settings) 'ControlType.Button' '*Add*' ; Check '  (fallback Add button)' ($null -ne $addBtn) }

  $baselineRaw = Get-Content $cfgPath -Raw
  $baseline = $baselineRaw | ConvertFrom-Json
  "  baseline: $((@($baseline.libraries)).Count) libraries"

  # ================= A: add a NEW library =================
  "`n== A: add new library =="
  [void](ClickEl $pid2 $addBtn $settings)
  $fd = WaitWin $pid2 '*Select Sticker Library Folder*' 12
  Check 'file dialog opens' ($fd -ne [IntPtr]::Zero)
  if ($fd -eq [IntPtr]::Zero) { throw 'no file dialog' }
  $ok = SetFileDialogPath $pid2 $fd $targetU
  Check 'file dialog accepted target path' $ok
  Check 'file dialog closed' (WaitGone $pid2 '*Select Sticker Library Folder*' 6)
  Start-Sleep -Milliseconds 800

  # ---- card assertions (the bug symptom: card must show the NEW path) ----
  $vals = GetPathEditValues $settings
  $nset = @($vals | ForEach-Object { Norm $_ } | Sort-Object -Unique)
  "  path edits on cards: " + (($vals | ForEach-Object { "'$_'" }) -join ', ')
  Check '3 path edits visible' ($vals.Count -eq 3) "count=$($vals.Count)"
  Check 'card shows the NEW target path' ($nset -contains (Norm $targetU)) ($nset -join ' ; ')
  Check 'existing EMO path still intact' ($nset -contains (Norm $emo))
  Check 'existing ddemo path still intact' ($nset -contains (Norm $ddemo))

  # ---- save ----
  $saveBtn = UiaFind (UiaRoot $settings) 'ControlType.Button' '*Save*Reload*'
  Check 'Save && Reload button found' ($null -ne $saveBtn)
  $mt0 = (Get-Item $cfgPath).LastWriteTime.ToString('HH:mm:ss.fff')
  $ms0 = (Get-Item $setPath).LastWriteTime.ToString('HH:mm:ss.fff')
  "  mtimes before save: cfg=$mt0 settings=$ms0"
  [void](ClickEl $pid2 $saveBtn $settings)
  for ($i = 1; $i -le 8; $i++) {
    Start-Sleep -Milliseconds 500
    try { $c = Get-Content $cfgPath -Raw | ConvertFrom-Json; $n = (@($c.libraries)).Count } catch { $n = 'ERR' }
    $mt = (Get-Item $cfgPath).LastWriteTime.ToString('HH:mm:ss.fff')
    $ms = (Get-Item $setPath).LastWriteTime.ToString('HH:mm:ss.fff')
    "    t+$([math]::Round($i*0.5,1))s: config libs=$n cfgmtime=$mt setmtime=$ms"
  }
  "  top-level windows after save:"
  foreach ($h in [U]::Pid([uint32]$pid2)) { "    '$([U]::T($h))'" }
  Check 'no warning box during valid save' ((WaitWin $pid2 '*Duplicate*' 0.5) -eq [IntPtr]::Zero)

  $raw = Get-Content $cfgPath -Raw
  $cfg = $raw | ConvertFrom-Json
  $libs = @($cfg.libraries)
  $paths = @($libs | ForEach-Object { $_.path })
  $ids   = @($libs | ForEach-Object { $_.id })
  $cpaths = @($paths | ForEach-Object { Norm $_ } | Sort-Object -Unique)
  "  saved: count=$($libs.Count) ids=[$($ids -join ',')] paths=[$($paths -join ' | ')]"
  Check 'config has 3 libraries' ($libs.Count -eq 3)
  Check 'ids stay unique & ascending' (($ids -join ',') -eq '0,2,3') "ids=$($ids -join ',')"
  Check 'new library is LAST in array' ((Norm $libs[2].path) -eq (Norm $targetU)) $libs[2].path
  Check 'no duplicate paths after save' ($cpaths.Count -eq 3) ($cpaths -join ' ; ')
  $savedRaw = $raw

  # ================= B: add the SAME library again =================
  "`n== B: duplicate add must be rejected =="
  $addBtn = UiaFind (UiaRoot $settings) 'ControlType.Button' '*Add Library*'
  [void](ClickEl $pid2 $addBtn $settings)
  $fd = WaitWin $pid2 '*Select Sticker Library Folder*' 12
  Check 'file dialog opens (2nd time)' ($fd -ne [IntPtr]::Zero)
  if ($fd -ne [IntPtr]::Zero) {
    $ok = SetFileDialogPath $pid2 $fd $targetU
    Check 'file dialog accepted target path (2nd time)' $ok
    Check 'file dialog closed (2nd time)' (WaitGone $pid2 '*Select Sticker Library Folder*' 6)
  }
  $mb = WaitWin $pid2 '*Library Already Exists*' 10
  Check '"Library Already Exists" message box shown' ($mb -ne [IntPtr]::Zero)
  if ($mb -ne [IntPtr]::Zero) { Check 'message box dismissed' (DismissMsgBox $pid2 '*Library Already Exists*') }
  Start-Sleep -Milliseconds 600
  $valsB = GetPathEditValues $settings
  Check 'still 3 cards (duplicate not added)' ($valsB.Count -eq 3) "count=$($valsB.Count)"
  Check 'config.json untouched by duplicate attempt' ((Get-Content $cfgPath -Raw) -eq $savedRaw)

  # ================= C: card edited to an existing path =================
  "`n== C: duplicate path typed into a card must block save =="
  $targetEdit = $null
  foreach ($e in (UiaAll (UiaRoot $settings))) {
    if ($e.Current.ControlType.ProgrammaticName -ne 'ControlType.Edit') { continue }
    $v = GetVal $e
    if ($v -and (Norm $v) -eq (Norm $targetU)) { $targetEdit = $e; break }
  }
  Check 'found the new card path edit' ($null -ne $targetEdit)
  if ($null -ne $targetEdit) {
    $v = SetVal $targetEdit $emo
    Check 'card path set to existing EMO path' ($v -and ((Norm $v) -eq (Norm $emo))) "got '$v'"
    $saveBtn = UiaFind (UiaRoot $settings) 'ControlType.Button' '*Save*Reload*'
    [void](ClickEl $pid2 $saveBtn $settings)
    $mb = WaitWin $pid2 '*Duplicate Library Path*' 10
    Check '"Duplicate Library Path" warning shown' ($mb -ne [IntPtr]::Zero)
    if ($mb -ne [IntPtr]::Zero) { Check 'warning dismissed' (DismissMsgBox $pid2 '*Duplicate Library Path*') }
    Start-Sleep -Milliseconds 600
    Check 'save was blocked (config.json untouched)' ((Get-Content $cfgPath -Raw) -eq $savedRaw)
  }

  # ---- close settings ----
  $cancel = UiaFind (UiaRoot $settings) 'ControlType.Button' 'Cancel'
  if ($cancel) { [void](ClickEl $pid2 $cancel $settings); Check 'Settings closed' (WaitGone $pid2 'Settings' 6) }
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
  Remove-Item $target -Recurse -Force -EA SilentlyContinue
  "restored config.json + settings.json, removed $target"
  "`n=== PASS ($pass) / FAIL ($fail) ==="
  if ($fail -gt 0) { exit 1 } else { exit 0 }
}
