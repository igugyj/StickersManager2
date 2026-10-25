# 全局热键模块

## 职责

注册全局热键（Win32 低级键盘钩子），把按键事件映射为库窗口的显示/隐藏切换；另含**设置热键**（默认 Ctrl+I）优先命中 → toggle 设置对话框。映射为空**且**设置热键未启用时自动停监听。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/core/globalinputlistener.h/.cpp` | Win32 `SetWindowsHookEx(WH_KEYBOARD_LL)` 监听、start/stop |
| `src/core/convertcodetostring.cpp/.hpp` | 键码↔字符串映射、`ShortcutCompare::compareShortcutKeys`（:hpp） |
| `src/ui/hotkeycapture.cpp` | 设置页按键捕获 |
| `src/ui/appcontroller.cpp` | `rebuildHotkeyMapping`、`onGlobalKeyReleased` 分发（设置热键优先）、`applySettingsHotkey`/`syncHotkeyListener`（#3 重构后由 main.cpp 外移） |

## 数据流

```
config → rebuildHotkeyMapping（enabled && hotkey 非空）→ hotkeyToWindow Map
settings.json → applySettingsHotkey → settingsHotkey/enabled 状态（每次 loadSettings 后刷新）
按键 → GlobalInputListener::keyReleased → 录制中？（是则 return 抑制）
      → 设置热键启用且匹配？→ toggleSettings()（toggle 设置对话框，模态期 no-op）return
      → 遍历 hotkeyToWindow → compareShortcutKeys → showWindow()/hide()
映射空 且 设置热键未启用 → stopListening()；否则 startListening()
```

## 依赖关系

- 依赖：Win32 API、convertcodetostring.hpp
- 被依赖：AppController（appcontroller.cpp，唯一使用者）

## 关键规则

- `enabled`（库配置）**仅**控制热键：注册 + 冲突计算，不影响窗口存在
- 热键字符串格式：`modifiers+key`（无修饰键时裸 key）
- 冲突比较 `compareShortcutKeys` 而非字符串相等（顺序/别名归一）
- 设置热键与库热键冲突时**设置热键优先命中**，不靠运行时仲裁 —— 引导靠设置页/库页双向标红

## 变更记录索引

- 2026-10-25：新增设置热键 `openSettingsHotkey`（默认 Ctrl+I，toggle 设置对话框）；监听启动条件扩展；录制期/首启模态期抑制
- 2025-01-14：docs/.ai Init（首次记录）
