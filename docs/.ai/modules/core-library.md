# StickerLibrary 模块

## 职责

扫描库目录，构建分类与贴纸列表，提供文件名搜索索引与统计。**仅直接子目录**为分类（非递归）。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/core/stickerlibrary.h/.cpp` | `scanLibrary()`、搜索索引、分类结构 |
| `src/utils/fsutil.hpp` | `forEachStickerFile`（模板，驱动扫描与统计）、`isPreviewFile()`（:26） |

## 数据流

```
库目录 → forEachStickerFile（遍历）→ isPreviewFile 过滤 → 分类（直接子目录）+ 小写文件名索引
搜索：lowercase 索引匹配 → QVector<QString> → MainWindow::displayStickers
统计：同一 forEachStickerFile 驱动 librarysettingspage.cpp 结构统计
```

## 依赖关系

- 依赖：fsutil.hpp（header-only）
- 被依赖：MainWindow（loadLibrary）、LibrarySettingsPage（统计）

## 关键规则

- preview 过滤：文件名以 `.preview` 开头或含 `.preview.` → 排除出扫描/搜索/统计
- 共享遍历：`forEachStickerFile` 模板同时服务 `scanLibrary()` 与统计，改动需同步两处调用方

## 变更记录索引

- 2025-01-14：docs/.ai Init（首次记录）
