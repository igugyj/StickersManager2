#include "settingsdialog.h"
#include "generalsettingspage.h"
#include "librarysettingspage.h"
#include "basesettingspage.h"
#include "aboutpage.h"
#include "configmanager.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(ConfigManager *config, bool keepOpenOnSave, QWidget *parent)
    : QDialog(parent), m_config(config), m_keepOpenOnSave(keepOpenOnSave)
{
    setWindowTitle("Settings");
    setMinimumSize(600, 500);

    Qt::WindowFlags flags = windowFlags() | Qt::WindowMaximizeButtonHint;
    if (m_config->getDefaultAlwaysOnTop())
        flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);

    auto *layout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    m_generalPage = new GeneralSettingsPage(config, this);
    m_libraryPage = new LibrarySettingsPage(config, this);
    m_basePage = new BaseSettingsPage(config, this);
    m_aboutPage = new AboutPage(config, this);

    m_tabWidget->addTab(m_generalPage, "General");
    m_tabWidget->addTab(m_libraryPage, "Libraries");
    m_tabWidget->addTab(m_basePage, "Base");
    m_tabWidget->addTab(m_aboutPage, "About");

    layout->addWidget(m_tabWidget);

    // library ids are renumbered on drag-reorder; refresh the target combo when Base is shown.
    // 跨页冲突刷新：设置热键 ↔ 库热键双向标红，切页时按对方最新状态重算。
    int baseTabIndex = m_tabWidget->indexOf(m_basePage);
    int libTabIndex = m_tabWidget->indexOf(m_libraryPage);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this, baseTabIndex, libTabIndex](int index) {
        if (index == baseTabIndex) {
            m_basePage->refreshTargets();
            m_basePage->refreshHotkeyConflict();
        } else if (index == libTabIndex) {
            m_libraryPage->validateAllHotkeys();
        }
    });

    auto *saveBtn = new QPushButton("Save && Reload");
    auto *cancelBtn = new QPushButton("Cancel");

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

bool SettingsDialog::isHotkeyRecording() const {
    return m_basePage && m_basePage->isHotkeyRecording();
}

void SettingsDialog::onSave() {
    m_generalPage->applyToConfig();
    if (!m_libraryPage->collectToConfig())
        return;
    m_basePage->applyToConfig();

    m_config->saveConfig();
    m_config->saveSettings();

    // keep the topmost state in sync with the General "Always on Top" default
    bool aot = m_config->getDefaultAlwaysOnTop();
    if (aot != bool(windowFlags() & Qt::WindowStaysOnTopHint)) {
        setWindowFlag(Qt::WindowStaysOnTopHint, aot);
        if (isVisible())
            show();
    }

    if (m_keepOpenOnSave)
        emit applied();
    else
        accept();
}
