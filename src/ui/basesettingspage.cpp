#include "basesettingspage.h"
#include "configmanager.h"
#include "updatechecker.h"
#include "appinfo.h"
#include "launcher.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QFileInfo>
#include <QDir>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

namespace {
QString windowsStartupFolder()
{
#ifdef Q_OS_WIN
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_DEFAULT, nullptr, &path))) {
        QString result = QString::fromWCharArray(path);
        CoTaskMemFree(path);
        return result;
    }
#endif
    return QString();
}

QString startupShortcutPath()
{
    QString dir = windowsStartupFolder();
    return dir.isEmpty() ? QString() : dir + "/StickersManager.lnk";
}

void applyStartWithWindows(bool enable)
{
    QString linkPath = startupShortcutPath();
    if (linkPath.isEmpty()) {
        qDebug() << "Startup: could not locate the Windows Startup folder";
        return;
    }
    if (enable) {
        if (QFile::exists(linkPath)) {
            qDebug() << "Startup: shortcut already exists:" << linkPath;
            return;
        }
        bool ok = QFile::link(QCoreApplication::applicationFilePath(), linkPath);
        if (ok)
            qDebug() << "Startup: shortcut created:" << linkPath;
        else
            qDebug() << "Startup: failed to create shortcut:" << linkPath
                     << "target:" << QCoreApplication::applicationFilePath();
    } else {
        if (!QFile::exists(linkPath)) {
            qDebug() << "Startup: shortcut not present, nothing to remove:" << linkPath;
            return;
        }
        bool ok = QFile::remove(linkPath);
        if (ok)
            qDebug() << "Startup: shortcut removed:" << linkPath;
        else
            qDebug() << "Startup: failed to remove shortcut:" << linkPath;
    }
}
}

BaseSettingsPage::BaseSettingsPage(ConfigManager *config, QWidget *parent)
    : QWidget(parent), m_config(config)
{
    auto *root = new QVBoxLayout(this);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);

    auto *trayGroup = new QGroupBox("Tray");
    auto *trayForm = new QFormLayout(trayGroup);

    m_doubleClickTarget = new QComboBox(this);
    populateTargets();

    trayForm->addRow("Double-click action:", m_doubleClickTarget);
    contentLayout->addWidget(trayGroup);

    // --- Performance ---
    auto *perfGroup = new QGroupBox("Performance");
    auto *perfForm = new QFormLayout(perfGroup);
    m_searchDelayMs = new QSpinBox(this);
    m_searchDelayMs->setRange(0, 5000);
    m_searchDelayMs->setSuffix(" ms");
    m_searchDelayMs->setValue(config->getSearchDelayMs());
    m_thumbnailCacheSize = new QSpinBox(this);
    m_thumbnailCacheSize->setRange(10, 5000);
    m_thumbnailCacheSize->setValue(config->getThumbnailCacheSize());
    perfForm->addRow("Search Delay:", m_searchDelayMs);
    perfForm->addRow("Thumbnail Cache Size:", m_thumbnailCacheSize);
    contentLayout->addWidget(perfGroup);

    // --- Startup ---
    auto *startupGroup = new QGroupBox("Startup");
    auto *startupForm = new QFormLayout(startupGroup);
    m_startWithWindows = new QCheckBox(this);
    m_startWithWindows->setChecked(config->getStartWithWindows());
    m_startWithWindows->setToolTip("Adds a shortcut to the Windows Startup folder.");
    startupForm->addRow("Start with Windows:", m_startWithWindows);
    contentLayout->addWidget(startupGroup);

    // --- Update ---
    auto *updateGroup = new QGroupBox("Update");
    auto *updateGroupLayout = new QVBoxLayout(updateGroup);
    auto *updateForm = new QFormLayout;
    updateGroupLayout->addLayout(updateForm);
    m_checkOnStartup = new QCheckBox(this);
    m_checkOnStartup->setChecked(config->getCheckForUpdatesOnStartup());
    updateForm->addRow("Check for updates on startup:", m_checkOnStartup);

    m_checkStatus = new QLabel(this);
    m_checkStatus->setWordWrap(true);
    m_checkStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_checkStatus->hide();
    m_checkForUpdatesBtn = new QPushButton("Check for Updates", this);
    updateGroupLayout->addWidget(m_checkForUpdatesBtn);
    updateGroupLayout->addWidget(m_checkStatus);
    contentLayout->addWidget(updateGroup);

    connect(m_checkForUpdatesBtn, &QPushButton::clicked, this, &BaseSettingsPage::checkForUpdates);

    updateCheckStatus();

    contentLayout->addStretch();

    scrollArea->setWidget(content);
    root->addWidget(scrollArea);
}

void BaseSettingsPage::populateTargets() {
    m_doubleClickTarget->clear();
    m_doubleClickTarget->addItem("Show first library", "first-library");
    m_doubleClickTarget->addItem("Show settings", "settings");

    // add each library (data = stable library id)
    auto libs = m_config->getLibraries();
    for (const auto &lib : libs) {
        QString name = QFileInfo(lib.path).fileName();
        if (!name.isEmpty())
            m_doubleClickTarget->addItem(name, QString::number(lib.id));
    }

    // select current: id match first, then legacy dirName / full path
    QString current = m_config->getDoubleClickTarget();
    for (int i = 0; i < m_doubleClickTarget->count(); ++i) {
        if (m_doubleClickTarget->itemData(i).toString() == current ||
            m_doubleClickTarget->itemText(i) == current ||
            QFileInfo(current).fileName() == m_doubleClickTarget->itemText(i)) {
            m_doubleClickTarget->setCurrentIndex(i);
            break;
        }
    }
}

void BaseSettingsPage::refreshTargets() {
    QString current = m_doubleClickTarget->currentData().toString();
    populateTargets();
    // keep the user's previous selection when it still exists
    for (int i = 0; i < m_doubleClickTarget->count(); ++i) {
        if (m_doubleClickTarget->itemData(i).toString() == current) {
            m_doubleClickTarget->setCurrentIndex(i);
            break;
        }
    }
}

void BaseSettingsPage::updateCheckStatus() {
    if (!m_config->getCheckForUpdatesOnStartup()) {
        m_checkStatus->hide();
        return;
    }

    const UpdateChecker::Result &r = UpdateChecker::lastResult;

    if (!r.success && r.error.isEmpty()) {
        // never checked yet (e.g. settings opened before the startup check finished)
        auto *checker = new UpdateChecker(this);
        connect(checker, &UpdateChecker::finished, this, [this, checker](bool, const QString &, const QString &) {
            checker->deleteLater();
            updateCheckStatus();
        });
        checker->check();
        m_checkStatus->setText("Checking...");
        m_checkStatus->setStyleSheet(QString());
        m_checkStatus->show();
        return;
    }

    if (!r.success) return; // silent on failure

    bool newer = UpdateChecker::compareVersions(r.latestVersion, AppInfo::version()) > 0;
    if (newer) {
        m_checkStatus->setText("Update available: " + r.latestVersion);
        m_checkStatus->setStyleSheet("color: orange;");
    } else {
        m_checkStatus->setText("You are running the latest version: " + AppInfo::version());
        m_checkStatus->setStyleSheet("color: green;");
    }
    m_checkStatus->show();
}

void BaseSettingsPage::checkForUpdates() {
    m_checkStatus->setText("Checking...");
    m_checkStatus->setStyleSheet(QString());
    m_checkStatus->show();
    m_checkForUpdatesBtn->setEnabled(false);

    auto *checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::finished, this, [this, checker](bool success, const QString &latestVersion, const QString &error) {
        checker->deleteLater();
        m_checkForUpdatesBtn->setEnabled(true);

        if (!success) {
            m_checkStatus->setText("Error: " + error);
            m_checkStatus->setStyleSheet("color: red;");
            return;
        }

        int cmp = UpdateChecker::compareVersions(latestVersion, AppInfo::version());
        if (cmp <= 0) {
            QMessageBox::information(this, "Up to Date",
                                     "You are running the latest version: " + AppInfo::version());
        } else {
            auto result = QMessageBox::question(this, "Update Available",
                                                "Current version: " + AppInfo::version() +
                                                    "\nLatest version: " +
                                                    latestVersion +
                                                    "\n\nOpen release page?",
                                                QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes)
                launch(AppInfo::repoUrl() + "/releases/tag/" + latestVersion);
        }
        updateCheckStatus();
    });
    checker->check();
}

void BaseSettingsPage::applyToConfig() {
    QString val = m_doubleClickTarget->currentData().toString();
    if (val.isEmpty())
        val = m_doubleClickTarget->currentText();
    m_config->setDoubleClickTarget(val);
    m_config->setSearchDelayMs(m_searchDelayMs->value());
    m_config->setThumbnailCacheSize(m_thumbnailCacheSize->value());
    m_config->setCheckForUpdatesOnStartup(m_checkOnStartup->isChecked());
    bool nextStartup = m_startWithWindows->isChecked();
    if (nextStartup != m_config->getStartWithWindows())
        applyStartWithWindows(nextStartup);
    m_config->setStartWithWindows(nextStartup);
}
