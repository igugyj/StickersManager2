#include "aboutpage.h"
#include "appinfo.h"
#include "configmanager.h"
#include "launcher.hpp"
#include "fsutil.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QScrollArea>

AboutPage::AboutPage(ConfigManager *config, QWidget *parent)
    : QWidget(parent), m_config(config)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget();
    auto *contentLayout = new QVBoxLayout(content);

    auto *infoGroup = new QGroupBox("Application");
    auto *infoForm = new QFormLayout(infoGroup);

    infoForm->addRow("Name:", new QLabel(AppInfo::name(), this));
    infoForm->addRow("Version:", new QLabel(AppInfo::version(), this));
    infoForm->addRow("Author:", new QLabel(AppInfo::author(), this));
    infoForm->addRow("License:", new QLabel(AppInfo::license(), this));

    auto *repoLink = new HightLabel(AppInfo::repoUrl(), this);
    repoLink->setCursor(Qt::PointingHandCursor);
    connect(repoLink, &QLabel::linkActivated, []() { launch(AppInfo::repoUrl()); });
    infoForm->addRow("Repository:", repoLink);

    auto *issuesLink = new HightLabel("issue", this);
    issuesLink->setCursor(Qt::PointingHandCursor);
    connect(issuesLink, &QLabel::linkActivated, []() { launch(AppInfo::issuesUrl()); });
    infoForm->addRow("issue:", issuesLink);

    contentLayout->addWidget(infoGroup);

    // Paths
    auto *pathsGroup = new QGroupBox("Paths");
    auto *pathsForm = new QFormLayout(pathsGroup);

    auto openLink = [](const QString &path) {
        launch(path);
    };

    QString appPath = QCoreApplication::applicationFilePath();
    auto *appPathLink = new HightLabel(appPath, this);
    appPathLink->setCursor(Qt::PointingHandCursor);
    connect(appPathLink, &QLabel::linkActivated, this, [appPath]() { launch(QFileInfo(appPath).absolutePath()); });
    pathsForm->addRow("Executable:", appPathLink);

    QString cfgDir = QFileInfo(m_config->getConfigPath()).absolutePath();
    auto *cfgLink = new HightLabel(cfgDir, this);
    cfgLink->setCursor(Qt::PointingHandCursor);
    connect(cfgLink, &QLabel::linkActivated, this, [cfgDir]() { launch(cfgDir); });
    pathsForm->addRow("Config folder:", cfgLink);

    contentLayout->addWidget(pathsGroup);

    // Storage
    auto *storageGroup = new QGroupBox("Storage");
    auto *storageForm = new QFormLayout(storageGroup);

    qint64 appDirBytes = dirSize(QCoreApplication::applicationDirPath());
    storageForm->addRow("App directory:", new QLabel(formatBytes(appDirBytes), this));

    qint64 libBytes = 0;
    auto libs = m_config->getLibraries();
    for (const auto &lib : libs) {
        if (lib.path.isEmpty()) continue;
        libBytes += dirSize(lib.path);
    }
    m_storageLabel = new QLabel(formatBytes(libBytes), this);
    storageForm->addRow("Library files:", m_storageLabel);

    contentLayout->addWidget(storageGroup);

    contentLayout->addStretch();

    scrollArea->setWidget(content);
    root->addWidget(scrollArea);
}

HightLabel::HightLabel(const QString &text, QWidget *parent, Qt::WindowFlags f)
{
    setText("<a href='#' style='color: palette(highlight);'>" + text + "</a>");
    setParent(parent);
    setWindowFlags(f);
}