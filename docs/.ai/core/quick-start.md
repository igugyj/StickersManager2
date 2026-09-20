# StickersManager 快速开始

## 环境要求

- **系统**：Windows（win32 目标）
- **工具链**：CMake ≥3.5、MinGW（与 Qt 包 ABI 匹配，如 13.1.0）
- **Qt**：6.10.1 mingw_64，默认 `D:/Qt/6.10.1/mingw_64`（CMakeLists.txt:15 `QT_PATH` 缓存变量）

## 安装步骤

```bash
cd D:/programing/Cpp/StickersManager

# 1. 配置 + 构建（Debug）
cd build && cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug && cmake --build . -- -j8

# 2. 可选：控制台调试模式（qDebug 直出、无 messageHandler）
cmake .. -DCONSOLE=ON && cmake --build . -- -j8

# 3. 可选：覆盖 Qt 路径
cmake .. -DQT_PATH=D:/Qt/6.10.1/mingw_64
```

## 基本命令（全部实测）

| 命令 | 功能 |
|---|---|
| `cmake --build . -- -j8` | 构建（windeployqt 自动部署 Qt DLL POST_BUILD） |
| `cmake .. -DCONSOLE=ON` | 切控制台模式（WIN32_EXECUTABLE FALSE + LOG_TO_CONSOLE） |
| `cmake ..`（改源码清单后） | 重新配置以纳入新 globbed 文件 |
| `scripts/clean_release.py` | 清 release/ 构建残留（本地手工打包用） |
| `iscc /DMyAppVersion=2.7.1 pkg.iss` | 本地打安装包（Inno Setup） |

## 开发流程

1. 改/加 `src/**/*.cpp/.h/.hpp`（新文件重跑 `cmake ..`）
2. 加设置项 → `hardDefaults()` / `defaultSettings()` 单源 → getter → `EffectiveSettings` + `refreshEffectiveSettings()`
3. 构建：`cmake --build . -- -j8`（exe 在 `build/StickersManager.exe`）
4. 运行：首次启动无库会弹设置对话框，添加库后托盘常驻
5. 热重载测试：改配置后托盘右键 Rescan（fullReload）；设置页 Save & Reload（applyChanges）

## 调试技巧

- 控制台模式：`cmake .. -DCONSOLE=ON` → qDebug 直达控制台（DEBUG_MODE=true 不装 messageHandler）
- 日志：`log/log.log`（相对工作目录，每次启动截断）
- 托盘消息：worker 线程用 `notifyTray()`（QueuedConnection）
- 热键调试：启动日志打 `Global input listener is running with N hotkeys`
- 配置调试：看 `[EXE_DIR]/.stickersmanager/config.json`（mergeDefaults 仅内存归一化，不回写）

## 常见问题

- **exe 找不到**：MinGW Makefiles 单配置，产物在 `build/` 顶层，不是 `build/Release`
- **新文件未编译**：GLOB_RECURSE 需重跑 `cmake ..`
- **图标缺失告警**：assets/st.ico 或 icon.rc 不在 → WARNING，用默认图标
- **windeployqt 未跑**：CMake 缓存里找不到 windeployqt → 检查 CMAKE_PREFIX_PATH/QT_PATH
- **双击无反应**：检查 `doubleClickTarget`（settings.json），旧版目录名仍兼容匹配

## 下一步

- [模块文档](../modules/)
- [配置文档](../config/)
- [最佳实践](../best-practices/)
