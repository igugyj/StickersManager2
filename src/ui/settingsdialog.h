#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class ConfigManager;
class GeneralSettingsPage;
class LibrarySettingsPage;
class BaseSettingsPage;
class AboutPage;
class QTabWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(ConfigManager *config, bool keepOpenOnSave = false, QWidget *parent = nullptr);
    // Base 页热键录制中（main.cpp 据此抑制全局热键触发，避免录制时按到热键关掉对话框）
    bool isHotkeyRecording() const;
signals:
    void applied();
private slots:
    void onSave();
private:
    ConfigManager *m_config;
    bool m_keepOpenOnSave;
    QTabWidget *m_tabWidget;
    GeneralSettingsPage *m_generalPage;
    LibrarySettingsPage *m_libraryPage;
    BaseSettingsPage *m_basePage;
    AboutPage *m_aboutPage;
};

#endif // SETTINGSDIALOG_H
