# Contributing to StickersManager

Thanks for your interest! Bug reports, fixes, UI polish, and docs are all welcome. This file covers the basics — you don't need to read anything else to get started.

By participating you agree to follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to Contribute

You don't need to write code to help. All of these are valued:

- **Bug reports** — reproducible, with logs attached (see [Reporting Bugs](#reporting-bugs)).
- **Feature requests** — describe the problem first, the solution second.
- **Code** — bug fixes, performance and memory improvements, UI polish, new sticker formats.
- **Documentation** — README, `docs/`, screenshots, clearer comments around non-obvious logic.
- **Packaging** — installer and portable build improvements.
- **Translation / wording** — user-visible strings and error messages.
- **Spreading the word** — a star, a post, telling someone who needs it.

For anything larger than a small fix, **open an issue first** so we can agree on the approach before you invest time in it. Small, focused PRs are reviewed much faster than large ones.

## Setup

You'll need **Windows 10/11**, **Qt 6.10.x**, a **C++20** compiler (MinGW 13.1+ or MSVC 2022), and **CMake**. Inno Setup 6 is only needed for the installer.

```bash
git clone https://github.com/igugyj/StickersManager2.git
cd StickersManager2
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DQT_PATH=C:/Qt/6.10.1/mingw_64
cmake --build . -- -j8
```

For MSVC use `-G "Visual Studio 17 2022" -A x64` and point `QT_PATH` at `msvc2022_64`. To run, either add Qt's `bin` folder to `PATH` or run `windeployqt` on the built exe. If the build commands drift from reality, `CMakeLists.txt` wins — update this file in the same PR.

## Where Things Live

```
src/core/     config, library scanning, image loading, cache, input, update check
src/ui/       appcontroller, mainwindow, tray, dialogs, cells, settings pages
src/utils/    log, launcher, fsutil, keycode maps
assets/       icons, stylesheet          resources.qrc   Qt resource manifest
pkg.iss       Inno Setup script          scripts/        release helpers
thirdparty/   vendored stb (don't patch in place)
```

Two things worth knowing before you debug:

- App state lives in `[EXE_DIR]/.stickersmanager/` (`config.json`, `settings.json`, `recent_<md5>.json`). It is **only written on "Save & Reload"** in Settings — please keep it that way.
- `log/log.log` is written relative to the working directory and truncated on each start.

## Coding Style

Just match the file you're editing — that's the main rule. A few habits that make review easier:

- Use the pointer-to-member `connect()` syntax, not `SIGNAL`/`SLOT`.
- Let Qt's parent-child ownership handle `QObject`s.
- Don't add new compiler warnings.

If you touch the image pipeline, keep the low-memory design: animated GIFs play only while visible and unload off-screen. Hiding a window releases its memory.

## Testing

There's no automated suite yet, so a quick manual pass is enough. Run the parts your change could affect — library scanning, search, copy/preview, GIF animation and memory on hide, hotkeys, Settings save/reload, tray → Rescan. Mention your Windows version and toolchain in the PR.

## Commits and PRs

Conventional Commits are appreciated but not enforced:

```
fix(cache): release GIF frames when the window is hidden
feat(hotkeys): highlight conflicting shortcuts
```

Before opening a PR:

- Build in Debug (Release too if you can) with no new warnings.
- Keep it focused — split unrelated refactors out.
- Don't commit `build*/`, `log/`, or `.stickersmanager/`.
- Update `README.md`/`docs/` if user-facing behaviour changed.
- Leave version numbers and packaging to the maintainer.

Describe **what** changed, **why**, and **how you verified it**.

## Reporting Bugs

Include the app version, Windows version, steps to reproduce, expected vs. actual, and the relevant `log/log.log` excerpt (raise the log level in Settings → Base → Logging if needed). Screenshots help for UI issues. Please redact personal file paths.

## Feature Requests

Describe the use case rather than only the proposed UI — it helps us find the simplest solution. If your request is about memory or startup time, include measurements.

---

Questions that aren't bugs or feature requests are welcome too — just open an issue. Thanks for helping make StickersManager better!
