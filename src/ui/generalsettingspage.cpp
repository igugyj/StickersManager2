#include "generalsettingspage.h"
#include "configmanager.h"
#include "settingswidgets.h"

#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QMessageBox>
#include <QSize>

GeneralSettingsPage::GeneralSettingsPage(ConfigManager *config, QWidget *parent)
    : QWidget(parent), m_config(config)
{
    auto *root = new QVBoxLayout(this);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);

    // --- UI ---
    auto *uiGroup = new QGroupBox("UI");
    auto *uiForm = new QFormLayout(uiGroup);
    m_categoryButtonSize = makeSpinBox(30, 300, config->getCategoryButtonSize(), this);
    m_gridCellSize = makeSpinBox(40, 400, config->getGridCellSize(), this);
    m_recentLimit = makeSpinBox(1, 1000, config->getRecentLimit(), this);
    m_recentEnabled = new QCheckBox(this);
    m_recentEnabled->setChecked(config->recentEnabled());
    uiForm->addRow("Category Button Size:", m_categoryButtonSize);
    uiForm->addRow("Grid Cell Size:", m_gridCellSize);
    uiForm->addRow("Recent Limit:", m_recentLimit);
    uiForm->addRow("Enable Recent Usage:", m_recentEnabled);
    contentLayout->addWidget(uiGroup);

    // --- Behavior ---
    auto *bhvGroup = new QGroupBox("Behavior");
    auto *bhvForm = new QFormLayout(bhvGroup);
    m_copyOnDblClick = new QCheckBox(this);
    m_copyOnDblClick->setChecked(config->copyOnDoubleClick());
    m_highlightOnClick = new QCheckBox(this);
    m_highlightOnClick->setChecked(config->highlightOnClick());
    m_animateThumbnails = new QCheckBox(this);
    m_animateThumbnails->setChecked(config->animateThumbnails());
    auto *animThumbRow = new QWidget(this);
    auto *animThumbLayout = new QHBoxLayout(animThumbRow);
    animThumbLayout->setContentsMargins(0, 0, 0, 0);
    auto *animThumbInfoBtn = makeInfoButton("Animate Thumbnails",
        "Enabling this may cause lag or stutter when the library contains "
        "many animated files. It is not recommended to enable it when there "
        "are too many animated files.", animThumbRow);
    animThumbLayout->addWidget(m_animateThumbnails);
    animThumbLayout->addWidget(animThumbInfoBtn, 0, Qt::AlignLeft);
    animThumbLayout->addStretch();
    m_animatePreview = new QCheckBox(this);
    m_animatePreview->setChecked(config->animatePreview());
    m_showFileTypeTag = new QCheckBox(this);
    m_showFileTypeTag->setChecked(config->showFileTypeTag());
    m_showStickerName = new QCheckBox(this);
    m_showStickerName->setChecked(config->showStickerName());
    m_showStickerSize = new QCheckBox(this);
    m_showStickerSize->setChecked(config->showStickerSize());
    m_showCategoryName = new QCheckBox(this);
    m_showCategoryName->setChecked(config->showCategoryName());
    m_showCategoryCount = new QCheckBox(this);
    m_showCategoryCount->setChecked(config->showCategoryCount());
    bhvForm->addRow("Copy on Double-Click:", m_copyOnDblClick);
    bhvForm->addRow("Highlight on Click:", m_highlightOnClick);
    bhvForm->addRow("Animate Thumbnails:", animThumbRow);
    bhvForm->addRow("Animate Preview:", m_animatePreview);
    bhvForm->addRow("Show File Type Tag:", m_showFileTypeTag);
    bhvForm->addRow("Show Sticker Name:", m_showStickerName);
    bhvForm->addRow("Show Sticker Size:", m_showStickerSize);
    bhvForm->addRow("Show Category Name:", m_showCategoryName);
    bhvForm->addRow("Show Category Count:", m_showCategoryCount);
    contentLayout->addWidget(bhvGroup);

    // --- Window ---
    auto *winGroup = new QGroupBox("Window");
    auto *winForm = new QFormLayout(winGroup);
    QSize winSize = config->getWindowSize();
    QPoint winPos = config->getWindowPosition();
    m_winPosX = makeSpinBox(-9999, 9999, winPos.x(), this);
    m_winPosY = makeSpinBox(-9999, 9999, winPos.y(), this);
    m_winW = makeSpinBox(200, 9999, winSize.width(), this);
    m_winH = makeSpinBox(200, 9999, winSize.height(), this);
    m_alwaysOnTop = new QCheckBox(this);
    m_alwaysOnTop->setChecked(config->getDefaultAlwaysOnTop());
    winForm->addRow("Position X:", m_winPosX);
    winForm->addRow("Position Y:", m_winPosY);
    winForm->addRow("Width:", m_winW);
    winForm->addRow("Height:", m_winH);
    winForm->addRow("Always on Top:", m_alwaysOnTop);
    contentLayout->addWidget(winGroup);

    // --- Reset ---
    auto *resetBtn = new QPushButton("Reset to Defaults");
    connect(resetBtn, &QPushButton::clicked, this, &GeneralSettingsPage::resetToDefaults);
    contentLayout->addWidget(resetBtn);

    contentLayout->addStretch();

    scrollArea->setWidget(content);
    root->addWidget(scrollArea);
}

void GeneralSettingsPage::applyToConfig() {
    QJsonObject cfg = m_config->config();
    QJsonObject def = cfg["default"].toObject();

    QJsonObject ui = def["ui"].toObject();
    ui["categoryButtonSize"] = m_categoryButtonSize->value();
    ui["gridCellSize"] = m_gridCellSize->value();
    ui["recentLimit"] = m_recentLimit->value();
    ui["recentEnabled"] = m_recentEnabled->isChecked();
    def["ui"] = ui;

    QJsonObject bhv = def["behavior"].toObject();
    bhv["copyOnDoubleClick"] = m_copyOnDblClick->isChecked();
    bhv["highlightOnClick"] = m_highlightOnClick->isChecked();
    bhv["animateThumbnails"] = m_animateThumbnails->isChecked();
    bhv["animatePreview"] = m_animatePreview->isChecked();
    bhv["showFileTypeTag"] = m_showFileTypeTag->isChecked();
    bhv["showStickerName"] = m_showStickerName->isChecked();
    bhv["showStickerSize"] = m_showStickerSize->isChecked();
    bhv["showCategoryName"] = m_showCategoryName->isChecked();
    bhv["showCategoryCount"] = m_showCategoryCount->isChecked();
    def["behavior"] = bhv;

    QJsonObject win = def["window"].toObject();
    QJsonArray pos = {m_winPosX->value(), m_winPosY->value()};
    QJsonArray size = {m_winW->value(), m_winH->value()};
    win["position"] = pos;
    win["size"] = size;
    win["alwaysOnTop"] = m_alwaysOnTop->isChecked();
    def["window"] = win;

    cfg["default"] = def;
    m_config->setConfig(cfg);
}

void GeneralSettingsPage::resetToDefaults() {
    // getDefaultConfig() is built from hardDefaults(), so every key exists
    // and needs no fallback value here (single source of truth).
    QJsonObject def = m_config->getDefaultConfig()["default"].toObject();

    QJsonObject ui = def["ui"].toObject();
    m_categoryButtonSize->setValue(ui["categoryButtonSize"].toInt());
    m_gridCellSize->setValue(ui["gridCellSize"].toInt());
    m_recentLimit->setValue(ui["recentLimit"].toInt());
    m_recentEnabled->setChecked(ui["recentEnabled"].toBool());

    QJsonObject bhv = def["behavior"].toObject();
    m_copyOnDblClick->setChecked(bhv["copyOnDoubleClick"].toBool());
    m_highlightOnClick->setChecked(bhv["highlightOnClick"].toBool());
    m_animateThumbnails->setChecked(bhv["animateThumbnails"].toBool());
    m_animatePreview->setChecked(bhv["animatePreview"].toBool());
    m_showFileTypeTag->setChecked(bhv["showFileTypeTag"].toBool());
    m_showStickerName->setChecked(bhv["showStickerName"].toBool());
    m_showStickerSize->setChecked(bhv["showStickerSize"].toBool());
    m_showCategoryName->setChecked(bhv["showCategoryName"].toBool());
    m_showCategoryCount->setChecked(bhv["showCategoryCount"].toBool());

    QJsonObject win = def["window"].toObject();
    QJsonArray pos = win["position"].toArray();
    QJsonArray size = win["size"].toArray();
    m_winPosX->setValue(pos[0].toInt());
    m_winPosY->setValue(pos[1].toInt());
    m_winW->setValue(size[0].toInt());
    m_winH->setValue(size[1].toInt());
    m_alwaysOnTop->setChecked(win["alwaysOnTop"].toBool());
}
