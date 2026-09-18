#include <QApplication>
#include <QMap>
#include <QFileInfo>
#include <QObject>
#include <QDir>
#include <QStyleFactory>
#include <QPointer>
#include <QMenu>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include "log.hpp"
#include "tray.h"
#include "mainwindow.h"
#include "configmanager.h"
#include "globalinputlistener.h"
#include "convertcodetostring.hpp"
#include "launcher.hpp"
#include "settingsdialog.h"
#include "updatechecker.h"
#include "appinfo.h"

#ifdef CONSOLE
#define DEBUG_MODE true
#else
#define DEBUG_MODE false
#endif

static void rebuildHotkeyMapping(const QMap<QString, MainWindow *> &windows, QMap<QString, MainWindow *> &hotkeyToWindow);

int main(int argc, char *argv[])
{
    initLogFile();
    setLogLevel(LogLevel::Debug);
    if (!DEBUG_MODE)
        qInstallMessageHandler(messageHandler);
    QApplication app(argc, argv);
    app.setApplicationName("Stickers Manager");
    app.setWindowIcon(QIcon(":/assets/st.png"));
    QString userName = QFileInfo(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).fileName();
    userName = !userName.isEmpty() ? userName : "unknown";
    QLockFile lockFile(QDir::temp().absoluteFilePath(QString("%1_%2.lock").arg(userName).arg(app.applicationName())));

    if (!lockFile.tryLock(100))
    {
        QMessageBox::warning(nullptr, "Warning", "There is already an instance running!");
        return 1;
    }
    app.setQuitOnLastWindowClosed(false);
    QStringList styles = QStyleFactory::keys();
    if (styles.contains("Fusion", Qt::CaseInsensitive))
        app.setStyle("Fusion");

    ConfigManager config;
    QPointer<SettingsDialog> settingsDlg;

    QMap<QString, MainWindow *> windows;
    QMap<QString, MainWindow *> hotkeyToWindow;
    QPointer<MainWindow> firstWindow;

    auto createWindows = [&](const QVector<LibraryConfig> &libs)
    {
        for (const auto &lib : libs)
        {
            if (lib.path.isEmpty())
                continue;
            if (!windows.contains(lib.path))
            {
                auto *window = new MainWindow(&config, lib);
                windows[lib.path] = window;
                QFileInfo dirInfo(lib.path);
                window->setWindowTitle("Stickers Manager - " + dirInfo.fileName());
                window->hide();
            }
        }
        rebuildHotkeyMapping(windows, hotkeyToWindow);
        TrayIcon::instance()->updateShowMenu(libs);
        if (firstWindow.isNull() && !windows.isEmpty())
            firstWindow = windows.first();
    };

    auto removeStaleWindows = [&](const QVector<LibraryConfig> &libs)
    {
        QStringList active;
        for (const auto &lib : libs)
            if (!lib.path.isEmpty())
                active.append(lib.path);
        QStringList toRemove;
        for (auto it = windows.begin(); it != windows.end(); ++it)
            if (!active.contains(it.key()))
                toRemove.append(it.key());
        for (const auto &path : toRemove)
        {
            delete windows[path];
            windows.remove(path);
        }
    };

    GlobalInputListener *listener = new GlobalInputListener();
    QObject::connect(listener, &GlobalInputListener::keyReleased, [&](int keyCode, ModifierKeys modifiers)
                     {
        QString keyName = keyCodeToKeyString(keyCode);
        QString modifiersName = modifiersToString(modifiers);
        QString hotkey = modifiersName + "+" + keyName;
        if (!modifiers) hotkey = keyName;

        for (auto it = hotkeyToWindow.begin(); it != hotkeyToWindow.end(); ++it) {
            if (ShortcutCompare::compareShortcutKeys(hotkey, it.key())) {
                MainWindow *window = it.value();
                if (window->isHidden())
                    window->showWindow();
                else
                    window->hide();
                break;
            }
        } });

    auto syncHotkeyListener = [&]()
    {
        if (hotkeyToWindow.isEmpty())
        {
            listener->stopListening();
            return;
        }
        if (listener->isListening())
            return;
        if (listener->startListening())
        {
            qDebug() << "Global input listener is running with" << hotkeyToWindow.size() << "hotkeys";
        }
        else
        {
            qCritical() << "Failed to start global input listening";
        }
    };

    auto fullReload = [&]()
    {
        config.loadSettings();
        auto libs = config.getLibraries();
        removeStaleWindows(libs);
        createWindows(libs);

        // rescan all existing windows
        for (auto window : windows)
            window->reloadLibrary();
        syncHotkeyListener();
    };

    auto applyChanges = [&]()
    {
        config.loadSettings();
        auto libs = config.getLibraries();
        removeStaleWindows(libs);
        createWindows(libs);

        // refresh per-window library config (hotkey, overrides, ...)
        for (auto it = windows.begin(); it != windows.end(); ++it)
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

        rebuildHotkeyMapping(windows, hotkeyToWindow);
        TrayIcon::instance()->updateShowMenu(libs);
        syncHotkeyListener();

        for (auto window : windows)
            window->applySettings();

        // visible windows keep their visibility, reapplied with new geometry/flags
        for (auto it = windows.begin(); it != windows.end(); ++it)
            if (it.value()->isVisible())
                it.value()->showWindow();
    };

    auto showSettings = [&](bool toggle = false)
    {
        if (toggle && settingsDlg && settingsDlg->isVisible())
        {
            settingsDlg->close();
            return;
        }
        if (!settingsDlg)
        {
            auto *dlg = new SettingsDialog(&config, true);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dlg, &SettingsDialog::applied, applyChanges);
            QObject::connect(dlg, &QDialog::finished, [&]()
                             { settingsDlg = nullptr; });
            settingsDlg = dlg;
        }
        settingsDlg->show();
        settingsDlg->raise();
        settingsDlg->activateWindow();
    };

    // Show settings dialog on first launch if no libraries configured
    auto libs = config.getLibraries();
    bool hasLib = false;
    for (const auto &lib : libs)
        if (!lib.path.isEmpty() && QDir(lib.path).exists())
        {
            hasLib = true;
            break;
        }

    if (!hasLib)
    {
        SettingsDialog tmpDlg(&config, false, nullptr);
        if (tmpDlg.exec() == QDialog::Accepted)
        {
            config.loadSettings();
            libs = config.getLibraries();
        }
    }

    createWindows(libs);
    syncHotkeyListener();

    TrayIcon::instance()->updateShowMenu(libs);

    QObject::connect(TrayIcon::instance()->showSubMenu, &QMenu::triggered, [&](QAction *action)
                     {
        QString libraryPath = action->data().toString();
        if (windows.contains(libraryPath)) {
            MainWindow *window = windows[libraryPath];
            if (window->isHidden())
                window->showWindow();
            else
                window->hide();
        } });

    QObject::connect(
        TrayIcon::instance(), &TrayIcon::activated,
        [&](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason != QSystemTrayIcon::DoubleClick)
                return;

            QString target = config.getDoubleClickTarget();
            if (target == "settings")
            {
                showSettings(true);
                return;
            }

            MainWindow *win = nullptr;
            if (target == "first-library" || target.isEmpty())
            {
                // first library in id (config) order
                for (const auto &lib : config.getLibraries())
                {
                    if (windows.contains(lib.path))
                    {
                        win = windows[lib.path];
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
                    for (auto it = windows.cbegin(); it != windows.cend(); ++it)
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
                    for (auto it = windows.cbegin(); it != windows.cend(); ++it)
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
        });

    QObject::connect(TrayIcon::instance()->action_rescan, &QAction::triggered, fullReload);

    QObject::connect(TrayIcon::instance()->action_settings, &QAction::triggered, [&]()
                     { showSettings(false); });

    TrayIcon::instance()->show();

    if (config.getCheckForUpdatesOnStartup())
    {
        auto *checker = new UpdateChecker();
        QObject::connect(checker, &UpdateChecker::finished, [checker](bool success, const QString &latestVersion, const QString &)
                         {
            if (success && UpdateChecker::compareVersions(latestVersion, AppInfo::version()) > 0) {
                TrayIcon::showMessage("Update Available",
                                      "Stickers Manager " + latestVersion + " is now available.");
            }
            checker->deleteLater(); });
        checker->check();
    }

    return app.exec();
}
static void rebuildHotkeyMapping(const QMap<QString, MainWindow *> &windows, QMap<QString, MainWindow *> &hotkeyToWindow)
{
    hotkeyToWindow.clear();
    for (auto it = windows.begin(); it != windows.end(); ++it)
    {
        LibraryConfig libConfig = it.value()->getLibraryConfig();
        if (libConfig.enabled && !libConfig.hotkey.isEmpty())
        {
            hotkeyToWindow[libConfig.hotkey] = it.value();
        }
    }
}