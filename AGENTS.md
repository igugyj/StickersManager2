# StickersManager — AGENTS.md

## Build & config
```bash
cd build && cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug && cmake --build . -- -j8
```
- Qt 6.10.1 MinGW, `QT_PATH` cache variable default `D:/Qt/6.10.1/mingw_64` (`CMakeLists.txt:13-14`), overridable via `-DQT_PATH=...` (CI: `-DQT_PATH=${{ env.Qt6_DIR }}`); `CMAKE_PREFIX_PATH` set from it, windeployqt POST_BUILD follows it
- `CMakeLists.txt` links `Qt6::Core Gui Widgets Concurrent Network`; new source files need re-run `cmake ..` to be picked up (`FILE(GLOB_RECURSE ./src/)` for `.cpp/.h/.hpp`)
- `CONSOLE` is a **plain variable** (`CMakeLists.txt:5`, source = switch, default OFF): ON → console window (`WIN32_EXECUTABLE FALSE`) + `add_compile_definitions(CONSOLE)` (activates `LOG_TO_CONSOLE` in `log.hpp`) + `DEBUG_MODE` follows it in `main.cpp:24-28` (true = no `messageHandler`, qDebug to console); subsystem is set once via target property, no `WIN32` flag duplication. C++20, `UNICODE`/`_UNICODE` defined
- `windeployqt` auto-deploy runs POST_BUILD on every build (`CMakeLists.txt:103-113`); C++20, `UNICODE`/`_UNICODE` defined

## CI / Release (`.github/workflows/release.yml`)
- Manual `workflow_dispatch` → builds Release with **MinGW** (Qt 6.10.1 `win64_mingw` via `jurplel/install-qt-action@v4` with **Qt-bundled** toolchain `tools: 'tools_mingw1310,qt.tools.win64_mingw1310'` — MinGW 13.1.0, must match the Qt package's ABI; `add-tools-to-path` (default on) puts `Tools\mingw1310_64\bin` on PATH, Configure/Build prepend `$env:QT_ROOT_DIR\bin` (the action's Qt-root env var, **not** `Qt6_DIR` which v4.3.1 no longer sets); `-G "MinGW Makefiles"` single-config), then packages: 7z portable + **Inno Setup** installer + release notes, auto-creates tag `vX.Y.Z`, uploads GitHub Release
- MinGW Makefiles put the exe in `build/` (no `build/Release`) — build job runs an "Assemble Release directory" step (filters `CMakeCache.txt/cmake_install.cmake/Makefile/CMakeFiles`, moves the rest into `build/Release/`) so the upload path stays `build/Release`; build step prepends `$env:Qt6_DIR\bin` + mingw bin to PATH (windeployqt needs objdump)
- Version single source = **top section of `CHANGELOG.md`** (`# Changelog — vA → vB`); parsed by `scripts/make_release_notes.ps1` (also generates `release_notes.md` with body + `### Packaging` + `**Full Changelog**` compare link; uses section date, else today)
- Installer: `pkg.iss` (Inno Setup) compiled by ISCC with `/DMyAppVersion=X.Y.Z` — CI: `choco install innosetup` then locate ISCC.exe dynamically (PATH, then `Program Files (x86)/Inno Setup 6`, `Program Files/Inno Setup 6`, `Program Files/Inno Setup 7`); paths inside pkg.iss are **relative to the script dir** (LICENSE, README.md, `assets/st.ico`, `OutputDir=.`, sources from `release/`); output `StickersManager_<ver>.exe` at repo root; start-menu shortcut always created, desktop icon = unchecked Task
- CI payload = `build/Release` + a "Add docs to payload" step copies `README.md` + `LICENSE` into `release/` (they ship in both 7z and installer); `scripts/clean_release.py` is **not needed on CI**
- `pkg.iss` also used for the **local manual** flow (iscc with default `#define MyAppVersion "2.7.1"` — e.g. `D:\Program Files\Inno Setup 7\ISCC.exe /DMyAppVersion=2.7.1 pkg.iss`)

## Architecture
- **Single instance**: `QLockFile` at `%TEMP%/<user>_Stickers Manager.lock` (`main.cpp:48`)
- **Config** (`[EXE_DIR]/.stickersmanager/`, relative to exe):
  - `config.json`: `{"default":{ui,behavior,window,performance},"libraries":[{id,path,hotkey,enabled,settings:{}}]}` — no version field; `id` is the **order marker** (0..n-1, contiguous): `getLibraries()` sorts by id (`configmanager.cpp:196-217`), missing ids (legacy) get array position, drag-reorder in the Libraries tab renumbers ids; `enabled` = **hotkey switch only** ("Enable Hotkey" checkbox) — does NOT affect window/tray/menu/double-click target/storage stats, only global-hotkey registration and hotkey-conflict computation
  - `settings.json`: `doubleClickTarget` (default `"settings"`; `"first-library"`/library id string; legacy dirName/path values still matched as fallback, `main.cpp:236-264`), `searchDelayMs` (300), `thumbnailCacheSize` (200), `checkForUpdatesOnStartup` (true), `startWithWindows` (false)
  - Defaults are **single-sourced**: every default value lives only in `hardDefaults()` (`configmanager.cpp:10`); `loadConfig()` merges the loaded `default` block against it (`mergeDefaults`, in memory only, never written back — missing or type-corrupted keys get the hard default), and getters take **no fallback argument**. Change a default in `hardDefaults()` (or `defaultSettings()` for settings.json), nowhere else
  - `recent_<md5(libraryPath)>.json`: per-library **recently-used** list `{"entries":[{path,time}]}` (storage capped 100, newest first, stale files pruned on load); written by `RecentUsageStore` (`src/core/recentusage.cpp`) on double-click copy; shown as the first "Recent" pseudo-category in `MainWindow::populateCategories()` — **hidden entirely when empty**, created live on first use (`onStickerDoubleClicked` → `refreshRecentButton()` returns false → repopulate), and hidden entirely when disabled via `recentEnabled` (`populateCategories` gate + `showCategory("Recent")` guard + record skipped in `onStickerDoubleClicked`); right-click on it shows a "Clear Recent Records" menu (`onCategoryContextMenuRequested`, `CustomMenu` style, `RecentUsageStore::clear()` deletes the file, then repopulate hides it); its preview icon is a clock (`setShowClock`), drawn from `:/assets/Clock - 24x24.png` tinted via `SourceIn` (`clockicon.cpp` `makeClockIcon()`); display cap = `recentLimit` (`default.ui`, default 100, per-library override) applied in `RecentUsageStore::paths()`; count label refreshed live via `MainWindow::refreshRecentButton()`
- **No auto-save** — config written on explicit `saveConfig()` / `saveSettings()` only (Settings "Save & Reload", `settingsdialog.cpp:66-67`), plus the fallback folder-picker save in `mainwindow.cpp:173` when a window has an empty path
- **First-launch** — if no library with existing path, modal `SettingsDialog tmpDlg.exec()` (`main.cpp:201-207`) blocks before any window/tray
- **Multi-window**: one `MainWindow` per library with non-empty path (regardless of hotkey switch); title `"Stickers Manager - <dirName>"` (`main.cpp:76`)
- **Scanning** (`stickerlibrary.cpp`): **immediate subdirectories** only are categories (non-recursive); `isPreviewFile()` (name starts with `.preview` or contains `.preview.`) files excluded from scan/search/stats (`fsutil.hpp:26`); search matches against a prebuilt lowercase-filename index; the directory walk is shared — `fsutil::forEachStickerFile` (template) drives both `StickerLibrary::scanLibrary()` and the library structure stats in `librarysettingspage.cpp`
- **Global hotkeys**: Win32 `SetWindowsHookEx(WH_KEYBOARD_LL)` via `GlobalInputListener`; key↔string mapping + `ShortcutCompare::compareShortcutKeys` in `convertcodetostring.hpp`; listener auto-starts/stops when hotkey map empties (`main.cpp:120-132`)
- **Two runtime reload paths** (don't confuse them):
  - `fullReload` (`main.cpp:134`) — tray **Rescan**: reloads config, removes stale windows, creates new ones, rescans all libs via `reloadLibrary()`
  - `applyChanges` (`main.cpp:146`) — settings dialog `applied` signal: updates per-window `LibraryConfig` + reapplies settings, **no rescan**
- **Image loading**: `stb_image` (primary), `QImageReader` (fallback) in `imageloader.cpp`
- **Async thumbnails**: `QtConcurrent` + `QThreadPool` via `AsyncThumbnailLoader`; LRU `QCache` in `ThumbnailCache`
- **Startup update check**: `UpdateChecker` hits GitHub API (`apiReleasesUrl()`), gated by `checkForUpdatesOnStartup`

## Override system
- `getEffectiveXxx(lib)` — checks `lib.settings[cat][key]` via `effVal()`, falls back to `default[cat][key]` (normalized at load, see Config), then `hardDefaults()`
- **MainWindow caches** its effective settings in `EffectiveSettings m_eff` (struct in `mainwindow.h`), refreshed by `refreshEffectiveSettings()` on ctor, `applySettings`, and `updateLibraryConfig` — the scroll hot path (`updateVisibleCells`) reads `m_eff`, so a **new setting must be added to both `EffectiveSettings` and `refreshEffectiveSettings()`**, then read via `m_eff.xxx` in hot paths
- 0 = "not overridden" for numeric; "General"/"On"/"Off" for booleans
- `getDefaultXxx()` reads from `default{}` (set by General tab)
- Tag toggles (4 keys in `default.behavior`, default true): `showStickerName`, `showStickerSize`, `showCategoryName`, `showCategoryCount` (General tab + per-library combos)
- `recentLimit` (`default.ui`, default 100, per-library numeric override, 0 = not overridden): recent display cap
- `recentEnabled` (`default.ui`, default true, per-library bool override via General/On/Off combo in the UI overrides group): master switch for the Recent feature — when off (effective), the Recent category is hidden (even with existing records), recording is skipped on double-click copy, and `showCategory("Recent")` is a no-op; records file is kept so re-enabling restores the category

## Animation (GIF)
- Detected by extension `.gif` or `QImageReader::imageCount() > 1`
- `QMovie + QBuffer + CacheNone` — buffer must outlive movie (set as parent)
- `StickerCell::setInViewport()` — calls `loadAnimation()` on scroll-in, `unloadAnimation()` on scroll-out (zero memory for off-screen)
- `ImagePreviewDialog`: same async pattern; `WA_DeleteOnClose` frees dialog+movie on close

## SettingsDialog
- 4-tab `QTabWidget`: General / Libraries / Base / About (`settingsdialog.cpp:33-36`)
- "Save & Reload" (`onSave`, `settingsdialog.cpp:60`): applies all pages, `saveConfig()` + `saveSettings()`, emits `applied` → `applyChanges`; keeps dialog open
- Implemented as `QPointer<SettingsDialog>` singleton with `WA_DeleteOnClose`; tray double-click toggles open/close (`main.cpp:175-190`)
- Shared widget builders live in `src/ui/settingswidgets.h` (`makeSpinBox`, `makeOverrideSpinBox` (0 = "General"), `makeBoolCombo`, `boolToCombo`, `makeInfoButton`) — settings pages include it, never re-declare local copies
- Libraries tab: drag-reorder renumbers ids; hotkey set by key capture (`hotkeycapture.cpp`); Base tab refreshes the double-click-target combo on tab show (`settingsdialog.cpp:40-45`)

## File type tag
- `QLabel` overlay with uppercase extension, positioned absolutely at `(7, cellSize - tagHeight - 7)` (`stickercell.cpp:57`). Not in layout.
- Hidden during placeholder; shown only after real thumbnail; controlled by `getEffectiveShowFileTypeTag()`
- `StickerCell` also overlays file name without extension (top-left `(7,7)`, elided) and file size (bottom-right, via `formatBytes`) — shown immediately, gated by `getEffectiveShowStickerName()` / `getEffectiveShowStickerSize()`
- `CategoryButton` overlays name (bottom-left, elided) and count (bottom-right), gated by `getEffectiveShowCategoryName()` / `getEffectiveShowCategoryCount()`; optional clock as the button **preview icon** (`clockicon.cpp` `makeClockIcon()`, tinted `:/assets/Clock - 24x24.png`) via `setShowClock()` — used only for the Recent category

## Directory layout
```
src/
├── main.cpp
├── appinfo.h          # inline namespace AppInfo — version, author, license, repo/issue URLs
├── core/              # config, library, image loading, cache, input, keycode map, update checker, recent usage
├── ui/                # mainwindow, tray, dialogs, cells, category buttons, custommenu, hotkey capture, settings pages
└── utils/             # log.hpp, launcher.hpp, fsutil.hpp (all header-only `static`)
assets/                # icons, menu.qss, icon.rc
scripts/clean_release.py   # deletes build artifacts from release/ (edit RUN/DIRS at top)
thirdparty/stb/        # stb_image.h, stb_image_resize2.h
```

## Conventions
- **All source globbed**: new files need re-run `cmake ..` to be picked up
- **No `.ui` files**: `AUTOUIC ON` but all UI in C++ code; `CMAKE_AUTOMOC` + `AUTORCC` also ON
- **Style**: Fusion (`main.cpp:53-55`)
- **Logging**: `log.hpp` writes to `log/log.log` relative to working dir, truncated on each start; `DEBUG_MODE` (`main.cpp:24-28`) — driven by `CONSOLE` compile definition (`-DCONSOLE=ON`): `false` installs `messageHandler`, `true` shows `qDebug()` in console
- **Version**: `appinfo.h:7` — `AppInfo::version()` returns `"2.7.1"`, manual bump only on release
- **About page**: version comparison strips `v` prefix, parses `major.minor.patch` numerically (`UpdateChecker::compareVersions`)
- **Header-only utils**: `static` functions in `log.hpp` / `launcher.hpp` / `fsutil.hpp` (one copy per TU). `launch()` opens paths/URLs via `QtConcurrent::run` + `QDesktopServices`; `notifyTray()` marshals tray messages from worker threads to the GUI thread (`QueuedConnection`)
- **No tests / CI / linters**; packaging via `pkg.iss` (Inno Setup); release artifacts (`StickersManager_<ver>.exe/.7z`) at repo root are **gitignored** (`*.exe`/`*.7z`) — local only, never committed