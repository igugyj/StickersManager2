# StickersManager AI 上下文文档索引

## ⚠️ 必须遵守的原则（红线）

1. 默认值单源：所有默认值只在 `hardDefaults()`（configmanager.cpp:10）与 `defaultSettings()`（configmanager.cpp:58），改默认只改这两处
2. 配置无自动保存：只在显式 `saveConfig()`/`saveSettings()` 时写盘（Settings "Save & Reload"、窗口空路径兜底保存 mainwindow.cpp:173）
3. `MainWindow::m_eff` 缓存有效设置：新设置必须同时加进 `EffectiveSettings` 和 `refreshEffectiveSettings()`，滚动热路径（updateVisibleCells）只读 `m_eff`
4. 两条运行时重载路径不可混淆：`fullReload`（托盘 Rescan，重建窗口+重扫库） vs `applyChanges`（设置 applied 信号，只更新配置不重扫）
5. 任务完成后必须：清冗余 + 更新对应文档 + 写 changelog

## 当前变更快照（2026-10-25，六次会话任务）

> 详细条目见 [changelog.md](./changelog.md) 顶部；待办规划见根目录 `TODO`

1. **CMake 配置现代化 + 锁文件区分**（参照 Pelr）
   - `cmake_minimum_required` 3.5→3.16；`CMAKE_BUILD_TYPE` 未设默认 Debug；配置期 `message(STATUS)` 日志（构建类型/编译器/CONSOLE）
   - `CONSOLE` 改为按构建类型推导（Release=OFF、其它=ON，`-DCONSOLE=ON/OFF` 可覆盖），不再硬编码 `set(CONSOLE OFF)`
   - 编译器识别补全：GNU → `-finput-charset=UTF-8 -fexec-charset=UTF-8`，MSVC → `/utf-8`
   - 锁文件区分构建：Release 保持 `%TEMP%/<user>_Stickers Manager.lock`；Debug 注入 `SM_TEST_BUILD`（`$<$<CONFIG:Debug>:...>`）→ `<user>_Stickers Manager_debug.lock`，可与正式版并行运行
2. **日志系统重构**（参照 Pelr logger）
   - `log.hpp` 拆为声明 + `log.cpp` 实现：`extern` 单例 + 锁（消除 per-TU `static` 副本隐患）
   - 控制台中文乱码根治：`WriteConsoleW` 直写 UTF-16（绕开代码页），重定向时回退 UTF-8 → `stderr`
   - `messageHandler` **始终安装**（控制台+文件双写）；删除 `DEBUG_MODE` 宏
   - 分级日志：`defaultLogLevel()` 按构建类型（Debug 构建=Debug、Release=Warning）；`settings.json` 新增 `logLevel`；Base 页新增 Logging/Log Level 下拉（显示实际生效级别，无 Auto 项，等于构建默认时不落盘）；`applyLogLevel()` 4 处（#3 重构后位于 `appcontroller.cpp`）；新增中文启动日志行
   - **`settings.json` 结构变了**：多了可选键 `logLevel`（`config/settings-json.md` 已同步）
3. **Qt 路径去硬编码 → `.vscode`**（参照 Pelr）
   - `CMakeLists.txt:37-44`：`QT_PATH` 默认空 + `if (QT_PATH)` 守卫 + `Qt lookup:` 日志；优先级 `-DQT_PATH`（CI）> `.vscode/settings.json` configureArgs > 系统查找
   - 新建 `.vscode/settings.json`（**gitignored 本地文件**）：`cmake.buildDirectory` 平铺 `build/`、`cmake.configureArgs` 注入 `-DCMAKE_PREFIX_PATH`；需 VSCode **CMake Tools** 扩展（本机尚未安装）
4. **全局设置热键 Ctrl+I**
   - `settings.json` 新增 `openSettingsHotkey`（默认 `"Ctrl+I"`）/ `openSettingsHotkeyEnabled`（默认 `true`），单源 `defaultSettings()`
   - Base 页 Hotkey 分组（Enable 开关 + `HotkeyCaptureButton` 录制），与库热键同一套 `WH_KEYBOARD_LL` 检测系统
   - `AppController`（#3 重构后位于 `appcontroller.cpp`）：`applySettingsHotkey()` 联动监听启停（启动条件含「设置热键启用」），`keyReleased` 设置热键优先命中 + toggle；录制期/首启模态期抑制；冲突双向标红（切页重算）
   - 运行验证通过（开/关、改键、禁用回落）
5. **内存优化：窗口隐藏即释放**（先量化）
   - 量化埋点：`ThumbnailCache::stats()`、`pixmapBytes()/isAnimationLoaded()`、`[mem]` Info 日志（hide/show）
   - 数据：大头是 GIF `QMovie`（整文件 QBuffer + 解码帧，单部 1.2~2.1MB），非缩略图缓存；GIF 类目显示 105.7MB → 旧版隐藏 150.2MB 不回落
   - 实施（温和策略）：`hideEvent` → 全 cell `setInViewport(false)` 卸 GIF + `ThumbnailCache::clear()`；`showEvent` → `updateCellVisibility()` 恢复；cell 保留 pixmap 零闪烁
   - 验收：105.7 → **35.2MB（−67%）**，重开 +6s 恢复 104.7MB；激进 `clearStickerCells()` 经数据否决（仅多 0.85MB）
6. **#2 GIF 首屏复核 + #4 版本号一致性校验**
   - GIF 首屏：**实测为非 bug**（点进 GIF 类目不滚动 12s → `movies=9`，走 `stickercell.cpp:93` 既有路径）；上轮"首屏不播"是类目无 GIF 的混淆变量，`AGENTS.md`/changelog 已更正；`hideEvent` 新增释放前 `logMemoryState("hidden")` 对照日志
   - 新增 `scripts/check_version.ps1`：CHANGELOG 顶部**仍为单源**（B 方案），比对 `appinfo.h`（剥 v）与 `pkg.iss`（剥 v），不一致即 throw；stdout 仅 `version=`/`tag=` 两行，状态行走 stderr（防污染 `$GITHUB_OUTPUT`）
   - 接入 `make_release_notes.ps1`（开头调用）与 `release.yml`（原 Parse 步骤替换，step id 不变 → 后续 7z/ISCC/tag/release 零改动）
   - 验证：正向三处 2.8.0 通过；反向 3 个篡改场景均按预期报错且文件还原；管道输出仅 2 行；YAML 语法通过（CI 端无法本地跑 Actions，属已知局限）
7. **#3 精简 main.cpp → AppController（重构）**
   - 新建 `src/ui/appcontroller.h/.cpp`：main() 全部 lambda/闭包收编为成员函数（窗口管理、全局热键、设置对话框、托盘接线、两条重载路径）；`main.cpp` 378 → **43 行**启动骨架
   - 顺带消除 `std::function toggleSettings` 前置声明技巧与死代码 `firstWindow`（只写不读）；`checkupdate.hpp` 补自包含 include（appinfo.h/tray.h）
   - 顺序红线逐条保持：首启模态先于托盘/窗口、`applyLogLevel` 4 处、`applySettingsHotkey` 4 处、`fullReload`≠`applyChanges`、`keyReleased` 三段顺序
   - 自动化验收 10/10（热键开/关窗、Ctrl+I 开/关、启动日志行、`3 hotkeys`、首启模态模拟且配置 hash 还原一致）；`AGENTS.md` + `docs/.ai` 15+ 处 `main.cpp:N` 行号引用迁至 `appcontroller.cpp`
8. **修复隐藏后 private bytes 不回落 784MB（QMovie 环泄漏）**
   - 根因：`~StickerCell()` 只 `m_movie->stop()` 不 `delete`，而 m_movie↔QBuffer 互为 parent（环）且不挂 cell → 切类目 `clearStickerCells()`/滚动剪枝 `delete cell` 整环泄漏
   - 先量化：`mCreated/mDestroyed` 原子计数 + `privMB/wsMB` + hide 后 decay 采样；实验 C 5 轮 = priv 线性 +60MB/轮（→341.7）、隐藏 315 平线、**leaked=96**
   - 修复：析构改走 `unloadAnimation()`；验收 **leaked=0、浏览稳定 ~64MB、隐藏回落 34MB**（同脚本 A/B 对照）

## 项目类型

Windows 桌面贴纸管理器，C++20 + Qt 6.10.1（Widgets），MinGW 构建，单实例托盘应用，本地文件夹即贴纸库。

## 文档结构（按优先级从高到低）

### 核心文档（必读）

| 文档路径 | 优先级 | 内容描述 |
|---|---|---|
| [core/project-overview.md](./core/project-overview.md) | 1 | 项目概述、技术栈、目录结构、关键功能 |
| [core/architecture.md](./core/architecture.md) | 2 | 架构设计、数据流、模块划分、性能 |
| [core/quick-start.md](./core/quick-start.md) | 3 | 构建命令、开发流程、调试手段 |

### 变更台账（每次会话开头读）

| 文档路径 | 优先级 | 内容描述 |
|---|---|---|
| [changelog.md](./changelog.md) | 0 | 最新变更记录，了解项目现状 |

### 模块文档（按任务点读）

| 文档路径 | 优先级 | 内容描述 |
|---|---|---|
| [modules/core-config.md](./modules/core-config.md) | 4 | ConfigManager、LibraryConfig、override 体系 |
| [modules/core-library.md](./modules/core-library.md) | 4 | StickerLibrary 扫描、分类、preview 过滤 |
| [modules/core-image.md](./modules/core-image.md) | 4 | 图片加载（stb/QImageReader）、缩略图缓存、GIF 动画 |
| [modules/core-input.md](./modules/core-input.md) | 4 | 全局热键（Win32 hook）、键码映射 |
| [modules/core-recent.md](./modules/core-recent.md) | 4 | 最近使用记录（Recent 分类） |
| [modules/ui-mainwindow.md](./modules/ui-mainwindow.md) | 4 | MainWindow 布局、滚动虚拟化、搜索、单元格 |
| [modules/ui-settings.md](./modules/ui-settings.md) | 4 | SettingsDialog 四标签页、设置页组件 |
| [modules/ui-tray.md](./modules/ui-tray.md) | 4 | 托盘图标、菜单、通知 |

### 配置文档（按需参考）

| 文档路径 | 优先级 | 内容描述 |
|---|---|---|
| [config/config-json.md](./config/config-json.md) | 按需 | config.json（default + libraries）详解 |
| [config/settings-json.md](./config/settings-json.md) | 按需 | settings.json 详解 |

### 最佳实践（参考级）

| 文档路径 | 优先级 | 内容描述 |
|---|---|---|
| [best-practices/development.md](./best-practices/development.md) | 参考 | 开发规范与踩坑（CMake/默认值/热路径） |
| [best-practices/deployment.md](./best-practices/deployment.md) | 参考 | 构建发布与 CI/CD（Inno Setup、release.yml） |

## 读取建议

1. changelog 尾部最新条目 → 项目现状
2. core/ 三篇 → 理解底座
3. 按任务点读 modules/、config/
4. 规范类 best-practices/ 按需查

## 文档更新规则

- 结构变化 → project-overview.md
- 架构变化 → architecture.md
- 模块增改 → 对应 modules/*.md
- 配置变化 → 对应 config/*.md
- **每次任务完成后必须更新 changelog.md，记录本次变更**

## 关键路径

- 根目录：`D:\programing\Cpp\StickersManager`
- 源码：`src/`（main.cpp、appinfo.h、core/、ui/、utils/）
- 构建：`build/`（MinGW Makefiles）
- 发布：`pkg.iss`（Inno Setup）、`release/`、`scripts/clean_release.py`
- 资源：`assets/`（st.ico、st.png、menu.qss、icon.rc）
- 第三方：`thirdparty/stb/`（stb_image.h、stb_image_resize2.h）
- 版本：`src/appinfo.h:7`（当前 `v2.9.0`，带 `v` 前缀；与 `CHANGELOG.md` 顶部、`pkg.iss` 三处须一致，`scripts/check_version.ps1` 强制校验）
- 运行时配置：`[EXE_DIR]/.stickersmanager/`（config.json、settings.json、recent_*.json）
- 日志：`log/log.log`（相对工作目录）
