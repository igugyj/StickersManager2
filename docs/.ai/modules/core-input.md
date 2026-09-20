# 全局热键模块

## 职责

注册全局热键（Win32 低级键盘钩子），把按键事件映射为库窗口的显示/隐藏切换。热键映射为空时自动停监听。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/core/globalinputlistener.h/.cpp` | Win32 `SetWindowsHookEx(WH_KEYBOARD_LL)` 监听、start/stop |
| `src/core/convertcodetostring.cpp/.hpp` | 键码↔字符串映射、`ShortcutCompare::compareShortcutKeys`（:hpp） |
| `src/ui/hotkeycapture.cpp` | 设置页按键捕获 |
| `src/main.cpp:30-38,101-132` | `rebuildHotkeyMapping`、keyReleased 分发、syncHotkeyListener |

## 数据流

```
config → rebuildHotkeyMapping（enabled && hotkey 非空）→ hotkeyToWindow Map
按键 → GlobalInputListener::keyReleased(keyCode, modifiers) → keyCodeToKeyString + modifiersToString → compareShortcutKeys → showWindow()/hide()
映射空 → stopListening()；非空且未监听 → startListening()
```

## 依赖关系

- 依赖：Win32 API、convertcodetostring.hpp
- 被依赖：main.cpp（唯一使用者）

## 关键规则

- `enabled`（库配置）**仅**控制热键：注册 + 冲突计算，不影响窗口存在
- 热键字符串格式：`modifiers+key`（无修饰键时裸 key）
- 冲突比较 `compareShortcutKeys` 而非字符串相等（顺序/别名归一）

## 变更记录索引

- 2025-01-14：docs/.ai Init（首次记录）
