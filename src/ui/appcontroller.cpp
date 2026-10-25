#include "appcontroller.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QSystemTrayIcon>
#include "mainwindow.h"
#include "tray.h"
#include "settingsdialog.h"
#include "convertcodetostring.hpp"
#include "checkupdate.hpp"
#include "log.hpp"

AppController::AppController(ConfigManager &config, QObject *parent)
    : QObject(parent), m_config(config)
{
    // 应用日志级别：settings.json 的 logLevel（空/未知 = 按构建类型的默认值）
    applyLogLevel();
    qDebug().noquote() << QString("StickersManager 启动 — 日志级别: %1（%2）")
                              .arg(logLevelToString(getLogLevel()))
                              .arg(defaultLogLevel() == LogLevel::Debug ? "Debug 构建" : "Release 构建");

    m_listener = new GlobalInputListener();
    connect(m_listener, &GlobalInputListener::keyReleased, this, &AppController::onGlobalKeyReleased);
}

void AppController::applyLogLevel()
{
    setLogLevel(logLevelFromString(m_config.getLogLevel(), defaultLogLevel()));
}

void AppController::startup()
{
    // Show settings dialog on first launch if no libraries configured
    auto libs = m_config.getLibraries();
    bool hasLib = false;
    for (const auto &lib : libs)
        if (!lib.path.isEmpty() && QDir(lib.path).exists())
        {
            hasLib = true;
            break;
        }

    if (!hasLib)
    {
        SettingsDialog tmpDlg(&m_config, false, nullptr);
        if (tmpDlg.exec() == QDialog::Accepted)
        {
            m_config.loadSettings();
            applyLogLevel();
            applySettingsHotkey();
            libs = m_config.getLibraries();
        }
    }

    createWindows(libs);
    applySettingsHotkey(); // 设置热键状态 + 启动监听（此时库热键映射已就绪，日志计数完整）

    TrayIcon::instance()->updateShowMenu(libs);

    setupTray();

    TrayIcon::instance()->show();

    checkupdate(m_config.getCheckForUpdatesOnStartup());
}

void AppController::createWindows(const QVector<LibraryConfig> &libs)
{
    for (const auto &lib : libs)
    {
        if (lib.path.isEmpty())
            continue;
        if (!m_windows.contains(lib.path))
        {
            auto *window = new MainWindow(&m_config, lib);
            m_windows[lib.path] = window;
            QFileInfo dirInfo(lib.path);
            window->setWindowTitle("Stickers Manager - " + dirInfo.fileName());
            window->hide();
        }
    }
    rebuildHotkeyMapping();
    TrayIcon::instance()->updateShowMenu(libs);
}

void AppController::removeStaleWindows(const QVector<LibraryConfig> &libs)
{
    QStringList active;
    for (const auto &lib : libs)
        if (!lib.path.isEmpty())
            active.append(lib.path);
    QStringList toRemove;
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
        if (!active.contains(it.key()))
            toRemove.append(it.key());
    for (const auto &path : toRemove)
    {
        delete m_windows[path];
        m_windows.remove(path);
    }
}

void AppController::rebuildHotkeyMapping()
{
    m_hotkeyToWindow.clear();
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
    {
        LibraryConfig libConfig = it.value()->getLibraryConfig();
        if (libConfig.enabled && !libConfig.hotkey.isEmpty())
        {
            m_hotkeyToWindow[libConfig.hotkey] = it.value();
        }
    }
}

void AppController::onGlobalKeyReleased(int keyCode, ModifierKeys modifiers)
{
    // Base 页录制热键期间抑制全局触发（否则录制时按到当前热键会把设置对话框关掉）
    if (m_settingsDlg && m_settingsDlg->isHotkeyRecording())
        return;

    QString keyName = keyCodeToKeyString(keyCode);
    QString modifiersName = modifiersToString(modifiers);
    QString hotkey = modifiersName + "+" + keyName;
    if (!modifiers) hotkey = keyName;

    // 设置热键优先命中（与库热键冲突时靠双向标红引导，不靠运行时仲裁）
    if (m_settingsHotkeyEnabled && !m_settingsHotkey.isEmpty() &&
        ShortcutCompare::compareShortcutKeys(hotkey, m_settingsHotkey)) {
        toggleSettings();
        return;
    }

    for (auto it = m_hotkeyToWindow.begin(); it != m_hotkeyToWindow.end(); ++it) {
        if (ShortcutCompare::compareShortcutKeys(hotkey, it.key())) {
            MainWindow *window = it.value();
            if (window->isHidden())
                window->showWindow();
            else
                window->hide();
            break;
        }
    }
}

void AppController::syncHotkeyListener()
{
    // 启动条件：任一库热键存在，或设置热键启用（后者无库热键时也必须监听）
    const bool settingsHkActive = m_settingsHotkeyEnabled && !m_settingsHotkey.isEmpty();
    if (m_hotkeyToWindow.isEmpty() && !settingsHkActive)
    {
        m_listener->stopListening();
        return;
    }
    if (m_listener->isListening())
        return;
    if (m_listener->startListening())
    {
        qDebug() << "Global input listener is running with"
                 << m_hotkeyToWindow.size() + (settingsHkActive ? 1 : 0) << "hotkeys";
    }
    else
    {
        qCritical() << "Failed to start global input listening";
    }
}

void AppController::applySettingsHotkey()
{
    m_settingsHotkey = m_config.getOpenSettingsHotkey();
    m_settingsHotkeyEnabled = m_config.getOpenSettingsHotkeyEnabled();
    syncHotkeyListener();
}

void AppController::fullReload()
{
    m_config.loadSettings();
    applyLogLevel();
    applySettingsHotkey();
    auto libs = m_config.getLibraries();
    removeStaleWindows(libs);
    createWindows(libs);

    // rescan all existing windows
    for (auto window : m_windows)
        window->reloadLibrary();
    syncHotkeyListener();
}

void AppController::applyChanges()
{
    m_config.loadSettings();
    applyLogLevel();
    applySettingsHotkey();
    auto libs = m_config.getLibraries();
    removeStaleWindows(libs);
    createWindows(libs);

    // refresh per-window library config (hotkey, overrides, ...)
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
    {
        for (const auto &lib : libs)
        {
            if (lib.path == it.key())
            {
                it.value()->updateLibraryConfig(lib);
                break;
            }
        }
    }

    rebuildHotkeyMapping();
    TrayIcon::instance()->updateShowMenu(libs);
    syncHotkeyListener();

    for (auto window : m_windows)
        window->applySettings();

    // visible windows keep their visibility, reapplied with new geometry/flags
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
        if (it.value()->isVisible())
            it.value()->showWindow();
}

void AppController::showSettings(bool toggle)
{
    if (toggle && m_settingsDlg && m_settingsDlg->isVisible())
    {
        m_settingsDlg->close();
        return;
    }
    if (!m_settingsDlg)
    {
        auto *dlg = new SettingsDialog(&m_config, true);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &SettingsDialog::applied, this, &AppController::applyChanges);
        connect(dlg, &QDialog::finished, this, [this](int)
                { m_settingsDlg = nullptr; });
        m_settingsDlg = dlg;
    }
    m_settingsDlg->show();
    m_settingsDlg->raise();
    m_settingsDlg->activateWindow();
}

// 全局热键触发入口：toggle 语义（已显示则关闭），与托盘双击一致
void AppController::toggleSettings()
{
    // 首次启动的模态设置对话框期间不响应，避免嵌套出第二个对话框
    if (QApplication::activeModalWidget())
        return;
    showSettings(true);
}

void AppController::setupTray()
{
    connect(TrayIcon::instance()->showSubMenu, &QMenu::triggered, this, &AppController::onShowMenuTriggered);

    connect(TrayIcon::instance(), &TrayIcon::activated, this, &AppController::onTrayActivated);

    connect(TrayIcon::instance()->action_rescan, &QAction::triggered, this, &AppController::fullReload);

    connect(TrayIcon::instance()->action_settings, &QAction::triggered, this, [this]()
            { showSettings(false); });
}

void AppController::onShowMenuTriggered(QAction *action)
{
    QString libraryPath = action->data().toString();
    if (m_windows.contains(libraryPath)) {
        MainWindow *window = m_windows[libraryPath];
        if (window->isHidden())
            window->showWindow();
        else
            window->hide();
    }
}

void AppController::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason != QSystemTrayIcon::DoubleClick)
        return;

    QString target = m_config.getDoubleClickTarget();
    if (target == "settings")
    {
        showSettings(true);
        return;
    }

    MainWindow *win = nullptr;
    if (target == "first-library" || target.isEmpty())
    {
        // first library in id (config) order
        for (const auto &lib : m_config.getLibraries())
        {
            if (m_windows.contains(lib.path))
            {
                win = m_windows[lib.path];
                break;
            }
        }
    }
    else
    {
        bool isId = false;
        int targetId = target.toInt(&isId);
        if (isId)
        {
            for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it)
            {
                if (it.value()->getLibraryConfig().id == targetId)
                {
                    win = it.value();
                    break;
                }
            }
        }
        else
        {
            // legacy config: target stored as directory name
            for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it)
            {
                if (QFileInfo(it.key()).fileName() == target)
                {
                    win = it.value();
                    break;
                }
            }
        }
    }

    if (win)
    {
        if (win->isHidden())
            win->showWindow();
        else
            win->hide();
    }
}
