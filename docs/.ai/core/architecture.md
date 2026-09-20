# StickersManager 架构文档

## 架构概览

单实例 Qt Widgets 托盘应用。main.cpp 持有全部生命周期：窗口 Map（path→MainWindow）、热键映射、两条重载路径。数据流单向：config → ConfigManager → MainWindow（m_eff 缓存）→ StickerLibrary → 单元格。无复杂状态库，状态在闭包 + QPointer。

## 分层与模块

### 入口层（src/main.cpp）
- 单实例锁、Fusion、托盘、首启设置
- 窗口生命周期：`createWindows` / `removeStaleWindows`（path 即身份）
- 热键：`rebuildHotkeyMapping`（只收 enabled && hotkey 非空）→ `keyReleased` → `compareShortcutKeys` → showWindow/hide
- 两条重载路径（勿混淆）：
  - `fullReload`（main.cpp:134）：loadSettings → 重建窗口 → `reloadLibrary()` 重扫 → syncHotkeyListener
  - `applyChanges`（main.cpp:146）：loadSettings → 重建/删窗口 → `updateLibraryConfig`（不重扫）→ 重算热键 → `applySettings()` → 可见窗口重显

### 配置层（src/core/configmanager.cpp）
- `ConfigManager`：config.json + settings.json；`loadConfig` 时 `mergeDefaults` 内存归一化（不回写）
- `LibraryConfig`（configmanager.h:16）：id/path/hotkey/enabled/settings
- override：`getEffectiveXxx(lib)` → `effVal(lib,cat,key)` → 库 settings → default{} → hardDefaults()
- `EffectiveSettings`（mainwindow.h:23）+ `refreshEffectiveSettings()`：每窗口缓存，滚动热路径零查找

### 库扫描层（src/core/stickerlibrary.cpp）
- 仅直接子目录为分类；`fsutil::forEachStickerFile`（模板）驱动扫描与统计
- `isPreviewFile()`（fsutil.hpp:26）排除 `.preview*` / `*.preview.*`
- 搜索：预建小写文件名索引

### UI 层（src/ui/）
- `MainWindow`：分类面板（CategoryButton）+ 贴纸网格（StickerCell），滚动虚拟化（updateVisibleCells）
- `StickerCell`：名称/大小/类型标签绝对定位覆盖；`CategoryButton`：名称/计数 + 可选时钟
- `SettingsDialog`：4 标签页，QPointer 单例 + WA_DeleteOnClose
- `TrayIcon`：托盘菜单、showSubMenu、notifyTray

### 图像层（src/core/imageloader.cpp、asyncthumbnailloader.cpp、thumbnailcache.cpp）
- stb_image 主、QImageReader 兜底
- AsyncThumbnailLoader：QtConcurrent + QThreadPool
- ThumbnailCache：LRU QCache（默认 200）
- GIF：QMovie + QBuffer + CacheNone，buffer 以 movie 为 parent（outlive）；setInViewport 滚入/滚出

### 输入层（src/core/globalinputlistener.cpp、convertcodetostring.cpp）
- Win32 `WH_KEYBOARD_LL` 钩子；键码↔字符串映射
- 热键映射空 → listener 自动停（main.cpp:120-132）

### 最近使用层（src/core/recentusage.cpp）
- 每库 `recent_<md5(libPath)>.json`，上限 100（recentLimit 覆盖），旧文件加载清理
- 双击复制记录；空 → Recent 隐藏

### 更新层（src/core/updatechecker.cpp）
- GitHub API `releases/latest`；`compareVersions` 数值比较 major.minor.patch

## 数据流

```
config.json/settings.json → ConfigManager（mergeDefaults）→ MainWindow（refreshEffectiveSettings → m_eff）
库目录 → StickerLibrary::scanLibrary（forEachStickerFile + isPreviewFile）→ populateCategories
双击贴纸 → RecentUsageStore::add + 剪贴板复制 → refreshRecentButton（空则建 Recent）
缩略图：文件 → AsyncThumbnailLoader（QtConcurrent）→ ThumbnailCache（LRU）→ onThumbnailLoaded → StickerCell
GIF：滚入 loadAnimation（QMovie+QBuffer）→ 滚出 unloadAnimation
热键：GlobalInputListener → keyReleased → keyCodeToKeyString+modifiersToString → compareShortcutKeys → show/hide
托盘双点 → getDoubleClickTarget() → settings / first-library / 库 id / 旧版 dirName
```

## 路由架构

桌面应用无 URL 路由。"入口路由"：
- 托盘双点 → doubleClickTarget 解析（main.cpp:230-272）
- 托盘 Show 子菜单 → 库 path → 窗口切换
- 全局热键 → 库窗口切换

## 状态管理

- main.cpp 闭包 + QPointer（settingsDlg、firstWindow）；无全局状态库
- MainWindow：m_currentCategory、m_currentStickers、m_currentCells、m_cellMap、m_previewDlg（QPointer）

## 性能优化

- 滚动虚拟化：updateVisibleCells 只布局视口内单元格；离屏 GIF 卸载
- 异步缩略图：QtConcurrent + QThreadPool
- LRU：QCache thumbnailCacheSize
- 搜索防抖：QTimer searchDelayMs
- 热路径读 m_eff 缓存，避免每格 QJsonObject 查找

## 安全考虑

- 单实例 QLockFile（%TEMP%）
- 路径来自用户配置，无沙箱；预览过滤按命名约定
- 无敏感数据；配置明文 JSON

## 扩展性

- 新增设置：hardDefaults()/defaultSettings() → getter → EffectiveSettings + refreshEffectiveSettings() → UI 页
- 新增模块：src/ 加文件（globbed）→ 重跑 cmake
- 新图像格式：stb 不支持 → QImageReader 自动兜底

## 架构演进

- 初始：单窗口单库 → 多窗口每库一窗（createWindows）
- 当前：override 体系、Recent、GIF 生命周期、EffectiveSettings 热路径缓存
- 规划：TODO（无实测规划文档）
