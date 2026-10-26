# 更新日志

本文件记录 StickersManager 的所有重要变更。**新条目加在顶部**。

---

## [2026-10-26]

### 中键打开资源管理器并选中文件（TODO #1，受 SuzuEmojy v1.13.2 启发）+ 预览残留修复

- **功能**：`StickerCell` 新增信号 `middleClicked(filePath)`（`mousePressEvent` 改三分支：右键 / 中键 / 其余），动作在 `MainWindow::onStickerMiddleClicked`：`closePreview()` → `m_highlightManager.setHighlightedPath(...)`（沿用 `highlightOnClick` 语义，**先关预览 → 再高亮 → 最后执行**，用户明确要求"先高亮再执行动作"）→ `QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(...)})`（Qt6 两参数重载，含空格路径安全；文件不存在 → `qWarning [MiddleClick]` + 托盘警告）
- **双击复制限左键**：`mouseDoubleClickEvent` 原本除右键外都复制 → 中键双击会"复制 + 连开两次资源管理器"，改为 `button() != Qt::LeftButton` 直接 return
- **预览残留 bug（用户发现）**：`ImagePreviewDialog` 是**独立 top-level 窗口**，双击复制时 `onStickerDoubleClicked` 只 `hide()` 主窗口，预览不随父窗口隐藏 → 双击后预览一直挂着
  - **最终设计（用户三轮拍板）**：挂在两处 —— `MainWindow::hideEvent` 开头 `closePreview()`（**随仓库窗口隐藏而隐藏，隐即关闭、再次显示不恢复**）+ **中键开头 `closePreview()`**（用户先定"中键不管"、后改口"中键也关"）；三处调用 = 右键 toggle / hideEvent / 中键
  - **顺序红线**：`closePreview()` → `finished` → `onPreviewClosed()` 会 `clearHighlight()`，所以中键里它必须排在 `setHighlightedPath` **之前**，否则高亮刚设就被清掉（验收日志 `setHighlightedPath: "<path>" old: ""` 正是该顺序的实证）
  - `closePreview()` 为幂等助手（`accept()` → 置空 `m_previewDlg` + 防御性再置空）
- **预览跟随点击（用户追加需求"点 cell → 预览切图"）**：此前左键只高亮、预览停在原图。新增 `ImagePreviewDialog::setCurrentFile(siblings, filePath)`（同类目命中 `m_siblings` 索引 → `showAt`；跨类目以该文件所在类目重建列表，兜底单元素；路径相同直接 return 不重载），由 `MainWindow::onStickerClicked` 在 `m_previewDlg` 非空时调用；抽出私有助手 `categorySiblings(filePath)`，与右键开预览共用同一段取 siblings 的逻辑
- **验收（pwsh 自动化 harness，14/14 通过，无残留进程/窗口）**：右键开预览 → **左键点另一个 cell → 预览切图**（`onPreviewFileChanged` 0→1，且其路径 == 点击 cell 的 `setHighlightedPath` 路径）且预览保持打开 → 中键（选在预览矩形外的 cell）→ **预览关闭** + 新增 explorer 窗口 + `setHighlightedPath` 非空路径 + 无 `[MiddleClick]` 告警 → 右键重新开预览 → 双击同 cell → **预览关闭（hideEvent 路径）** + 主窗口隐藏 + `Successfully copied file to clipboard`；热键恢复主窗口 → 左键不开 explorer、不开预览
- **harness 坑（复用注意）**：
  - `mouse_event` 中键标志是 **MIDDLEDOWN=0x0020 / MIDDLEUP=0x0040**（0x0010 是 RIGHTUP、0x0002/0x0004 才是左键）—— 记错就完全点不出事件，表现为"三项交互全失败但左键高亮正常"
  - 预览对话框**没有显式标题** → Windows 上标题取应用名 `Stickers Manager`（不是空串）；另有 1 个 0×0 尺寸的 Qt 助手窗口常驻，枚举时须按"尺寸 ≥50 且居中"过滤，否则误判
  - 预览居中覆盖主窗口大半，测"预览开着时点 cell"必须选**预览矩外**的 cell，否则点击落在预览上
  - 日志时间戳格式含 12/24 小时歧义 → 断言改用**匹配行计数前后差**，不用时间戳字符串比较

## [2026-10-25]

### 修复：切类目/滚动剪枝泄漏 QMovie（隐藏后 private bytes 不回落 784MB 根因）

- **现象**：窗口隐藏后 `hidden-released` 已执行（`movies=0`/`cache=0`），private bytes 仍钉死在 784.59MB 不回落
- **根因（H2 真泄漏，静态锁定 + 计数器实证）**：`StickerCell::~StickerCell()`（stickercell.cpp:71）只 `m_movie->stop()` 不 `delete`；而 `m_movie` 与 QBuffer **互为 parent（环）且不挂在 cell 下**（`new QMovie(buffer)` + `buffer->setParent(m_movie)`）→ cell 带播放中动画被删即整环泄漏（QMovie + 整个 GIF 文件 QBuffer ~2MB + 解码器）。两条高频删除路径：`clearStickerCells()`（mainwindow.cpp:271，切类目整批删）与滚动剪枝 `delete cell`（mainwindow.cpp:365，`margin=1`）
- **量化埋点（Build 1，Info 级，长期保留）**：
  - QMovie 创建/销毁全局原子计数 `animationMoviesCreated()/animationMoviesDestroyed()`（stickercell.cpp，`loadAnimation` 创建 +1 / `unloadAnimation` 删除 +1），`[mem]` 行输出 `mCreated/mDestroyed` —— **差值 > 0 即泄漏**
  - `MainWindow::logMemoryState()` 新增 `privMB/wsMB`（`GetProcessMemoryInfo` → `PrivateUsage`，mainwindow.cpp `processMemoryUsage()`，链 `psapi` + 需先 `<windows.h>`）
  - `hideEvent` 释放后 `QTimer::singleShot` 追加 1/5/15/60s 四条 `[mem] decay+Xs` 行 → 判"缓慢回落" vs "钉死"
- **实验 A（Build 1 未修）**：bilibili 深滚 → 切 DeepSeek → 回切 → 隐藏：`mCreated=63 ≠ mDestroyed=51`（**leaked=12**），priv 隐藏后 72MB 平线
- **实验 C（Build 1 未修，5 轮切类目+深滚对照基线）**：priv **线性 +60MB/轮**（23.8 → 341.7MB），隐藏后 **315.7 → 315.6 完全平线**，`mCreated=270 / mDestroyed=174` → **leaked=96**；每泄漏环 ≈3.3MB，与用户 784MB 现象同构（更长会话即到该量级）
- **修复**：`~StickerCell()` 改为调用 `unloadAnimation()`（其 `delete m_movie` 连带析构 child QBuffer 断环，该路径 hideEvent 现网已验证安全）
- **验收（实验 C 同脚本 A/B 对照）**：5 轮浏览 priv **64.7MB 平台期不再线性涨**（修复前 341.7）；隐藏后 **34.0 → 33.9 回落至近基线**（修复前 315 平线）；**`mCreated=270 == mDestroyed=270`，leaked=0**
- 排除项：`unloadAnimation()` 同步 delete 正确、`frameChanged` 只写缩放后 pixmap、`hideEvent` 遍历的 `m_cellMap` 覆盖全部活 cell；H1（分配器滞留）不成立 —— 修复后同强度浏览 priv 稳定在 ~64MB 平台
- 自动化 harness 两个坑（复用时注意）：`mouse_event` 的 LDOWN/LUP **忽略 dx/dy**（须先 `SetCursorPos`）；PowerShell 中 `Cat` 是 `Get-Content` 别名（勿用作函数名）

### CMake 现代化 + 日志系统重构 + Qt 路径去硬编码（参照 Pelr）

- CMake 配置现代化
  - `cmake_minimum_required` 3.5 → 3.16；`CMAKE_BUILD_TYPE` 未设置默认 Debug；新增配置期 `message(STATUS)` 日志（构建类型 / 编译器 ID / CONSOLE / Qt 查找来源）
  - `CONSOLE` 由硬编码 `set(CONSOLE OFF)` 改为按构建类型推导（Release=OFF、其它=ON），`-DCONSOLE=ON/OFF` 可覆盖
  - 编译器识别补全：GNU → `-finput-charset=UTF-8 -fexec-charset=UTF-8`（修 MinGW 中文），MSVC → `/utf-8`
  - `QT_PATH` 去硬编码（默认空 + `if (QT_PATH)` 守卫，避免清掉 configureArgs 传入的 `CMAKE_PREFIX_PATH`）；优先级 `-DQT_PATH`（CI）> `.vscode/settings.json` > 系统查找；新建 `.vscode/settings.json`（gitignored，CMake Tools 的 `cmake.configureArgs` 注入 Qt 路径，`cmake.buildDirectory` 平铺 `build/`）
- 锁文件区分发行/测试版
  - Debug 构建经 `$<$<CONFIG:Debug>:SM_TEST_BUILD>` 注入宏 → `%TEMP%/<user>_Stickers Manager_debug.lock`；Release 锁名不变，两版可同时运行（`main.cpp:20-33`）
- 日志系统重构（Pelr logger 模式）
  - `log.hpp` 拆为声明 + `log.cpp` 实现：`extern g_logLevel/g_logMutex` 单例 + 带锁 `set/getLogLevel()`，消除头文件 `static` 每 TU 副本隐患
  - 控制台中文乱码根治：`writeToConsole()` 用 `WriteConsoleW` 直写 UTF-16 绕开控制台代码页，输出被重定向时回退 UTF-8 字节写 `stderr`
  - `messageHandler` 始终安装：控制台构建 控制台+文件 双写，非控制台仅文件（此前控制台构建根本不写日志文件）；删除 `DEBUG_MODE` 宏与 `LOG_TO_CONSOLE` 宏
  - 分级日志：`defaultLogLevel()` 按构建类型（CONSOLE=Debug、否则 Warning）；`settings.json` 新增 `logLevel` 键（空=未设置→构建默认）；Base 页新增 Logging 分组 Log Level 下拉（Debug/Info/Warning/Critical/Fatal，显示实际生效级别，选中值等于构建默认时清除键不落盘）；`main.cpp` 在启动及 3 处 `loadSettings()` 后 `applyLogLevel()`；新增中文启动日志行
  - 验证：字节级确认 `log.log` 为合法 UTF-8；`logLevel=Warning` 时 0 条 Debug / 2 条 Warning
- 全局设置热键 Ctrl+I（打开/切换设置对话框）
  - `settings.json` 新增 `openSettingsHotkey`（默认 `"Ctrl+I"`，录制模式字符串，`""` = 未绑定）与 `openSettingsHotkeyEnabled`（默认 `true`），单源 `defaultSettings()`
  - Base 页新增 Hotkey 分组：`Enable` 复选框 + `Open Settings` `HotkeyCaptureButton`（与库热键同一套录制/检测系统），`applyToConfig()` 落盘
  - `main.cpp` 接线：`applySettingsHotkey()` 刷新状态并联动 `syncHotkeyListener()`（启动条件改为「库热键 map 非空 **或** 设置热键启用」），在启动（`createWindows` 后）及 3 处 `loadSettings()` 后调用；`keyReleased` 中设置热键**优先命中**再走窗口 map（toggle 语义）；首次启动模态对话框期间 no-op（`activeModalWidget()`）；Base 页录制期间 `SettingsDialog::isHotkeyRecording()` 抑制全局触发
  - 冲突**双向标红**：Base 页 `setConflictChecker` 比对启用库热键，`LibrarySettingsPage::hotkeyInUse()` 比对设置热键，切页时 `SettingsDialog` 重跑 `refreshHotkeyConflict()` / `validateAllHotkeys()`
  - 验证：默认 Ctrl+I 开/关（监听日志 `2 hotkeys`）；改键 `Ctrl+Shift+F9` 生效；`enabled=false` 回落 `1 hotkeys` 且按键失效
- 内存优化：窗口隐藏即释放（先量化再动手）
  - **量化埋点**（Info 级，长期保留）：`ThumbnailCache::stats()`（条目 + 估算字节，key→bytes 台账按 `size()` 剪除淘汰键）、`StickerCell::pixmapBytes()/isAnimationLoaded()`、`CategoryButton::pixmapBytes()`、`MainWindow::logMemoryState()` —— hide/show 输出 `[mem]` 行
  - **量化结论**（双库 emo 1207 张 / ddemo 688 张，`animateThumbnails=true`，自动化测：显示 → 滚动 → 隐藏）：启动隐藏 20.0MB；静态类目显示 23.9MB、隐藏 23.6MB（几乎不回落）；**GIF 类目（bilibili 343/344 GIF，均值 2MB/部）显示 105.7~149.9MB、隐藏后 150.2MB 完全不回落** —— 大头是 GIF `QMovie`（`loadAnimation` 整文件读入 QBuffer + 解码帧），**不是**缩略图缓存（实测仅 0.9~4.9MB）
  - **释放策略（温和，按数据选定）**：`MainWindow::hideEvent` 对全部 cell `setInViewport(false)`（复用 scroll-out 路径卸 QMovie）+ `m_thumbnailCache->clear()`；**不**做激进的 `clearStickerCells()`（只多回收 0.85MB，却要付重建代价）；cell 保留 `m_currentPixmap` → 重开零闪烁
  - 恢复：`showEvent` 调 `updateCellVisibility()` 重置视口状态，动画按既有懒加载路径恢复；托盘"隐藏"逐窗口 `hide()` 同样覆盖
  - **验收数据**：GIF 类目显示 105.7MB → 隐藏 **35.2MB（−70.5MB，−67%）**；重开 +6s 恢复 104.7MB（动画回来），`[mem]` 显示 `cache=0`、`cellPixKB=850` 前后不变（无占位闪烁）
- GIF 首屏播放复核（原"附带发现"已证伪）
  - 曾怀疑首屏 cell 因 `m_inViewport` 默认 `true` 导致 GIF 要"滚出再滚回"才播放 —— **实测为误判**：点进 GIF 类目（bilibili）**全程不滚动** 12s，`[mem] hidden` 行 `movies=9`，即走 `setThumbnail()` 既有路径（`m_animateEnabled && isAnimated && m_inViewport` → `loadAnimation()`，stickercell.cpp:93）正常播放
  - 之前 `movies=0` 的原因是该首屏类目（`13-10-40_*`）**0 个 GIF**（全 webp/png），属混淆变量；`AGENTS.md` 的 "Known quirk" 已改为 "verified working" 更正段
  - 代码不改；`hideEvent` 顺带新增**释放前** `logMemoryState("hidden")`，与释放后 `hidden-released` 对照，`movies` 计数即 GIF 播放实证
- 版本号一致性校验（`scripts/check_version.ps1`，新）
  - CHANGELOG.md 顶部**仍为版本单源**（B 方案），脚本解析后比对 `src/appinfo.h` `version()`（剥 `v` 前缀）与 `pkg.iss` `#define MyAppVersion`（剥 `v`），任一不一致即 `throw`
  - 输出：stdout 仅 `version=X.Y.Z` / `tag=vX.Y.Z` 两行供 `$GITHUB_OUTPUT` 管道；状态行走 **stderr**（`[Console]::Error.WriteLine`，避免污染 CI 输出）—— 修掉初版 `Write-Host` 被管道捕获的 bug
  - 接入两处：`scripts/make_release_notes.ps1` 开头 `$null = & check_version.ps1`；`.github/workflows/release.yml` 原 "Parse version from CHANGELOG" 步骤替换为 `.\scripts\check_version.ps1 | Out-File $env:GITHUB_OUTPUT -Append`（step id 仍为 `version`，后续 7z/ISCC/tag/release 四步零改动）
  - 验证：正向三处一致（2.8.0）通过；反向 T1 改 appinfo→2.9.0、T2 改 pkg.iss→2.7.0、T3 经 make_release_notes 调用，均按预期 throw 且文件还原（hash 一致）；管道输出确认仅 2 行；release.yml YAML 语法 `yaml.safe_load` 通过
  - 局限：CI 端无法本地跑 GitHub Actions，CI 步骤只是调用本地已验证的同一脚本
- **#3 精简 main.cpp：逻辑外移 → `AppController`**（TODO#3，阶段 1 机械搬移 + 阶段 2 接口整理）
  - 新建 `src/ui/appcontroller.h/.cpp`（QObject）：main() 的全部 lambda/闭包收编为成员 —— `createWindows`/`removeStaleWindows`/`rebuildHotkeyMapping`/`onGlobalKeyReleased`（原 keyReleased 连接）/`syncHotkeyListener`/`applySettingsHotkey`/`fullReload`/`applyChanges`/`showSettings`/`toggleSettings`/`setupTray`/`onShowMenuTriggered`/`onTrayActivated`；状态收编为成员 `m_config`(引用)/`m_windows`/`m_hotkeyToWindow`/`m_settingsDlg`/`m_listener`/`m_settingsHotkey*`
  - `main.cpp` 378 → **43 行**：initLogFile → messageHandler → QApplication → 锁 → Fusion → ConfigManager → `AppController controller(config)` → `startup()` → `exec()`
  - 顺带消除：`std::function toggleSettings` 前置声明技巧（类内成员互相可见，不再需要）、死代码 `firstWindow`（只写不读，grep 验证后删除）
  - `checkupdate.hpp` 补自包含 include（`appinfo.h` + `tray.h` —— 原来靠 main.cpp 先包含才编译过）
  - 顺序红线逐条保持：首启模态先于托盘/窗口、`applyLogLevel` 4 处、`applySettingsHotkey` 4 处（首启 :270 先于 :275 的原始顺序）、`fullReload`≠`applyChanges`、`keyReleased` 三段顺序、监听在 createWindows 后启动
  - 自动化验收 10/10：开/关窗（Ctrl+Shift+E / Shift+Ctrl+D）、Ctrl+I 开/关设置、启动日志行、`3 hotkeys`、首启模态（临时挪配置目录模拟，配置 hash 还原一致）
  - 文档同步：`AGENTS.md`（新增 Architecture 首条 + 8 处 `main.cpp:N` 行号 → `appcontroller.cpp`）+ `docs/.ai` 7 个文件的行号/结构引用

## [2025-01-16]

### 高亮管理重构 + 预览窗口精简

- 新增 HighlightManager（src/core/highlighmanager.h）：路径驱动高亮，统一状态源
  - m_highlightedPath 替代旧 StickerCell* m_highlightedCell，消除虚拟滚动野指针
  - refresh() 遍历 m_cellMap，按路径匹配设置/清除高亮
- MainWindow 高亮逻辑统一：左键、右键、预览切换三处均走 HighlightManager，受 highlightOnClick 配置控制
- onPreviewFileChanged 按索引计算滚动位置，先滚动创建 cell 再设置高亮路径
- StickerCell::mousePressEvent 移除自动高亮，仅发射信号
- StickerCell::setHighlighted 改用 QApplication::palette() 解决颜色不一致
- 预览窗口移除底部操作栏，仅保留顶部信息栏 + 中部图片区
- 预览窗口移除播放控制按钮，GIF 播放状态仅由 animatePreview 设置控制
- eventFilter 改为 QApplication::installEventFilter 全局捕获
- 关闭预览时清除高亮

## [2025-01-15]

### 增强图片预览窗口

- ImagePreviewDialog 三栏布局：顶部元数据栏（文件名/扩展名/容器格式/像素尺寸/磁盘大小/修改时间/类目）
- 复制功能：复制图像（pixmap→剪贴板）、复制路径（文本）、复制文件（QMimeData urls，可粘进资源管理器，镜像双击复制语义）
- 上一张/下一张导航：方向键 + 图片区左右10% hover箭头；兄弟列表来源为 StickerLibrary::getCategories()（规范数据，不重扫描）
- GIF 播放/暂停切换（默认遵循 animatePreview 设置）
- 关闭规则重写：ESC + 右键；左键忽略（子控件自行处理）；Enter/Space 不再关闭
- 调用点（mainwindow.cpp onStickerRightClicked）传递类目兄弟列表
- 预览切换上下张时主窗口网格高亮跟随（currentFileChanged 信号）
- 边界提示：hover 进入左右区域但无法导航时显示 "First" / "Last" 标签

## [2025-01-14]

### 初始记录

- 建立 docs/.ai 结构（Init 协议）
  - 扫描来源：README.md、AGENTS.md、CMakeLists.txt、src/main.cpp、src/appinfo.h、src/core/configmanager.h/.cpp、src/ui/mainwindow.h、.github/workflows/release.yml
  - 已建：index.md（红线+分层路由）、core/（project-overview / architecture / quick-start）、modules/（core-config / core-library / core-image / core-input / core-recent / ui-mainwindow / ui-settings / ui-tray）、config/（config-json / settings-json）、best-practices/（development / deployment）
  - 待验证项：
    - `default.window.position/size` 具体数值（configmanager.cpp:14-15，hardDefaults 实测）
    - settings.json 默认值（configmanager.cpp:58-66，defaultSettings 实测）
    - ~~版本 2.7.1 与 CHANGELOG.md 顶部是否同步~~（已解决：2026-10-25 起由 `scripts/check_version.ps1` 强制三处一致，当前 2.9.0）

<!--
格式：`- 功能/修复/重构标题` + 缩进子项（改了什么 + 为什么 + 关键细节）
示例：
- 导航栏自动隐藏
  - 桌面端 hover 顶部 100px 下拉显示
  - 移动端始终显示（userAgent 检测，非屏幕宽度）
  - 环境变量 NEXT_PUBLIC_HEADER_AUTO_HIDE_ENABLED 仅作用桌面端
-->
