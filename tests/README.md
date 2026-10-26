# StickersManager 手动验收脚本

三个 PowerShell + UIA（UI Automation）端到端脚本，驱动真实 exe 做点击/键盘级验收。均**自动备份并还原** `build/.stickersmanager/config.json` 与 `settings.json`，结束后不留残留（进程、临时目录、备份文件都会清理）。

## 运行要求

- **Windows**，`powershell.exe` 5.1（不能用 pwsh 跑 `test_addlib` / `test_reload_path`，它们引用 UIAutomationClient/System.Windows.Forms 程序集；`test_preview` 用 pwsh 亦可）
- 先构建：`cd build && cmake --build . -- -j8`（`StickersManager.exe` 存在且为最新）
- 运行前**手动关闭已开的 StickersManager**（脚本开头会强杀同名进程）
- 屏幕需处于交互状态（脚本会真实移动光标、点击、发按键；所有点击先校验落点属于本进程，防误点）

## 用法（仓库根目录执行）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\test_preview.ps1        # 预览/中键回归，预期 PASS (14)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\test_addlib.ps1         # TODO ③ 三层修复，预期 PASS (29)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\test_reload_path.ps1    # 失效路径 update-not-append，预期 PASS (19)
```

路径全部按脚本位置推导（`tests/` 上一级 = 仓库根，构建目录 = `<仓库根>/build`），临时目录用 `$env:TEMP\sm_tests\`，换机器/换目录无需改动。

## 脚本说明

| 脚本 | 验收内容 | 断言数 |
|---|---|---|
| `test_preview.ps1` | 右键开预览、左键切换预览、中键关预览并资源管理器选中、隐藏后预览关闭 | 14 |
| `test_addlib.ps1` | A 新增库卡片显示所选路径 + 落盘 id 升序/末位/唯一；B 重复添加弹 `Library Already Exists` 不落盘；C 卡片改已有路径 Save 弹 `Duplicate Library Path` 拦截保存 | 29 |
| `test_reload_path.ps1` | P1 死路径启动弹框→选回原文件夹**原地替换**（不追加）+ 热键开窗标题正确；P2 重启不再弹框；P3 选已有库被拒且 config 不动 | 19 |

## 技术备注

- 文件夹选择框是 Qt 非原生 `QFileDialog`：脚本用 UIA `ValuePattern.SetValue` 直填完整路径再发 Enter（绕开中文 IME），对应 Qt 链路 `lineEdit returnPressed → accept() → typedFiles`
- Qt 控件无原生子 HWND，全靠 UIA 树定位；消息框按顶层窗口标题匹配
- 写回 `config.json` 用**无 BOM UTF-8**（QJsonDocument 要求）+ `ConvertTo-Json -Depth 100`（PS 5.1 默认深度只有 2，库的 `settings` 会丢）
- 修改验收代码后记得 `cd build && cmake --build . -- -j8` 再跑
