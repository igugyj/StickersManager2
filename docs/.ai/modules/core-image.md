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
- **QMovie 生命周期红线（2026-10-25 泄漏修复）**：`m_movie` 与 QBuffer 互为 parent（环）且**不挂在 cell 下** → `StickerCell` 析构**必须走 `unloadAnimation()`**（`delete m_movie` 连带析构 child QBuffer 断环），绝不能只 `stop()` —— 旧版析构只 `stop()`，切类目 `clearStickerCells()`/滚动剪枝 `delete cell` 即整环泄漏（QMovie+整文件 QBuffer+解码器 ≈3.3MB/个）；5 轮实验 leaked=96、priv 线性 +60MB/轮、隐藏后 315MB 平线；修复后 leaked=0、浏览稳定 ~64MB、隐藏回落 34MB
- 量化埋点（长期保留）：`StickerCell::pixmapBytes()/isAnimationLoaded()`、`CategoryButton::pixmapBytes()`、`ThumbnailCache::stats()`、`MainWindow::logMemoryState()`（`[mem]` Info 行，含 `mCreated/mDestroyed` QMovie 计数 + `privMB/wsMB`，hide 后另有 `decay+1/5/15/60s` 采样）、`animationMoviesCreated()/Destroyed()`（stickercell.cpp 全局原子计数）
- QBuffer 必须 outlive movie：设为 movie parent
- **首屏 GIF 播放已验证正常**（2026-10-25）：`setThumbnail()` 在 `m_animateEnabled && isAnimated && m_inViewport`（默认 `m_inViewport=true`）时直接 `loadAnimation()`（stickercell.cpp:93）—— 点进 GIF 类目不滚动 12s 实测 `movies=9`；曾怀疑的"要滚出再滚回才播"是首屏类目恰好无 GIF 的误判，**无此 bug、代码未改**
- 动画开关：`animateThumbnails`（网格）/ `animatePreview`（预览）两个独立设置
- ImagePreviewDialog 双栏布局：顶部信息栏（文件名/格式/尺寸/大小/日期/类目）、中部图片+左右hover箭头
- 关闭规则：ESC + 右键；左键无效果（子控件自行处理）；Enter/Space 不再关闭；关闭时清除高亮
- 左右导航箭头默认隐藏，鼠标进入图片区左右10%区域时显示对应按钮（QApplication 级 eventFilter）
- GIF 播放状态由 animatePreview 设置控制，无手动切换按钮
- 预览切换上下张时高亮跟随由 HighlightManager 统一管理（currentFileChanged 信号）

## 变更记录索引

- 2026-10-25（三）：修复 `~StickerCell()` 只 `stop()` 不 `delete` 的 QMovie 环泄漏（析构改走 `unloadAnimation()`）；新增 `mCreated/mDestroyed` 计数 + `privMB/wsMB` + decay 采样埋点（见 changelog）
- 2026-10-25（二）：GIF 首屏播放复核实证（非 bug）+ `check_version.ps1` 版本一致性校验（见 changelog）
- 2026-10-25：内存量化（stats/pixmapBytes 埋点）+ 隐藏时清缓存/卸 GIF 的释放策略
- 2025-01-14：docs/.ai Init（首次记录）
