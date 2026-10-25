#pragma once

#include <QObject>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QAction>
#include <QSystemTrayIcon> // QSystemTrayIcon::ActivationReason（成员函数签名需要完整类型）
#include "configmanager.h"
#include "globalinputlistener.h"

class MainWindow;
class SettingsDialog;

// 应用外壳（TODO#3 从 main.cpp 外移）：main() 只留启动骨架（日志/QApp/锁/样式），
// 窗口管理、全局热键、设置对话框生命周期、托盘接线、两条重载路径全部收编于此。
//
// 顺序红线（见 docs/.ai/changelog）：
// - startup()：首启模态对话框 → createWindows → applySettingsHotkey → 托盘
// - fullReload（托盘 Rescan，重扫）≠ applyChanges（Save & Reload，不重扫）
// - keyReleased：录制期抑制 → 设置热键优先 → 库热键 map
class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(ConfigManager &config, QObject *parent = nullptr);

    // 启动编排（内部顺序敏感，勿重排）
    void startup();

private:
    void applyLogLevel();
    void createWindows(const QVector<LibraryConfig> &libs);
    void removeStaleWindows(const QVector<LibraryConfig> &libs);
    void rebuildHotkeyMapping();
    void onGlobalKeyReleased(int keyCode, ModifierKeys modifiers);
    void syncHotkeyListener();
    void applySettingsHotkey();
    void fullReload();   // 托盘 Rescan：reload 设置 + 清理/新建窗口 + 全部窗口重扫
    void applyChanges(); // 设置 Save & Reload：更新各窗口配置 + applySettings，不重扫
    void showSettings(bool toggle = false);
    void toggleSettings();
    void setupTray();
    void onShowMenuTriggered(QAction *action);                              // 托盘「显示」子菜单：切换对应库窗口
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);        // 托盘双击：按 doubleClickTarget 分派

    ConfigManager &m_config;
    QPointer<SettingsDialog> m_settingsDlg;
    QMap<QString, MainWindow *> m_windows;
    QMap<QString, MainWindow *> m_hotkeyToWindow;
    GlobalInputListener *m_listener = nullptr;
    QString m_settingsHotkey;
    bool m_settingsHotkeyEnabled = false;
};
