#ifndef BASESETTINGSPAGE_H
#define BASESETTINGSPAGE_H

#include <QWidget>

class ConfigManager;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;

class BaseSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit BaseSettingsPage(ConfigManager *config, QWidget *parent = nullptr);
    void applyToConfig();
    void refreshTargets();
private slots:
    void checkForUpdates();
private:
    void updateCheckStatus();
    ConfigManager *m_config;
    QComboBox *m_doubleClickTarget;
    QSpinBox *m_searchDelayMs;
    QSpinBox *m_thumbnailCacheSize;
    QComboBox *m_logLevel;
    QCheckBox *m_checkOnStartup;
    QCheckBox *m_startWithWindows;
    QLabel *m_checkStatus;
    QPushButton *m_checkForUpdatesBtn;
    void populateTargets();
};

#endif // BASESETTINGSPAGE_H
