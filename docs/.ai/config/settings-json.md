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
| `logLevel` | `""`（未设置） | 日志级别；空 = 按构建类型推导（控制台构建 Debug，否则 Warning）；值为 `Debug`/`Info`/`Warning`/`Critical`/`Fatal`，等于构建默认时不落盘 |
| `openSettingsHotkey` | `"Ctrl+I"` | 全局热键：打开/切换设置对话框（录制模式字符串如 `"Ctrl+Shift+E"`；`""` = 未绑定） |
| `openSettingsHotkeyEnabled` | `true` | 上述热键的启用开关（Base 页 "Enable" 复选框） |

## 修改注意

- 默认值单源：`defaultSettings()`（configmanager.cpp:58），getter 无 fallback 参数
- `doubleClickTarget` 匹配顺序（appcontroller.cpp `onTrayActivated`，:280-341）：`"settings"` → `"first-library"`/空 → 库 id → 旧版目录名（QFileInfo::fileName）
- 读走 `loadSettings()`（AppController 两条重载路径都先 loadSettings，appcontroller.cpp）
- 无自动保存：Settings "Save & Reload" 才写盘

## 环境变量

无（纯配置文件）
