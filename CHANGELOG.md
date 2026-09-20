# Changelog — v2.7.0 → v2.8.0 (2026-09-20)

### Added

- `HighlightManager` (path-driven highlight, replaces raw `StickerCell*` pointer)

### Changed

- ConfigManager boilerplate deduplicated; all defaults single-sourced in `hardDefaults()`
- Settings UI helpers consolidated into `src/ui/settingswidgets.h`
- Library scanning shares one walker `fsutil::forEachStickerFile`
- Thumbnail load failures show "Load Failed" placeholder instead of stuck loading
- MainWindow caches effective settings in `EffectiveSettings`
- Dead code removed; image-format/animation caches capped at 4096
- Highlight unified: left/right-click and preview nav all route through `HighlightManager`
- `StickerCell::mousePressEvent` only emits signals; MainWindow owns highlight state
- `StickerCell::setHighlighted` uses `QApplication::palette()` for consistent color
- Preview dialog simplified to two-zone layout
- Preview hover uses `QApplication` eventFilter; `onPreviewFileChanged` scrolls by index
- Recent picture refactor to my drawing, which looks suitable

# Changelog — v2.6.0 → v2.7.0 (2026.08.14)

### Added

- New setting **Enable Recent Usage** (`default.ui.recentEnabled`, default on, General tab checkbox + per-library General/On/Off override): when off, the Recent category is hidden (even with existing records) and usage recording stops; records are kept so re-enabling restores the category

### Changed

- Debug console is now a single CMake option `CONSOLE` (plain variable in `CMakeLists.txt:5`, source = switch, default OFF): enables the console window (`WIN32_EXECUTABLE FALSE`), live qDebug output (`DEBUG_MODE` follows `CONSOLE`), and the `LOG_TO_CONSOLE` macro; subsystem selection is no longer duplicated in `add_executable` and target properties

# Changelog — v2.5.0 → v2.6.0

### Added

- **Category tags**: each category button shows the name (bottom-left) and sticker count (bottom-right) as overlay labels; the "Recent" category uses a clock as its preview icon (only while usage records exist)
- **Sticker tags**: each sticker cell shows the file name without extension (top-left) and file size (bottom-right) as overlay labels; the existing file-type tag (bottom-left) is unchanged
- **Recently used stickers**: a "Recent" pseudo-category appears first in the category panel and is the default view while there are usage records (hidden entirely when empty, created live on first use); per-library (stored in `[EXE_DIR]/.stickersmanager/recent_<md5(path)>.json`), sorted by usage time, recorded on double-click copy, stale files pruned on load; the count label refreshes live after every copy
- New per-library overridable settings in `default.behavior` (General tab, default on): `showStickerName`, `showStickerSize`, `showCategoryName`, `showCategoryCount` — missing/empty keys fall back to the default, per the existing override system
- New display-cap setting `default.ui.recentLimit` (default 100, General tab + per-library override, 0 = General): caps how many recent stickers are shown; storage cap stays 100
- Right-clicking the "Recent" category shows a "Clear Recent Records" context menu item — clears the per-library records file and hides the category again

### Fixes

- Recent category no longer fails to appear when usage records already exist on startup (recents are now loaded before the category panel is built), and appears immediately after the very first copy

# Changelog — v2.3.0 → v2.5.0 (2026.08.14)

### Changed

- Category button tooltips show the sticker count: `categoryName (N)`, refreshed on rescan
- Library cards gain a **Show Structure** toggle (collapsed by default): a summary line
  (category count / total stickers / total size) followed by a full-width zebra-striped table of
  every category (sticker count + whole-folder size, center-aligned cells); scanned asynchronously,
  so large libraries never block the settings dialog
- Base tab gains "Start with Windows" (above the Update group): on save it creates a shortcut in
  the Windows Startup folder (`FOLDERID_Startup`, locale-safe) or removes it; each action is
  written to `log/log.log` via `qDebug`, and the global log level is raised to Debug
- Settings dialog gains a maximize button; its always-on-top state now follows the General
  "Always on Top" default and is re-applied live on every "Save & Reload"
- Duplicated helpers consolidated into `src/utils/fsutil.hpp` (`formatBytes`, `dirSize`,
  `isPreviewFile`); README documents the `.preview*` file filtering feature

### Fixes

- Settings dialog no longer forces itself to stay on top regardless of the Always-on-Top setting

# Changelog — v2.2.1 → v2.3.0 (2026.08.12)

### Changed

- Library config entries gain a numeric `id` as the authoritative **order marker** (0..n-1);
  `getLibraries()` sorts by id, legacy entries without one get their array position, `setLibraries()` persists ids
- **Drag-to-reorder** in the Libraries tab: drag handle (`:::`), live swap preview during drag,
  auto-scroll near the list edges, drop highlight flash; ids renumbered in the new visual order
- Double-click target stored as stable library **id** (was folder name); legacy dirName/path values
  still matched as fallback; the target combo refreshes whenever the Base tab is shown, so
  reordering never leaves a stale target
- The "Enabled" checkbox becomes "Enable Hotkey" — a pure **hotkey switch**: it only gates
  global-hotkey registration and hotkey-conflict calculation. Windows are created for every
  configured library regardless of the switch; the tray Show menu lists all libraries; About
  storage totals include all libraries; the first-launch check keys on configured paths only.
  An empty hotkey string remains a second way to disable a hotkey
- Hotkey conflicts are only considered between hotkey-enabled libraries (checkbox checked and
  non-empty hotkey); live re-validation when the checkbox is toggled; save-time duplicate check
  skips disabled libraries
- Legacy configs: libraries that were unchecked now reappear in window/tray with hotkey disabled

### Fixes

- Fixed tray double-click resolving to a null target when `doubleClickTarget` holds a stale/missing value

# Changelog — v2.1.2 → v2.2.1 (2026.08.03)

### Added

- Set hotkey by capture — press the desired key combination directly in the Hotkey field

### Fixed

- SettingsDialog no longer keeps the always-on-top hint after closing

**Full Changelog**: [v2.1.2...v2.2.1](https://github.com/igugyj/StickersManager2/compare/v2.1.2...v2.2.1)

# Changelog — v2.0.1 → v2.1.2 (2026.08.03)

### Added

- Check for updates on startup (default on) — tray notification only when a newer version exists
- Manual "Check for Updates" button on the Base tab with color-coded status (orange = update available, green = up to date, red = error)
- Per-library card **Reset** button — resets all overrides on the card to defaults
- "Use custom geometry" per library (off by default) with prefilled default values
- Clickable info button explaining the Animate Thumbnails performance warning
- Numeric version comparison (strips `v` prefix, compares `major.minor.patch`)

### Changed

- **Virtualized sticker grid** — cells are created only for visible rows; large categories render instantly and window resizing no longer rebuilds the whole grid
- **Thumbnail cache fixed** — LRU now actually stores entries (previously never inserted); size now means thumbnail count
- Thumbnails decoded downscaled via `QImageReader::setScaledSize`; double scaling on the UI thread removed; animated-file detection memoized
- Thumbnails loaded only for visible cells; scrolled-away cells are destroyed (off-screen GIFs use zero memory)
- Search uses a prebuilt lowercase filename index instead of per-keystroke `QFileInfo`
- Global hotkey listener synced on settings apply — hotkeys work immediately after being added; duplicate hooks prevented
- Config writes are atomic (`QSaveFile`); tray notifications from worker threads are marshalled to the GUI thread
- Per-window grid-column state and preview dialog are no longer shared across windows
- "Save & Reload" keeps the settings dialog open and applies changes live

**Full Changelog**: [v2.0.1...v2.1.2](https://github.com/igugyj/StickersManager2/compare/v2.0.1...v2.1.2)

# Changelog — v1.8.0 → v2.0.1 (2026.07.31)

### Config system (breaking)

- Config v2: flat structure → `{"default":{...},"libraries":[{...,"settings":{}}]}`
- `settings.json` split from `config.json`: stores `doubleClickTarget`, `searchDelayMs`, `thumbnailCacheSize`
- Per-library override system: `getEffectiveXxx(lib)` checks `lib.settings` → `default{}` → hardcoded fallback
- `QSpinBox::setSpecialValueText("General")` (0 = not overridden), boolean combo "General"/"On"/"Off"
- Atomic file write (`.tmp` + rename) for both config files (was direct write)

### Settings dialog (new)

- 4-tab `QTabWidget`: **General** (default values), **Libraries** (override per library), **Base** (app-level), **About**
- Singleton via `QPointer<SettingsDialog>` + `WA_DeleteOnClose`; double‑click tray toggles open/close
- "Save & Reload" calls `saveConfig()` + `saveSettings()` then `fullReload`

### About page (new)

- Displays name, version, author, license, repo/issue links (from `appinfo.h`)
- **Storage**: app directory size + all library files total size (separate)
- **Paths**: executable path (opens folder) + config folder path (opens folder) — clickable
- **Update check**: fetches `api.github.com/repos/.../releases/latest`, strips `v` prefix, compares `major.minor.patch`
- Disk usage computed via `QDirIterator` (recursive)

### Override system (new: 10 `getEffective*` methods)

- `window`: size, position, alwaysOnTop
- `ui`: categoryButtonSize, gridCellSize, gridColumns
- `behavior`: copyOnDoubleClick, highlightOnClick, animateThumbnails, animatePreview, showFileTypeTag

### Application info (new: `src/appinfo.h`)

- Inline namespace `AppInfo` — single source for `version()`, `author()`, `license()`, `repoUrl()`, `issuesUrl()`, `apiReleasesUrl()`
- `"2.0.1"`, `"igugyj"`, `"MIT"`

### First-launch flow (changed)

- Before: `QFileDialog::getExistingDirectory` → save to config
- After: modal `SettingsDialog` (4-tab) blocks before any window/tray

### Tray icon (changed)

- Removed `TrayIcon::Version` (moved to `appinfo.h`)
- Tooltip now `AppInfo::name() + " " + AppInfo::version()`
- Removed `action_openRepo` (was `"Open App Dir"` conditionally); now always shows "Open App Dir"
- Removed GitHub link from tray menu

### Double-click tray behavior (changed)

- Before: always toggles first library window
- After: checks `settings.json:doubleClickTarget` — `"settings"` toggles SettingsDialog, `"first-library"`/dirName toggles window

### Dark theme compatibility (changed)

- `StickerCell` background: hardcoded `#f5f5f5` → `QPalette::Base`
- `StickerCell` highlight: hardcoded → `palette().color(QPalette::Highlight)`
- `CategoryButton` QSS: injected via palette colors instead of `#ffffff` / `#e8f4f8` / `#409eff`
- All plain `QLabel` links: `color: palette(highlight)`

### Window always‑on‑top (changed)

- `WindowStaysOnTopHint` reapplied in `showWindow()` each time (was set once in constructor)
- `showWindow()` reapplies geometry (position + size) every show
- `getEffectiveAlwaysOnTop()` with per-library override

### Highlight & copy (changed)

- `StickerCell::setHighlightEnabled(bool)` / `setCopyOnDoubleClick(bool)` — driven by config
- Highlight on click and copy on double-click are per-library configurable

### Search & cache (changed)

- `searchDelayMs` moved from `config.json` to `settings.json`; read via `m_config->getSearchDelayMs()`
- `thumbnailCacheSize` moved from `config.json` to `settings.json`; per-library via `getEffectiveThumbnailCacheSize()`

### File structure (restructured)

- Source files moved into subdirectories: `src/core/`, `src/ui/`, `src/utils/`
- `CMakeLists.txt`: `GLOB_RECURSE`, linked `Qt6::Network`, `include_directories` for subdirs
- All dialog `QFileDialog::getExistingDirectory` calls: `QFileDialog::DontUseNativeDialog`
- Removed `docs/.ai/`, `GitHub/img.gif`, stale `thirdparty/QtTheme/`

### Code quality

- `reloadLibrary()` now calls `recalculateGridColumns()`
- `populateCategories()` updates category panel width when button size changes
- `onStickerClicked()` clears highlight on other cells
- Version comparison in update check strips `v` prefix, parses numeric (major.minor.patch)
- `QFile`: atomic write pattern for both config files
- `settings.json`: no auto-save (only on explicit `saveSettings()`)

**Full Changelog**: [v1.8.0...v2.0.1](https://github.com/igugyj/StickersManager2/compare/v1.8.0...v2.0.1)

# Changelog — 1.7.0 → v1.8.0 (2026.05.29)

### Added

- File type labels (GIF, PNG, etc.) on stickers in the list
- Setting to show/hide file type labels

### Changed

- Removed unused version configuration for a cleaner project structure
- Updated documentation with a lovely caption
- Unified GitHub contributor username

**Full Changelog**: [1.7.0...v1.8.0](https://github.com/igugyj/StickersManager2/compare/1.7.0...v1.8.0)

# Changelog — ver20260517.8 → 1.7.0 (2026.05.18)

### Added

- Animated sticker support (GIF)

### Changed

- Perf: lazy load/unload animated stickers by scroll viewport
- Async loading for image preview dialog
- Simplified CMake flags
- Updated docs

**Full Changelog**: [ver20260517.8...1.7.0](https://github.com/igugyj/StickersManager2/compare/ver20260517.8...1.7.0)

# Changelog — ver20260501.7 → ver20260517.8 (2026-05-17)

> I learned the rule of version number, this release should be `1.6.0`

### Added

- Category search input on left sidebar — filter category buttons by name, click to jump to the matching category
- Rescan button now hot-reloads config: creates windows for new libraries, rebuilds hotkey map and tray menu without restart

### Changed

- Simplified config JSON format: removed legacy fields (`libraryPath`, `shortcuts`, flat `windowPosition`/`windowSize`, unused `lazyLoadEnabled`); window settings grouped under `window.position`/`window.size`
- Cleaned up ~400 lines of dead code (unused mouse hook, stale methods, duplicate includes)
- Updated all markdown docs to match current codebase
- Refactored the single-instance implementation, no longer occupying the port
- Style restructured to Fusion, consistent across platforms

Note: old config files are still read correctly via backward-compatible fallbacks.

**Full Changelog**: [ver20260501.7...ver20260517.8](https://github.com/igugyj/StickersManager2/compare/ver20260501.7...ver20260517.8)

# Changelog — ver20260416.6 → ver20260501.7 (2026-05-01)

### Fixes

- Fixed an issue where the configuration file would be **cleared or reset to default** when the program exits unexpectedly (e.g., crash, forced termination)
- Fixed a bug that caused the program to crash when double‑clicking the system tray icon

Note: new configuration still requires a restart of the software to take effect.

**Full Changelog**: [ver20260416.6...ver20260501.7](https://github.com/igugyj/StickersManager2/compare/ver20260416.6...ver20260501.7)

# Changelog — ver20260116.5 → ver20260416.6 (2026-04-16)

### Added

- **多贴纸库支持**：现在可以管理多个独立的贴纸库，方便分类与切换
- **响应式布局**：界面可根据窗口大小自动调整，适配不同屏幕尺寸

### Changed

- 更新 `README.md`，完善项目介绍与使用说明
- 新增配置示例，帮助用户快速自定义设置

**Full Changelog**: [ver20260116.5...ver20260416.6](https://github.com/igugyj/StickersManager2/compare/ver20260116.5...ver20260416.6)

# Changelog — ver20260106.4 → ver20260116.5 (2026-01-17)

### Fixes

- 修复复制表情可能失效的问题（不一定有效）
- 修复窗口顶置

**Full Changelog**: [ver20260106.4...ver20260116.5](https://github.com/igugyj/StickersManager2/compare/ver20260106.4...ver20260116.5)

# Changelog — initial release: ver20260106.4 (2026-01-06)

- First release
- Python version: <https://github.com/csy214-beep/StickersManager>
