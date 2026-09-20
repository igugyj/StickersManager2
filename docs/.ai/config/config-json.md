# config.json 配置

## 配置位置

`[EXE_DIR]/.stickersmanager/config.json`（applicationDirPath 相对，非用户可移植目录）

## 结构

```json
{
  "default": { "ui": {}, "behavior": {}, "window": {}, "performance": {} },
  "libraries": [ { "id": 0, "path": "D:/stickers", "hotkey": "Ctrl+Shift+A", "enabled": true, "settings": {} } ]
}
```

## 关键配置项

| 配置项 | 默认值 | 作用/后果 |
|---|---|---|
| `default.window.position` | `[900, 50]` | 默认窗口位置 |
| `default.window.size` | `[540, 430]` | 默认窗口大小 |
| `default.window.alwaysOnTop` | `true` | 窗口置顶 |
| `default.ui.categoryButtonSize` | `90` | 分类按钮尺寸 |
| `default.ui.gridCellSize` | `120` | 网格单元格尺寸 |
| `default.ui.gridColumns` | `3` | 网格列数 |
| `default.ui.recentLimit` | `100` | Recent 显示上限 |
| `default.ui.recentEnabled` | `true` | Recent 功能总开关 |
| `default.behavior.copyOnDoubleClick` | `true` | 双击复制 |
| `default.behavior.highlightOnClick` | `true` | 单击高亮 |
| `default.behavior.animateThumbnails` | `false` | 网格 GIF 动画 |
| `default.behavior.animatePreview` | `true` | 预览 GIF 动画 |
| `default.behavior.showFileTypeTag` | `true` | 显示文件类型标签 |
| `default.behavior.showStickerName` | `true` | 显示贴纸名 |
| `default.behavior.showStickerSize` | `true` | 显示贴纸大小 |
| `default.behavior.showCategoryName` | `true` | 显示分类名 |
| `default.behavior.showCategoryCount` | `true` | 显示分类计数 |
| `default.performance` | `{}` | 性能（当前空） |
| `libraries[].id` | 数组序 | **顺序标记**，0..n-1 连续；`getLibraries()` 按 id 排序 |
| `libraries[].enabled` | `true` | **仅热键开关**：只影响热键注册/冲突计算 |
| `libraries[].settings` | `{}` | 库级覆盖（override 体系） |

## 修改注意

- **无版本字段**；缺失/类型损坏的键在 loadConfig 时经 `mergeDefaults` 取硬默认（仅内存，不回写）
- **默认值单源**：改默认只改 `hardDefaults()`（configmanager.cpp:10），勿在 getter/UI 写死
- 拖拽重排 Libraries 标签页会**重编号所有 id**
- 老配置缺 id：加载时按数组位置补
- `enabled=false` **不**影响窗口/托盘/菜单/双击目标/统计，只关热键

## 环境变量

无（纯配置文件）
