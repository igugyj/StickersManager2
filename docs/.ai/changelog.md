# 更新日志

本文件记录 StickersManager 的所有重要变更。**新条目加在顶部**。

---

## [2026-10-25]

### CMake 现代化 + 日志系统重构 + Qt 路径去硬编码（参照 Pelr）

- CMake 配置现代化
  - `cmake_minimum_required` 3.5 → 3.16；`CMAKE_BUILD_TYPE` 未设置默认 Debug；新增配置期 `message(STATUS)` 日志（构建类型 / 编译器 ID / CONSOLE / Qt 查找来源）
  - `CONSOLE` 由硬编码 `set(CONSOLE OFF)` 改为按构建类型推导（Release=OFF、其它=ON），`-DCONSOLE=ON/OFF` 可覆盖
  - 编译器识别补全：GNU → `-finput-charset=UTF-8 -fexec-charset=UTF-8`（修 MinGW 中文），MSVC → `/utf-8`
  - `QT_PATH` 去硬编码（默认空 + `if (QT_PATH)` 守卫，避免清掉 configureArgs 传入的 `CMAKE_PREFIX_PATH`）；优先级 `-DQT_PATH`（CI）> `.vscode/settings.json` > 系统查找；新建 `.vscode/settings.json`（gitignored，CMake Tools 的 `cmake.configureArgs` 注入 Qt 路径，`cmake.buildDirectory` 平铺 `build/`）
- 锁文件区分发行/测试版
  - Debug 构建经 `$<$<CONFIG:Debug>:SM_TEST_BUILD>` 注入宏 → `%TEMP%/<user>_Stickers Manager_debug.lock`；Release 锁名不变，两版可同时运行（`main.cpp:44-48`）
- 日志系统重构（Pelr logger 模式）
  - `log.hpp` 拆为声明 + `log.cpp` 实现：`extern g_logLevel/g_logMutex` 单例 + 带锁 `set/getLogLevel()`，消除头文件 `static` 每 TU 副本隐患
  - 控制台中文乱码根治：`writeToConsole()` 用 `WriteConsoleW` 直写 UTF-16 绕开控制台代码页，输出被重定向时回退 UTF-8 字节写 `stderr`
  - `messageHandler` 始终安装：控制台构建 控制台+文件 双写，非控制台仅文件（此前控制台构建根本不写日志文件）；删除 `DEBUG_MODE` 宏与 `LOG_TO_CONSOLE` 宏
  - 分级日志：`defaultLogLevel()` 按构建类型（CONSOLE=Debug、否则 Warning）；`settings.json` 新增 `logLevel` 键（空=未设置→构建默认）；Base 页新增 Logging 分组 Log Level 下拉（Debug/Info/Warning/Critical/Fatal，显示实际生效级别，选中值等于构建默认时清除键不落盘）；`main.cpp` 在启动及 3 处 `loadSettings()` 后 `applyLogLevel()`；新增中文启动日志行
  - 验证：字节级确认 `log.log` 为合法 UTF-8；`logLevel=Warning` 时 0 条 Debug / 2 条 Warning

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
    - 版本 2.7.1 与 CHANGELOG.md 顶部是否同步（appinfo.h:7 实测当前值）

<!--
格式：`- 功能/修复/重构标题` + 缩进子项（改了什么 + 为什么 + 关键细节）
示例：
- 导航栏自动隐藏
  - 桌面端 hover 顶部 100px 下拉显示
  - 移动端始终显示（userAgent 检测，非屏幕宽度）
  - 环境变量 NEXT_PUBLIC_HEADER_AUTO_HIDE_ENABLED 仅作用桌面端
-->
