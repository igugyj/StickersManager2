#include "configmanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QSaveFile>
#include <algorithm>

// Single source of truth for every default setting value.
static QJsonObject hardDefaults() {
    QJsonObject def;

    QJsonObject window;
    window["position"] = QJsonArray{900, 50};
    window["size"] = QJsonArray{540, 430};
    window["alwaysOnTop"] = true;
    def["window"] = window;

    QJsonObject ui;
    ui["categoryButtonSize"] = 90;
    ui["gridCellSize"] = 120;
    ui["gridColumns"] = 3;
    ui["recentLimit"] = 100;
    ui["recentEnabled"] = true;
    def["ui"] = ui;

    QJsonObject behavior;
    behavior["copyOnDoubleClick"] = true;
    behavior["highlightOnClick"] = true;
    behavior["animateThumbnails"] = false;
    behavior["animatePreview"] = true;
    behavior["showFileTypeTag"] = true;
    behavior["showStickerName"] = true;
    behavior["showStickerSize"] = true;
    behavior["showCategoryName"] = true;
    behavior["showCategoryCount"] = true;
    def["behavior"] = behavior;

    def["performance"] = QJsonObject();
    return def;
}

// Recursively fill missing or type-corrupted keys of `block` from the hard
// defaults. User values with the correct type are kept.
static void mergeDefaults(QJsonObject &block, const QJsonObject &hard) {
    for (auto it = hard.begin(); it != hard.end(); ++it) {
        const QJsonValue cur = block.value(it.key());
        if (cur.isObject() && it.value().isObject()) {
            QJsonObject sub = cur.toObject();
            mergeDefaults(sub, it.value().toObject());
            block[it.key()] = sub;
        } else if (cur.isUndefined() || cur.type() != it.value().type()) {
            block[it.key()] = it.value();
        }
    }
}

static QJsonObject defaultSettings() {
    QJsonObject s;
    s["doubleClickTarget"] = QString("settings");
    s["searchDelayMs"] = 300;
    s["thumbnailCacheSize"] = 200;
    s["checkForUpdatesOnStartup"] = true;
    s["startWithWindows"] = false;
    return s;
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent) {
    QString exeDir = QCoreApplication::applicationDirPath();
    QDir configDir = QDir(exeDir + "/.stickersmanager");
    QDir().mkpath(configDir.absolutePath());
    m_configPath = configDir.absolutePath() + "/config.json";
    m_settingsPath = configDir.absolutePath() + "/settings.json";

    if (!loadConfig()) {
        qWarning() << "Using default configuration";
    }
    loadSettings();
}

ConfigManager::~ConfigManager()
{
}

bool ConfigManager::loadConfig() {
    QFile configFile(m_configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        m_config = getDefaultConfig();
        return false;
    }

    QByteArray data = configFile.readAll();
    configFile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        m_config = getDefaultConfig();
        return false;
    }

    m_config = doc.object();
    QJsonObject def = m_config["default"].toObject();
    mergeDefaults(def, hardDefaults());
    m_config["default"] = def;
    return true;
}

bool ConfigManager::saveConfig() {
    QSaveFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(m_config);
    if (file.write(doc.toJson()) == -1 || !file.commit()) {
        return false;
    }

    return true;
}

void ConfigManager::setConfig(const QJsonObject &cfg) {
    m_config = cfg;
}

QJsonObject ConfigManager::config() const {
    return m_config;
}

bool ConfigManager::loadSettings() {
    QFile file(m_settingsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_settings = defaultSettings();
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        m_settings = defaultSettings();
        return false;
    }

    m_settings = doc.object();
    return true;
}

bool ConfigManager::saveSettings() {
    QSaveFile file(m_settingsPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(m_settings);
    if (file.write(doc.toJson()) == -1 || !file.commit())
        return false;

    return true;
}

QString ConfigManager::getDoubleClickTarget() const {
    return m_settings["doubleClickTarget"].toString("settings");
}

void ConfigManager::setDoubleClickTarget(const QString &v) {
    m_settings["doubleClickTarget"] = v;
}

bool ConfigManager::getCheckForUpdatesOnStartup() const {
    return m_settings["checkForUpdatesOnStartup"].toBool(true);
}

void ConfigManager::setCheckForUpdatesOnStartup(bool v) {
    m_settings["checkForUpdatesOnStartup"] = v;
}

bool ConfigManager::getStartWithWindows() const {
    return m_settings["startWithWindows"].toBool(false);
}

void ConfigManager::setStartWithWindows(bool v) {
    m_settings["startWithWindows"] = v;
}

QJsonValue ConfigManager::defVal(const QString &cat, const QString &key) const {
    return defaultBlock()[cat].toObject()[key];
}

QJsonValue ConfigManager::effVal(const LibraryConfig &lib, const QString &cat, const QString &key) const {
    QJsonObject o = lib.settings[cat].toObject();
    return o.contains(key) ? o[key] : defVal(cat, key);
}

QVector<LibraryConfig> ConfigManager::getLibraries() const {
    QVector<LibraryConfig> result;
    QJsonArray libArray = m_config["libraries"].toArray();
    result.reserve(libArray.size());
    for (const QJsonValue &val: libArray) {
        QJsonObject obj = val.toObject();
        LibraryConfig lib;
        lib.id = obj["id"].toInt(-1);
        lib.path = obj["path"].toString();
        lib.hotkey = obj["hotkey"].toString();
        lib.enabled = obj["enabled"].toBool(true);
        lib.settings = obj["settings"].toObject();
        result.append(lib);
    }
    // legacy migration: entries without id get their array position
    for (int i = 0; i < result.size(); ++i)
        if (result[i].id < 0)
            result[i].id = i;
    // id is the authoritative order marker
    std::sort(result.begin(), result.end(),
              [](const LibraryConfig &a, const LibraryConfig &b) { return a.id < b.id; });
    return result;
}

void ConfigManager::setLibraries(const QVector<LibraryConfig> &libs) {
    QVector<LibraryConfig> sorted = libs;
    std::sort(sorted.begin(), sorted.end(),
              [](const LibraryConfig &a, const LibraryConfig &b) { return a.id < b.id; });
    QJsonArray libArray;
    for (const LibraryConfig &lib: sorted) {
        QJsonObject obj;
        obj["id"] = lib.id;
        obj["path"] = lib.path;
        obj["hotkey"] = lib.hotkey;
        obj["enabled"] = lib.enabled;
        if (!lib.settings.isEmpty())
            obj["settings"] = lib.settings;
        libArray.append(obj);
    }
    m_config["libraries"] = libArray;
}

void ConfigManager::addLibrary(const LibraryConfig &lib) {
    auto libs = getLibraries();
    libs.append(lib);
    setLibraries(libs);
}

QJsonObject ConfigManager::getDefaultConfig() {
    QJsonObject config;
    config["libraries"] = QJsonArray();
    config["default"] = hardDefaults();
    return config;
}

QSize ConfigManager::getWindowSize() const {
    QJsonArray a = defVal("window", "size").toArray();
    return a.size() == 2 ? QSize(a[0].toInt(), a[1].toInt()) : QSize(540, 430);
}

QPoint ConfigManager::getWindowPosition() const {
    QJsonArray a = defVal("window", "position").toArray();
    return a.size() == 2 ? QPoint(a[0].toInt(), a[1].toInt()) : QPoint(900, 50);
}

int ConfigManager::getCategoryButtonSize() const {
    return defVal("ui", "categoryButtonSize").toInt();
}

int ConfigManager::getGridCellSize() const {
    return defVal("ui", "gridCellSize").toInt();
}

int ConfigManager::getGridColumns() const {
    return defVal("ui", "gridColumns").toInt();
}

int ConfigManager::getRecentLimit() const {
    return defVal("ui", "recentLimit").toInt();
}

bool ConfigManager::recentEnabled() const {
    return defVal("ui", "recentEnabled").toBool();
}

int ConfigManager::getThumbnailCacheSize() const {
    return m_settings["thumbnailCacheSize"].toInt(200);
}

void ConfigManager::setThumbnailCacheSize(int v) {
    m_settings["thumbnailCacheSize"] = v;
}

bool ConfigManager::animateThumbnails() const {
    return defVal("behavior", "animateThumbnails").toBool();
}

bool ConfigManager::animatePreview() const {
    return defVal("behavior", "animatePreview").toBool();
}

bool ConfigManager::showFileTypeTag() const {
    return defVal("behavior", "showFileTypeTag").toBool();
}

bool ConfigManager::showStickerName() const {
    return defVal("behavior", "showStickerName").toBool();
}

bool ConfigManager::showStickerSize() const {
    return defVal("behavior", "showStickerSize").toBool();
}

bool ConfigManager::showCategoryName() const {
    return defVal("behavior", "showCategoryName").toBool();
}

bool ConfigManager::showCategoryCount() const {
    return defVal("behavior", "showCategoryCount").toBool();
}

bool ConfigManager::copyOnDoubleClick() const {
    return defVal("behavior", "copyOnDoubleClick").toBool();
}

bool ConfigManager::highlightOnClick() const {
    return defVal("behavior", "highlightOnClick").toBool();
}

int ConfigManager::getSearchDelayMs() const {
    return m_settings["searchDelayMs"].toInt(300);
}

void ConfigManager::setSearchDelayMs(int v) {
    m_settings["searchDelayMs"] = v;
}

bool ConfigManager::getDefaultAlwaysOnTop() const {
    return defVal("window", "alwaysOnTop").toBool();
}

QSize ConfigManager::getEffectiveWindowSize(const LibraryConfig &lib) const {
    QJsonArray a = effVal(lib, "window", "size").toArray();
    return a.size() == 2 ? QSize(a[0].toInt(), a[1].toInt()) : getWindowSize();
}

QPoint ConfigManager::getEffectiveWindowPosition(const LibraryConfig &lib) const {
    QJsonArray a = effVal(lib, "window", "position").toArray();
    return a.size() == 2 ? QPoint(a[0].toInt(), a[1].toInt()) : getWindowPosition();
}

int ConfigManager::getEffectiveCategoryButtonSize(const LibraryConfig &lib) const {
    return effVal(lib, "ui", "categoryButtonSize").toInt();
}

int ConfigManager::getEffectiveGridCellSize(const LibraryConfig &lib) const {
    return effVal(lib, "ui", "gridCellSize").toInt();
}

int ConfigManager::getEffectiveGridColumns(const LibraryConfig &lib) const {
    return effVal(lib, "ui", "gridColumns").toInt();
}

int ConfigManager::getEffectiveRecentLimit(const LibraryConfig &lib) const {
    return effVal(lib, "ui", "recentLimit").toInt();
}

bool ConfigManager::getEffectiveRecentEnabled(const LibraryConfig &lib) const {
    return effVal(lib, "ui", "recentEnabled").toBool();
}

int ConfigManager::getEffectiveThumbnailCacheSize(const LibraryConfig &) const {
    return getThumbnailCacheSize();
}

bool ConfigManager::getEffectiveAnimateThumbnails(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "animateThumbnails").toBool();
}

bool ConfigManager::getEffectiveAnimatePreview(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "animatePreview").toBool();
}

bool ConfigManager::getEffectiveShowFileTypeTag(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "showFileTypeTag").toBool();
}

bool ConfigManager::getEffectiveShowStickerName(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "showStickerName").toBool();
}

bool ConfigManager::getEffectiveShowStickerSize(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "showStickerSize").toBool();
}

bool ConfigManager::getEffectiveShowCategoryName(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "showCategoryName").toBool();
}

bool ConfigManager::getEffectiveShowCategoryCount(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "showCategoryCount").toBool();
}

bool ConfigManager::getEffectiveHighlightOnClick(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "highlightOnClick").toBool();
}

bool ConfigManager::getEffectiveCopyOnDoubleClick(const LibraryConfig &lib) const {
    return effVal(lib, "behavior", "copyOnDoubleClick").toBool();
}

bool ConfigManager::getEffectiveAlwaysOnTop(const LibraryConfig &lib) const {
    return effVal(lib, "window", "alwaysOnTop").toBool();
}

QString ConfigManager::getConfigPath() const {
    return m_configPath;
}
