<h1 align="center">
StickersManager
</h1>
<p align="center">
    <img src="https://img.shields.io/badge/language-C%2B%2B20-blue?logo=c%2B%2B" alt="Language">
    <img src="https://img.shields.io/badge/framework-Qt%206.10.1-brightgreen?logo=qt" alt="Qt">
    <img src="https://img.shields.io/badge/platform-Windows-blue?logo=windows" alt="Windows">
    <img src="https://img.shields.io/badge/license-MIT-lightgrey" alt="License">
</p>

## Overview

StickersManager is a Windows sticker management tool built with C++20 and Qt 6.10.1. It manages sticker packs from local folders.

![Demo](https://github.com/user-attachments/assets/d2b5bd03-7ba1-4f4e-9c14-c9a01cbb2302)

## Features

- **Libraries & categories** — each library is a local folder, every subfolder becomes a category; search stickers by filename and keep a per-library "Recently Used" list
- **Multi-window & hotkeys** — one window per library, toggled by global hotkeys (record conflicts highlighted); press **Ctrl+I** to open Settings from anywhere
- **Stickers** — double-click to copy, right-click for full-size preview, optional tags (name / size / file type) on every cell
- **Animated GIFs** — play while visible, unload off-screen; hiding a window releases its memory immediately
- **Configurable** — per-library overrides on top of global defaults, drag-to-reorder libraries, tray double-click target, Start with Windows
- **Hot reload** — tray *Rescan* picks up config and library changes without restarting
- **Update check** — on startup and on demand, against the latest GitHub release

## Installation

Download the latest version from the [Releases page](https://github.com/igugyj/StickersManager2/releases):

- `StickersManager_<version>.exe` — installer
- `StickersManager_<version>.7z` — portable (extract anywhere; configuration lives next to the executable)

## Project Structure

```
StickersManager/
├── CMakeLists.txt
├── resources.qrc
├── pkg.iss                # Inno Setup installer script
├── assets/                # icons, stylesheet, icon.rc
├── src/
│   ├── main.cpp           # startup skeleton → AppController
│   ├── appinfo.h          # version, author, license, URLs
│   ├── core/              # config, library, image loading, cache, input, update check
│   ├── ui/                # appcontroller, mainwindow, tray, dialogs, cells, settings pages
│   └── utils/             # log, launcher, fsutil, keycode maps
├── scripts/               # release packaging & version-check helpers
├── docs/                  # design docs
└── thirdparty/
    └── stb/               # stb_image, stb_image_resize2
```

## Sticker Library Structure

Each library is a local folder; every **immediate subdirectory becomes a category**:

```
Stickers/
├── Cat/
│   ├── sticker1.png
│   └── sticker2.gif
├── Meme/
│   └── ...
```

- Supported image formats: `png, jpg, jpeg, gif, bmp, tiff, tif, webp, psd, hdr, tga, ico, svg, heic, heif, avif`
- Files named `.preview*` (or containing `.preview.`) are excluded from scanning, search, and statistics

## Configuration

App-level settings live next to the executable (relative to `[EXE_DIR]`):

```
[EXE_DIR]/.stickersmanager/
├── config.json          # defaults{} + libraries[] (id, path, hotkey, enabled, settings{})
├── settings.json        # doubleClickTarget, searchDelayMs, thumbnailCacheSize,
│                        # checkForUpdatesOnStartup, startWithWindows, logLevel,
│                        # openSettingsHotkey, openSettingsHotkeyEnabled
└── recent_<md5>.json    # per-library recently-used records
```

Configuration is written only on "Save & Reload" in the Settings dialog.

## Logs

Diagnostics are written to `log/log.log` **relative to the working directory** (created/truncated on each start). The detail level is configurable in Settings → Base → Logging; debug builds also print to the console.

## Build

```bash
cd build && cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DQT_PATH=<path-to-Qt6> && cmake --build . -- -j8
```

Requires Qt 6.10 and a C++20 toolchain (MinGW 13.1 or MSVC).

## Third-Party Libraries

- [Qt 6.10.1](https://www.qt.io/)
- [stb_image](https://github.com/nothings/stb)

## License

[MIT](LICENSE)

---

> If you find this tool useful, a star would be appreciated! ⭐
