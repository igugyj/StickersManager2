# TrayIcon 模块

## 职责

系统托盘常驻图标：菜单（Rescan / Settings / Show 库子菜单）、通知（notifyTray 跨线程）、双击激活分发。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/ui/tray.h/.cpp` | `TrayIcon` 单例、showSubMenu、action_rescan/action_settings、activated 信号 |
| `src/utils/launcher.hpp` | `launch()`：QtConcurrent::run + QDesktopServices 打开路径/URL |
| `src/utils/log.hpp` | 日志、messageHandler |

## 数据流

```
托盘 Show 子菜单 triggered → 库 path → showWindow()/hide()
双击 activated(DoubleClick) → getDoubleClickTarget() → settings / first-library / id / dirName（main.cpp:225-272）
worker 线程 → notifyTray()（QueuedConnection）→ 托盘消息
```

## 依赖关系

- 依赖：main.cpp（连接信号）、launcher.hpp
- 被依赖：无（单例被 main 驱动）

## 关键规则

- `updateShowMenu(libs)` 由 createWindows / applyChanges / fullReload 调用刷新
- 库子菜单 action data 存库 path
- 通知从工作线程必须经 `notifyTray()` 转 GUI 线程

## 变更记录索引

- 2025-01-14：docs/.ai Init（首次记录）
