# SettingsDialog 模块

## 职责

四标签页配置对话框（General / Libraries / Base / About），"Save & Reload" 应用全部页面并触发 `applied` 信号。QPointer 单例，托盘双点开合。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/ui/settingsdialog.h/.cpp` | 对话框容器、标签页组装（:33-36）、onSave（:60）、applied 信号 |
| `src/ui/generalsettingspage.h/.cpp` | General：窗口/网格/行为默认值 |
| `src/ui/librarysettingspage.h/.cpp` | Libraries：库管理、拖拽重排（重编号 id）、热键捕获、结构统计 |
| `src/ui/basesettingspage.h/.cpp` | Base：双击目标等 settings.json 项 |
| `src/ui/aboutpage.h/.cpp` | About：版本对比、更新检查 |
| `src/ui/settingswidgets.h` | 共享组件：`makeSpinBox`、`makeOverrideSpinBox`（0=General）、`makeBoolCombo`、`boolToCombo`、`makeInfoButton` |

## 数据流

```
托盘双点/菜单 → showSettings(toggle) → QPointer 检查 → new SettingsDialog(&config, true)
Save & Reload → 各页 apply → saveConfig() + saveSettings() → emit applied → applyChanges（main.cpp:146）
Base 页 show → 刷新双击目标下拉（settingsdialog.cpp:40-45）
```

## 依赖关系

- 依赖：ConfigManager、四个设置页、settingswidgets.h
- 被依赖：main.cpp（showSettings、首启模态 tmpDlg）

## 关键规则

- 共享组件**只**在 settingswidgets.h 定义，页面 include 绝不重声明
- "Save & Reload" 后对话框保持打开
- 首启模式：`SettingsDialog tmpDlg(&config, false, nullptr)` 模态，Accepted 后 loadSettings

## 变更记录索引

- 2025-01-14：docs/.ai Init（首次记录）
