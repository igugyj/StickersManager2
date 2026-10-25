# MainWindow 模块

## 职责

每库一个窗口：分类面板 + 贴纸网格，滚动虚拟化渲染，搜索（贴纸/分类），复制/预览交互，缓存有效设置（m_eff）。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/ui/mainwindow.h` | `EffectiveSettings` 结构（:23）、成员、槽/私有方法声明 |
| `src/ui/mainwindow.cpp` | initUI、populateCategories、updateVisibleCells、搜索、事件 |
| `src/ui/stickercell.h/.cpp` | 贴纸单元格：缩略图 + 名称/大小/类型标签 + GIF 生命周期 |
| `src/ui/categorybutton.h/.cpp` | 分类按钮：名称/计数 + 可选时钟图标 |
| `src/core/highlighmanager.h` | HighlightManager：路径驱动高亮，单一状态源 |

## 数据流

```
m_config + LibraryConfig → 构造 → loadLibrary()（StickerLibrary 扫描）→ populateCategories()
显示分类 → displayStickers → relayoutGrid → updateVisibleCells（只建视口内 StickerCell）→ refresh()
滚动 → recalculateGridColumns → updateCellVisibility → setInViewport（GIF 装卸）
搜索 → onSearchTextChanged → delayedSearch（QTimer）→ performSearch → displayStickers
双击 → onStickerDoubleClicked → 复制 + RecentUsageStore::add
左键 → onStickerClicked → m_highlightManager.setHighlightedPath(path)
右键 → onStickerRightClicked → setHighlightedPath + ImagePreviewDialog(filePath, animatePreview, siblings)
预览切换 → currentFileChanged → onPreviewFileChanged → setHighlightedPath + 滚动到目标 cell
关闭预览 → onPreviewClosed → m_highlightManager.clearHighlight()
窗口 hide → 全 cell setInViewport(false)（卸 GIF）+ ThumbnailCache::clear() → logMemoryState("hidden-released") → 1/5/15/60s `decay+Xs` 采样行
窗口 show → updateCellVisibility()（恢复视口动画）→ logMemoryState("shown")
```

## 依赖关系

- 依赖：ConfigManager、StickerLibrary、ThumbnailCache、AsyncThumbnailLoader、StickerCell、CategoryButton、ImagePreviewDialog、RecentUsageStore
- 被依赖：AppController（appcontroller.cpp `createWindows`）、TrayIcon（showSubMenu）

## 关键规则

- **新设置三处**：`EffectiveSettings` 字段 + `refreshEffectiveSettings()` 填充 + 热路径读 `m_eff`（updateVisibleCells）
- 可见性由 showWindow()/hide() 控制；closeEvent 与 resizeEvent 回写窗口几何
- **隐藏即释放**（`hideEvent`，2026-10-25 量化后落地）：
  - 对全部 cell `setInViewport(false)`（复用 scroll-out 路径卸 QMovie）+ `m_thumbnailCache->clear()`，**不** `clearStickerCells()` —— cell 保留 `m_currentPixmap`，重开零闪烁（量化数据：GIF 类目显示 105.7MB → 隐藏 35.2MB，重开 +6s 恢复且 `cellPixKB` 不变）
  - 大头是 GIF `QMovie`（整文件 QBuffer + 解码帧），缩略图缓存次之（≤10MB）；`showEvent` 里 `updateCellVisibility()` 恢复视口动画
  - `logMemoryState()` 在 hide/show 输出 `[mem]` 行（cells/cellPixKB/movies/catBtn/cache + `mCreated/mDestroyed` QMovie 计数 + `privMB/wsMB`），Info 级别；hide 释放后另有 1/5/15/60s `decay+Xs` 采样行（判回落 vs 钉死）；`processMemoryUsage()` 助手用 `GetProcessMemoryInfo`（需 `<windows.h>` 在 `<psapi.h>` 之前）
- Recent 伪分类：空隐藏、首用现场创建、右键清空（见 core-recent）
- 窗口标题由 AppController 设 `"Stickers Manager - <dirName>"`（appcontroller.cpp:79）
- **高亮管理**：HighlightManager 路径驱动，单一状态源 `m_highlightedPath`
  - `setHighlightedPath(path)` 设置路径并 refresh 所有 cell
  - `clearHighlight()` 清空路径并 refresh
  - `refresh()` 遍历 `m_cellMap`，路径匹配 → `setHighlighted(true)`，否则 `setHighlighted(false)`
  - 调用时机：左键点击、右键打开预览、预览切换上下张、updateVisibleCells 新建 cell 后
  - `onPreviewFileChanged` 先按索引滚动到目标行，再设置高亮路径（解决虚拟滚动 cell 不在 map 的问题）
  - 关闭预览时调用 `clearHighlight()`
- **高亮样式**：`StickerCell::setHighlighted` 用 `QApplication::palette().color(QPalette::Highlight)` 取色（非 palette()，避免不一致）
- `mousePressEvent` 不做高亮，仅发射信号

## 变更记录索引

- 2026-10-25：隐藏即释放（hideEvent 卸 GIF + 清缩略图缓存）+ `[mem]` 内存埋点（hideEvent/showEvent）
- 2025-01-14：docs/.ai Init（首次记录）
