# RecentUsage 模块

## 职责

按库记录最近使用贴纸（双击复制时），驱动 Recent 伪分类：空时隐藏、首用时现场创建、可清空、显示上限可配。

## 关键文件

| 文件路径 | 作用 |
|---|---|
| `src/core/recentusage.h/.cpp` | `RecentUsageStore`：读写 `recent_<md5(libPath)>.json`、`add`/`paths`/`clear`、上限截断、旧文件清理 |
| `src/ui/mainwindow.cpp` | `populateCategories` 注入 Recent、`refreshRecentButton`、`onStickerDoubleClicked` 记录、右键清空菜单 |
| `src/ui/clockicon.cpp` | `makeClockIcon()`：时钟预览图标（`:/assets/Clock - 24x24.png` SourceIn 染色） |
| `src/ui/categorybutton.cpp` | `setShowClock()` 应用时钟图标 |

## 数据流

```
双击复制 → RecentUsageStore::add（新条目在头部，上限 recentLimit=100）→ refreshRecentButton → 无 Recent 则 populateCategories 创建
Recent 右键 "Clear Recent Records" → RecentUsageStore::clear()（删文件）→ repopulate 隐藏
recentEnabled=false → 分类隐藏 + 记录跳过 + showCategory 无操作；文件保留，重开即恢复
```

## 依赖关系

- 依赖：MainWindow、CategoryButton、clockicon
- 被依赖：无（叶子功能）

## 关键规则

- 显示上限 = `recentLimit`（`default.ui`，默认 100，可库级覆盖），在 `paths()` 应用
- 计数标签实时刷新（refreshRecentButton）
- 记录仅发生在**双击复制**成功路径（`onStickerDoubleClicked`）

## 变更记录索引

- 2025-01-14：docs/.ai Init（首次记录）
