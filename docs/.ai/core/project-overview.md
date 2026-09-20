# StickersManager 项目概述

## 基本信息

- **名称**：StickersManager
- **位置**：`D:\programing\Cpp\StickersManager`
- **类型**：Windows 桌面贴纸管理器（托盘常驻、单实例、多窗口）
- **技术栈**：C++20 + Qt 6.10.1（Core/Gui/Widgets/Concurrent/Network）+ MinGW + stb_image

## 项目类型说明

- 纯桌面 GUI 应用，无后端服务；配置存本地 JSON（`[EXE_DIR]/.stickersmanager/`）
- 单实例：`QLockFile` 于 `%TEMP%/<user>_Stickers Manager.lock`（main.cpp:48）
- 多窗口：一个库一个 `MainWindow`（与热键开关无关），标题 `"Stickers Manager - <dirName>"`
- 首启动无有效库 → 模态 SettingsDialog 阻塞（main.cpp:201-207）
- Fusion 风格（main.cpp:58），托盘驻留 `setQuitOnLastWindowClosed(false)`
- 图标：`assets/st.png`（qrc 资源，`:/assets/st.png`），窗口图标 `assets/st.ico`（icon.rc）

## 技术栈

| 技术 | 版本 | 用途 |
|---|---|---|
| C++ | 20 | 语言标准（`CMAKE_CXX_STANDARD 20`） |
| Qt | 6.10.1（mingw_64） | GUI 框架，链接 Core/Gui/Widgets/Concurrent/Network |
| MinGW | 13.1.0（CI Qt-bundled 工具链） | 编译器，`-G "MinGW Makefiles"` 单配置 |
| stb_image | 单头文件 | 主图像解码（imageloader.cpp），QImageReader 兜底 |
| CMake | ≥3.5（本机 3.30.5） | 构建 |
| Inno Setup | 6/7 | 安装包（pkg.iss） |
| GitHub Actions | — | 发布流水线（.github/workflows/release.yml） |

## 目录结构

```
StickersManager/
├── CMakeLists.txt       # CONSOLE 开关、QT_PATH、windeployqt POST_BUILD
├── resources.qrc
├── pkg.iss              # Inno Setup 脚本（路径相对脚本目录）
├── CHANGELOG.md         # 发布说明（顶部为版本单源）
├── AGENTS.md            # agent 工作指令
├── assets/              # st.ico、st.png、menu.qss、icon.rc
├── src/
│   ├── main.cpp         # 入口：锁、窗口生命周期、热键、重载路径
│   ├── appinfo.h        # AppInfo：版本/作者/许可/仓库 URL
│   ├── core/            # config、library、image、cache、input、update、recent
│   ├── ui/              # mainwindow、tray、dialogs、cells、settings pages
│   └── utils/           # log.hpp、launcher.hpp、fsutil.hpp（header-only static）
├── thirdparty/stb/      # stb_image.h、stb_image_resize2.h
├── build/               # 构建产物（exe 直接在 build/）
├── release/             # 本地发布组装目录
├── scripts/clean_release.py
└── docs/.ai/            # 本项目 AI 文档
```

## 关键功能

- **贴纸库**：每库 = 本地文件夹；**仅直接子目录**为分类（非递归）
- **全局热键**：Win32 `SetWindowsHookEx(WH_KEYBOARD_LL)`（GlobalInputListener），每库可绑，冲突比较 `ShortcutCompare`
- **Recent 分类**：双击复制时记录，时钟图标（clockicon.cpp），空时隐藏，上限 `recentLimit`
- **搜索**：贴纸按预建小写文件名索引；分类按名；`searchDelayMs` 防抖
- **复制/预览**：双击复制；右键 `ImagePreviewDialog` 全尺寸预览
- **GIF 动画**：滚入 `loadAnimation()` / 滚出 `unloadAnimation()`，离屏零内存
- **override 体系**：库 settings 覆盖 `default{}` → `hardDefaults()`
- **热重载**：托盘 Rescan = `fullReload`（重建+重扫）；设置页 Save = `applyChanges`（只更新配置）
- **更新检查**：UpdateChecker 查 GitHub API，`checkForUpdatesOnStartup` 门控

## 重要配置

- `config.json`：`{"default":{ui,behavior,window,performance},"libraries":[{id,path,hotkey,enabled,settings:{}}]}`；无版本字段；`id` 为顺序标记（0..n-1 连续）
- `settings.json`：`doubleClickTarget`（默认 "settings"）、`searchDelayMs`（300）、`thumbnailCacheSize`（200）、`checkForUpdatesOnStartup`（true）、`startWithWindows`（false）
- 默认值单源：`hardDefaults()`（configmanager.cpp:10）+ `defaultSettings()`（configmanager.cpp:58）；getter 无 fallback 参数
- 无自动保存：显式 save 才写盘

## 开发命令

| 命令 | 功能 |
|---|---|
| `cd build && cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug && cmake --build . -- -j8` | 配置+构建 |
| `cmake .. -DCONSOLE=ON` | 控制台调试（qDebug 直出 + 无 messageHandler） |
| `cmake .. -DQT_PATH=<path>` | 覆盖 Qt 路径（CI 传 `${{ env.Qt6_DIR }}`） |
| 新增源文件后重跑 `cmake ..` | GLOB 需重新配置才纳入 |

## 开发规范

- 全源 globbed：新文件重跑 cmake
- 无 `.ui` 文件：UI 全 C++ 代码；AUTOMOC/AUTORCC/AUTOUIC ON
- header-only 工具：`static` 函数（每 TU 一份）
- 线程→GUI 通信：`notifyTray()` QueuedConnection
- 版本手动 bump（appinfo.h:7），仅发布时
- 无测试/CI 门禁/linter

## 已知问题

- MinGW Makefiles 单配置：exe 在 `build/` 顶层，非 `build/Release`（CI 有组装步骤）
- 手工打包前 `scripts/clean_release.py` 清 release/ 残留（CI 不需要）
- 日志 `log/log.log` 相对工作目录，启动截断
