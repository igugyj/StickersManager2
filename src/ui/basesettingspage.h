#ifndef BASESETTINGSPAGE_H
#define BASESETTINGSPAGE_H

#include <QWidget>

class ConfigManager;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class HotkeyCaptureButton;

class BaseSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit BaseSettingsPage(ConfigManager *config, QWidget *parent = nullptr);
    void applyToConfig();
    void refreshTargets();
    // 全局设置热键：录制状态（main.cpp 录制期间抑制全局热键触发）+ 冲突重算（跨页改动后刷新红标）
    bool isHotkeyRecording() const;
    void refreshHotkeyConflict();
private slots:
    void checkForUpdates();
private:
    void updateCheckStatus();
    ConfigManager *m_config;
    QComboBox *m_doubleClickTarget;
    QSpinBox *m_searchDelayMs;
    QSpinBox *m_thumbnailCacheSize;
    QComboBox *m_logLevel;
    HotkeyCaptureButton *m_openSettingsHotkey;
    QCheckBox *m_openSettingsEnabled;
    QCheckBox *m_checkOnStartup;
    QCheckBox *m_startWithWindows;
    QLabel *m_checkStatus;
    QPushButton *m_checkForUpdatesBtn;
    void populateTargets();
};

#endif // BASESETTINGSPAGE_H
