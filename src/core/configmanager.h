#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QSize>
#include <QPoint>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QVector>

struct LibraryConfig
{
    int id = -1;
    QString path;
    QString hotkey;
    bool enabled;
    QJsonObject settings;

    LibraryConfig() : enabled(true) {}
    LibraryConfig(const QString &p, const QString &h, bool e = true)
        : path(p), hotkey(h), enabled(e) {}
    LibraryConfig(const QString &p, const QString &h, const QJsonObject &s, bool e = true)
        : path(p), hotkey(h), enabled(e), settings(s) {}
};

class ConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();

    bool loadConfig();
    bool saveConfig();

    QVector<LibraryConfig> getLibraries() const;
    void setLibraries(const QVector<LibraryConfig> &libs);
    void addLibrary(const LibraryConfig &lib);

    void setConfig(const QJsonObject &cfg);
    QJsonObject config() const;

    // settings.json
    bool loadSettings();
    bool saveSettings();
    QString getDoubleClickTarget() const;
    void setDoubleClickTarget(const QString &v);

    bool getCheckForUpdatesOnStartup() const;
    void setCheckForUpdatesOnStartup(bool v);

    bool getStartWithWindows() const;
    void setStartWithWindows(bool v);

    // General defaults
    QSize getWindowSize() const;
    QPoint getWindowPosition() const;
    int getCategoryButtonSize() const;
    int getGridCellSize() const;
    int getGridColumns() const;
    int getRecentLimit() const;
    bool recentEnabled() const;
    bool animateThumbnails() const;
    bool animatePreview() const;
    bool showFileTypeTag() const;
    bool showStickerName() const;
    bool showStickerSize() const;
    bool showCategoryName() const;
    bool showCategoryCount() const;
    bool copyOnDoubleClick() const;
    bool highlightOnClick() const;
    bool getDefaultAlwaysOnTop() const;

    // settings.json fields
    int getSearchDelayMs() const;
    void setSearchDelayMs(int v);
    int getThumbnailCacheSize() const;
    void setThumbnailCacheSize(int v);

    // Effective per-library
    QSize getEffectiveWindowSize(const LibraryConfig &lib) const;
    QPoint getEffectiveWindowPosition(const LibraryConfig &lib) const;
    int getEffectiveCategoryButtonSize(const LibraryConfig &lib) const;
    int getEffectiveGridCellSize(const LibraryConfig &lib) const;
    int getEffectiveGridColumns(const LibraryConfig &lib) const;
    int getEffectiveRecentLimit(const LibraryConfig &lib) const;
    bool getEffectiveRecentEnabled(const LibraryConfig &lib) const;
    int getEffectiveThumbnailCacheSize(const LibraryConfig &lib) const;
    bool getEffectiveAnimateThumbnails(const LibraryConfig &lib) const;
    bool getEffectiveAnimatePreview(const LibraryConfig &lib) const;
    bool getEffectiveShowFileTypeTag(const LibraryConfig &lib) const;
    bool getEffectiveShowStickerName(const LibraryConfig &lib) const;
    bool getEffectiveShowStickerSize(const LibraryConfig &lib) const;
    bool getEffectiveShowCategoryName(const LibraryConfig &lib) const;
    bool getEffectiveShowCategoryCount(const LibraryConfig &lib) const;
    bool getEffectiveHighlightOnClick(const LibraryConfig &lib) const;
    bool getEffectiveCopyOnDoubleClick(const LibraryConfig &lib) const;
    bool getEffectiveAlwaysOnTop(const LibraryConfig &lib) const;

    QString getConfigPath() const;

    QJsonObject getDefaultConfig();

private:
    QJsonObject defaultBlock() const { return m_config["default"].toObject(); }
    QJsonValue defVal(const QString &cat, const QString &key) const;
    QJsonValue effVal(const LibraryConfig &lib, const QString &cat, const QString &key) const;

    QJsonObject m_config;
    QString m_configPath;

    QJsonObject m_settings;
    QString m_settingsPath;
};

#endif // CONFIGMANAGER_H
