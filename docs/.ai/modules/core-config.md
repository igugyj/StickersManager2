# ConfigManager 模块

## 职责

读写 config.json + settings.json，提供全项目设置默认值（单源）与每库 effective 覆盖解析。不负责 UI，不自动保存。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/core/configmanager.h` | `LibraryConfig` 结构、getter 声明、`defaultBlock`/`defVal`/`effVal` 私有辅助 |
| `src/core/configmanager.cpp` | `hardDefaults()`（configmanager.cpp:10）、`defaultSettings()`（:58）、`mergeDefaults`（:45）、读写实现 |

## 数据流

```
config.json → loadConfig() → mergeDefaults(default{} ← hardDefaults()) → getLibraries()（按 id 排序）
getEffectiveXxx(lib) → effVal(lib,cat,key) → lib.settings[cat][key] → default[cat][key] → hardDefaults()
settings.json → loadSettings() → getDoubleClickTarget() 等
显式 saveConfig()/saveSettings()（QSaveFile）→ 写盘
```

## 依赖关系

- 依赖：Qt Core（QJson/QFile/QSaveFile/QStandardPaths）
- 被依赖：main.cpp、MainWindow、SettingsDialog 各页、TrayIcon

## 关键规则

- 默认值**只**在 `hardDefaults()` / `defaultSettings()`，getter 无 fallback 参数
- `id` = 顺序标记（0..n-1 连续）：`getLibraries()` 按 id 排序，缺失 id（legacy）取数组位置，拖拽重排重编号
- `enabled` = 热键开关**仅**：只影响热键注册与冲突计算，不影响窗口/托盘/双击目标/统计
- 配置路径：`[EXE_DIR]/.stickersmanager/`（applicationDirPath 相对，非可移植配置）

## 变更记录索引

- 2025-01-14：docs/.ai Init（首次记录）
