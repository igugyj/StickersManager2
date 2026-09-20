# settings.json 配置

## 配置位置

`[EXE_DIR]/.stickersmanager/settings.json`（与 config.json 同目录）

## 关键配置项

| 配置项 | 默认值 | 作用/后果 |
|---|---|---|
| `doubleClickTarget` | `"settings"` | 托盘双点目标：`"settings"` / `"first-library"` / 库 id 字符串 / 旧版 dirName |
| `searchDelayMs` | `300` | 搜索防抖毫秒数 |
| `thumbnailCacheSize` | `200` | 缩略图 LRU 缓存上限 |
| `checkForUpdatesOnStartup` | `true` | 启动查 GitHub 更新 |
| `startWithWindows` | `false` | 开机自启 |

## 修改注意

- 默认值单源：`defaultSettings()`（configmanager.cpp:58），getter 无 fallback 参数
- `doubleClickTarget` 匹配顺序（main.cpp:230-272）：`"settings"` → `"first-library"`/空 → 库 id → 旧版目录名（QFileInfo::fileName）
- 读走 `loadSettings()`（main.cpp 两条重载路径都先 loadSettings）
- 无自动保存：Settings "Save & Reload" 才写盘

## 环境变量

无（纯配置文件）
