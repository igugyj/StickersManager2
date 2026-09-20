# 部署与发布最佳实践

## 构建产物位置

- 本地 Debug/Release：`build/StickersManager.exe`（MinGW Makefiles 单配置，**无** build/Release 层）
- windeployqt 自动部署：POST_BUILD 跑（CMakeLists.txt:98-107），Qt DLL/插件进 build/

## 本地手工打包（Inno Setup）

```powershell
# 1. 组装 release/（含 README.md + LICENSE 副本）
# 2. 清构建残留
python scripts/clean_release.py
# 3. 编译安装包（版本号硬编码示例）
"D:\Program Files\Inno Setup 7\ISCC.exe" /DMyAppVersion=2.7.1 pkg.iss
```

- pkg.iss 内路径**相对脚本目录**：LICENSE、README.md、assets/st.ico，OutputDir=.
- 产物：`StickersManager_<ver>.exe`（仓库根，gitignored）
- 开始菜单快捷方式总是创建；桌面图标默认不勾选（Task）

## CI 发布（.github/workflows/release.yml）

- 手动 `workflow_dispatch` 触发
- Qt 6.10.1 win64_mingw（jurplel/install-qt-action@v4），**Qt-bundled 工具链** `tools_mingw1310,qt.tools.win64_mingw1310`（MinGW 13.1.0，ABI 必须匹配 Qt 包）
- 关键点：
  - Qt 根 env 变量是 `QT_ROOT_DIR`（v4.3.1 不再设 `Qt6_DIR`）
  - 配置传 `-DQT_PATH=${{ env.Qt6_DIR }}`（workflow 内映射）
  - Configure/Build 步骤 PATH 前置 `$env:QT_ROOT_DIR\bin` + mingw bin（windeployqt 需要 objdump）
  - "Assemble Release directory" 步骤：滤掉 CMakeCache/cmake_install/Makefile/CMakeFiles，其余移入 `build/Release/`（上传路径保持 build/Release）
  - 7z portable + Inno 安装包 + release notes
- 版本单源：`CHANGELOG.md` 顶部 `# Changelog — vA → vB`；`scripts/make_release_notes.ps1` 解析生成 release_notes.md
- 安装包：CI 用 `choco install innosetup`，ISCC.exe 动态定位（PATH → Program Files (x86)/Inno Setup 6 → Program Files/Inno Setup 6 → Program Files/Inno Setup 7）
- 自动打 tag `vX.Y.Z`，传 GitHub Release

## 版本流程

1. `src/appinfo.h:7` 手动 bump 版本号
2. `CHANGELOG.md` 顶部加版本段（含日期，缺则用今天）
3. CI dispatch 或本地 iscc 打包
4. 产物 gitignored，不提交

## 违例与后果

- 上传路径假设 build/Release 存在 → MinGW 单配置无此层，必须走组装步骤
- MinGW 版本与 Qt 不匹配 → 链接/运行 ABI 错误；CI 用 Qt-bundled 工具链规避
