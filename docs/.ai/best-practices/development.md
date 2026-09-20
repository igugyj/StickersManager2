# 开发最佳实践

## 新增设置项流程（五步，缺一不可）

1. `hardDefaults()`（configmanager.cpp:10）或 `defaultSettings()`（:58）加默认值 —— 单源
2. ConfigManager 加 getter（无 fallback 参数）
3. `EffectiveSettings`（mainwindow.h:23）加字段 + `refreshEffectiveSettings()` 填充 —— 否则滚动热路径读不到
4. 设置页（General 或 Libraries）加控件，用 `settingswidgets.h` 共享组件
5. 库级可覆盖项：加 `getEffectiveXxx(lib)` 走 `effVal()`

## 源文件管理

- 新 `.cpp/.h/.hpp` 加进 `src/` 后**必须重跑 `cmake ..`**（FILE(GLOB_RECURSE) 只在新配置时纳入）
- 头文件搜索路径：`./ src/ src/core/ src/ui/ src/utils/ thirdparty/stb`（CMakeLists.txt:41-48）
- 新 Qt 模块：`find_package(Qt6 COMPONENTS ...)` + `target_link_libraries` 两处都要加（CMakeLists.txt:29,60）

## 热路径纪律

- `updateVisibleCells` / `updateCellVisibility` 是滚动热路径：**只读 `m_eff`**，禁止 QJsonObject 查找
- 缩略图/图像解码绝不走 UI 线程（AsyncThumbnailLoader）
- 离屏 GIF 必须卸载（setInViewport 对称调用）

## 线程与信号

- worker 线程 → GUI：`notifyTray()`（QueuedConnection），禁止直接碰托盘
- QBuffer 给 QMovie 当 parent，保证 outlive
- QPointer 管对话框生命周期（settingsDlg、m_previewDlg），配 WA_DeleteOnClose

## 命名与结构

- header-only 工具用 `static` 函数（log.hpp/launcher.hpp/fsutil.hpp），每 TU 一份
- `EffectiveSettings` 命名 `m_eff`；库配置 `m_libConfig`；配置指针 `m_config`
- 分类/贴纸相关 UI 组件在 `src/ui/`，纯逻辑在 `src/core/`

## 违例与后果

- 曾在别处写死默认值 → 与 hardDefaults 分叉；现强制单源 + getter 无 fallback
- 新设置漏加 `EffectiveSettings` → 滚动时表现不一致（m_eff 缺字段）；已列入五步流程
