# 图像与缩略图模块

## 职责

解码图像（主 stb_image、兜底 QImageReader），异步生成缩略图（QtConcurrent + QThreadPool），LRU 缓存，GIF 动画生命周期管理。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/core/imageloader.h/.cpp` | 解码：stb_image 主、QImageReader 兜底 |
| `src/core/asyncthumbnailloader.h/.cpp` | AsyncThumbnailLoader：QtConcurrent 异步缩略图 |
| `src/core/thumbnailcache.h/.cpp` | ThumbnailCache：LRU QCache（默认 200）；`stats()` 返回条目数 + 估算字节（key→bytes 台账按 `size()` 剪除淘汰键，仅供诊断） |
| `src/ui/stickercell.cpp` | `setInViewport()`：滚入 loadAnimation / 滚出 unloadAnimation |
| `src/ui/imagepreviewdialog.cpp` | 全尺寸预览，WA_DeleteOnClose 释放 dialog+movie |

## 数据流

```
文件 → AsyncThumbnailLoader（worker 线程解码+缩放）→ ThumbnailCache（LRU）→ onThumbnailLoaded → StickerCell::setPixmap
GIF：滚入 → loadAnimation（QMovie+QBuffer+CacheNone，buffer 以 movie 为 parent）→ 滚出 → unloadAnimation（零内存）
预览：右键 → ImagePreviewDialog（async 同模式）
```

## 依赖关系

- 依赖：QtConcurrent、QThreadPool、stb（thirdparty/stb）
- 被依赖：MainWindow、ImagePreviewDialog

## 关键规则

- GIF 判定：扩展名 `.gif` 或 `QImageReader::imageCount() > 1`
- **内存量化结论（2026-10-25）**：后台内存大头是 GIF `QMovie`（`loadAnimation` 把**整个 GIF 文件**读进 QBuffer，实测单部 1.2–2.1MB + 解码帧），不是缩略图缓存（实测 0.9–4.9MB）；GIF 类目 12 部播放即从 35MB 涨到 ~150MB，且旧版隐藏后完全不回落 → 现由 `MainWindow::hideEvent` 卸载
- 量化埋点（长期保留）：`StickerCell::pixmapBytes()/isAnimationLoaded()`、`CategoryButton::pixmapBytes()`、`ThumbnailCache::stats()`、`MainWindow::logMemoryState()`（`[mem]` Info 行）
- QBuffer 必须 outlive movie：设为 movie parent
- 动画开关：`animateThumbnails`（网格）/ `animatePreview`（预览）两个独立设置
- ImagePreviewDialog 双栏布局：顶部信息栏（文件名/格式/尺寸/大小/日期/类目）、中部图片+左右hover箭头
- 关闭规则：ESC + 右键；左键无效果（子控件自行处理）；Enter/Space 不再关闭；关闭时清除高亮
- 左右导航箭头默认隐藏，鼠标进入图片区左右10%区域时显示对应按钮（QApplication 级 eventFilter）
- GIF 播放状态由 animatePreview 设置控制，无手动切换按钮
- 预览切换上下张时高亮跟随由 HighlightManager 统一管理（currentFileChanged 信号）

## 变更记录索引

- 2026-10-25：内存量化（stats/pixmapBytes 埋点）+ 隐藏时清缓存/卸 GIF 的释放策略
- 2025-01-14：docs/.ai Init（首次记录）
