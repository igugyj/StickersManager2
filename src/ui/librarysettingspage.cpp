#include "librarysettingspage.h"
#include "configmanager.h"
#include "hotkeycapture.h"
#include "convertcodetostring.hpp"
#include "settingswidgets.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QMessageBox>
#include <QSize>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QTimer>
#include <QFontDatabase>
#include <QTableWidget>
#include <QHeaderView>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <functional>

#include "imageloader.h"
#include "fsutil.hpp"

static QStringList computeLibraryStats(const QString &libPath)
{
    struct CatInfo
    {
        int count = 0;
        qint64 bytes = 0;
    };
    QDir rootDir(libPath);
    if (!rootDir.exists())
        return QStringList();

    QHash<QString, CatInfo> info;
    QStringList order;
    forEachStickerFile(libPath, [&](const QString &categoryName, const QFileInfo &fi) {
        if (!info.contains(categoryName)) {
            info[categoryName] = CatInfo{};
            order.append(categoryName);
        }
        info[categoryName].bytes += fi.size();
        if (ImageLoader::isFormatSupported(fi.absoluteFilePath()))
            ++info[categoryName].count;
    });

    qint64 totalBytes = 0;
    int totalStickers = 0;
    for (const QString &categoryName : order) {
        totalBytes += info[categoryName].bytes;
        totalStickers += info[categoryName].count;
    }

    QStringList result;
    result.append(QString("%1 (%2 categories)\nTotal: %3 stickers, %4")
                      .arg(rootDir.dirName())
                      .arg(order.size())
                      .arg(totalStickers)
                      .arg(formatBytes(totalBytes)));
    for (const QString &categoryName : order)
        result.append(QString("%1\t%2\t%3").arg(categoryName)
                          .arg(info[categoryName].count)
                          .arg(formatBytes(info[categoryName].bytes)));
    return result;
}

static const char *kLibraryDragMime = "application/x-stickersmanager-library";
static const char *kCardDragStyle = "QGroupBox { border: 2px dashed #3a7bd5; background: rgba(58, 123, 213, 0.08); }";
static const char *kCardFlashStyle = "QGroupBox { border: 2px solid #3a7bd5; background: rgba(58, 123, 213, 0.10); }";

class DraggableLibraryCard : public QGroupBox
{
public:
    explicit DraggableLibraryCard(QWidget *parent = nullptr) : QGroupBox(parent) {}

    int m_listIndex = -1;
    std::function<void(int, int)> reorderCallback;

    void setDragging(bool dragging)
    {
        m_dragging = dragging;
        if (dragging)
            setStyleSheet(QLatin1String(kCardDragStyle));
        else
            setStyleSheet(m_flash ? QLatin1String(kCardFlashStyle) : QString());
    }

    void flashDrop()
    {
        m_flash = true;
        setStyleSheet(QLatin1String(kCardFlashStyle));
        QTimer::singleShot(450, this, [this]()
                           {
            m_flash = false;
            if (!m_dragging)
                setStyleSheet(QString()); });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonRelease)
        {
            handleMouseEvent(static_cast<QMouseEvent *>(event));
        }
        return QGroupBox::eventFilter(watched, event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        handleMouseEvent(event);
        QGroupBox::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        handleMouseEvent(event);
        QGroupBox::mouseMoveEvent(event);
    }

private:
    void handleMouseEvent(QMouseEvent *event)
    {
        if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::LeftButton)
        {
            m_pressPos = event->globalPosition();
        }
        else if (event->type() == QEvent::MouseMove &&
                 (event->buttons() & Qt::LeftButton) &&
                 (event->globalPosition() - m_pressPos).manhattanLength() >= QApplication::startDragDistance())
        {
            auto *mime = new QMimeData;
            mime->setData(QLatin1String(kLibraryDragMime), QByteArray::number(m_listIndex));
            auto *drag = new QDrag(this);
            drag->setMimeData(mime);
            int origIndex = m_listIndex;
            setDragging(true);
            Qt::DropAction action = drag->exec(Qt::MoveAction);
            setDragging(false);
            if (action == Qt::IgnoreAction && m_listIndex != origIndex && reorderCallback)
                reorderCallback(m_listIndex, origIndex);
            if (action == Qt::MoveAction)
                flashDrop();
        }
    }

    QPointF m_pressPos;
    bool m_dragging = false;
    bool m_flash = false;
};

class LibraryListContainer : public QWidget
{
public:
    explicit LibraryListContainer(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAcceptDrops(true);
        m_autoScrollTimer = new QTimer(this);
        m_autoScrollTimer->setInterval(40);
        connect(m_autoScrollTimer, &QTimer::timeout, this, &LibraryListContainer::autoScroll);
    }

    std::function<void(int, int)> reorderCallback;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat(QLatin1String(kLibraryDragMime)))
            event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (!event->mimeData()->hasFormat(QLatin1String(kLibraryDragMime)))
            return;
        auto *card = dynamic_cast<DraggableLibraryCard *>(event->source());
        if (!card)
            return;
        int target = cardUnder(event->position().toPoint());
        if (target >= 0 && target != card->m_listIndex && reorderCallback)
            reorderCallback(card->m_listIndex, target);

        m_lastPos = event->position().toPoint();
        updateAutoScroll();
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        m_autoScrollTimer->stop();
        QWidget::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        m_autoScrollTimer->stop();
        event->acceptProposedAction();
    }

private:
    void updateAutoScroll()
    {
        auto *sa = qobject_cast<QScrollArea *>(parentWidget() ? parentWidget()->parentWidget() : nullptr);
        auto *vbar = sa ? sa->verticalScrollBar() : nullptr;
        if (!sa || !vbar) {
            m_autoScrollTimer->stop();
            return;
        }
        const QPoint vp = mapTo(sa->viewport(), m_lastPos);
        const int edge = 40;
        if (vp.y() <= edge) {
            m_scrollDir = -1;
            m_autoScrollTimer->start();
        } else if (vp.y() >= sa->viewport()->height() - edge) {
            m_scrollDir = 1;
            m_autoScrollTimer->start();
        } else {
            m_autoScrollTimer->stop();
        }
    }

    void autoScroll()
    {
        auto *sa = qobject_cast<QScrollArea *>(parentWidget() ? parentWidget()->parentWidget() : nullptr);
        auto *vbar = sa ? sa->verticalScrollBar() : nullptr;
        if (!sa || !vbar) {
            m_autoScrollTimer->stop();
            return;
        }
        updateAutoScroll();
        if (m_autoScrollTimer->isActive())
            vbar->setValue(vbar->value() + m_scrollDir * 20);
    }

    int cardUnder(const QPoint &pos)
    {
        QWidget *w = childAt(pos);
        while (w)
        {
            if (auto *card = dynamic_cast<DraggableLibraryCard *>(w))
                return card->m_listIndex;
            w = w->parentWidget();
        }
        return -1;
    }

    QTimer *m_autoScrollTimer = nullptr;
    QPoint m_lastPos;
    int m_scrollDir = 0;
};

LibrarySettingsPage::LibrarySettingsPage(ConfigManager *config, QWidget *parent)
    : QWidget(parent), m_config(config)
{
    auto *root = new QVBoxLayout(this);

    auto *addBtn = new QPushButton("+ Add Library");
    addBtn->setFixedWidth(200);
    auto *topLayout = new QHBoxLayout;
    topLayout->addStretch();
    topLayout->addWidget(addBtn);
    topLayout->addStretch();
    root->addLayout(topLayout);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);

    auto *content = new LibraryListContainer;
    content->reorderCallback = [this](int a, int b)
    { swapLibraries(a, b); };
    auto *contentLayout = new QVBoxLayout(content);
    m_listLayout = new QVBoxLayout;
    contentLayout->addLayout(m_listLayout);
    contentLayout->addStretch();

    scrollArea->setWidget(content);
    root->addWidget(scrollArea);

    connect(addBtn, &QPushButton::clicked, this, &LibrarySettingsPage::addLibrary);

    buildFromConfig();
}

void LibrarySettingsPage::buildFromConfig() {
    m_libs.clear();
    auto libs = m_config->getLibraries();
    for (const auto &lib : libs)
        m_libs.append(LibraryEditWidgets{});
    rebuildList();
}

void LibrarySettingsPage::rebuildList() {
    while (auto *item = m_listLayout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto libs = m_config->getLibraries();
    for (int i = 0; i < m_libs.size(); ++i) {
        // match config entry by path so unsaved reorders survive rebuilds
        LibraryConfig libCfg;
        QString path = m_libs[i].pathEdit ? m_libs[i].pathEdit->text() : QString();
        if (!path.isEmpty())
        {
            for (const auto &lib : libs)
            {
                if (lib.path == path)
                {
                    libCfg = lib;
                    break;
                }
            }
        }
        if (libCfg.path.isEmpty() && i < libs.size())
            libCfg = libs[i]; // first build: align with config order

        auto *card = new DraggableLibraryCard;
        card->m_listIndex = i;
        card->reorderCallback = [this](int a, int b)
        { swapLibraries(a, b); };
        auto *cardLayout = new QVBoxLayout(card);

        // drag handle
        auto *handle = new QLabel(":::");
        handle->setAlignment(Qt::AlignCenter);
        handle->setFixedHeight(18);
        handle->setCursor(Qt::SizeAllCursor);
        handle->setToolTip("Drag to reorder");
        handle->setStyleSheet("QLabel { color: #999999; }");
        handle->installEventFilter(card);
        auto *handleRow = new QHBoxLayout;
        handleRow->addStretch();
        handleRow->addWidget(handle);
        handleRow->addStretch();
        cardLayout->addLayout(handleRow);

        // Row 1: path + browse
        auto *pathRow = new QHBoxLayout;
        auto *pathEdit = new QLineEdit(libCfg.path);
        pathEdit->setPlaceholderText("Path to sticker library...");
        pathEdit->setMinimumWidth(300);
        auto *browseBtn = new QPushButton("Browse");
        pathRow->addWidget(pathEdit);
        pathRow->addWidget(browseBtn);

        // Row 2: hotkey + enabled
        int idx = i;
        auto *hkRow = new QHBoxLayout;
        auto *hotkeyBtn = new HotkeyCaptureButton;
        hotkeyBtn->setConflictChecker([this, idx](const QString &hk) {
            return hotkeyInUse(idx, hk);
        });
        hotkeyBtn->setHotkey(libCfg.hotkey);
        auto *enabledCheck = new QCheckBox("Enable Hotkey");
        enabledCheck->setChecked(libCfg.enabled);
        auto *delBtn = new QPushButton("Delete");
        hkRow->addWidget(new QLabel("Hotkey:"));
        hkRow->addWidget(hotkeyBtn, 1);
        hkRow->addWidget(enabledCheck);
        hkRow->addWidget(delBtn);

        // Overrides toggle
        auto *overrideToggle = new QPushButton("Show Overrides");
        overrideToggle->setCheckable(true);
        overrideToggle->setChecked(false);

        // Override content
        auto *overrideWidget = new QWidget;
        auto *ovLayout = new QVBoxLayout(overrideWidget);
        ovLayout->setContentsMargins(0, 0, 0, 0);

        // -- Window overrides --
        auto *winOv = new QGroupBox("Window");
        auto *winOvForm = new QFormLayout(winOv);
        QJsonObject winSettings = libCfg.settings["window"].toObject();
        QPoint pos = winSettings["position"].toArray().size() == 2
                         ? QPoint(winSettings["position"].toArray()[0].toInt(),
                                  winSettings["position"].toArray()[1].toInt())
                         : m_config->getWindowPosition();
        QSize sz = winSettings["size"].toArray().size() == 2
                       ? QSize(winSettings["size"].toArray()[0].toInt(),
                               winSettings["size"].toArray()[1].toInt())
                       : m_config->getWindowSize();
        bool useCustom = winSettings.contains("customGeometry")
                             ? winSettings["customGeometry"].toBool()
                             : false;
        auto *useCustomGeometry = new QCheckBox("Use custom geometry", this);
        useCustomGeometry->setChecked(useCustom);
        auto *winPosX = makeSpinBox(-9999, 9999, pos.x(), this);
        auto *winPosY = makeSpinBox(-9999, 9999, pos.y(), this);
        auto *winW = makeSpinBox(0, 9999, sz.width(), this);
        auto *winH = makeSpinBox(0, 9999, sz.height(), this);
        for (QSpinBox *sb : {winPosX, winPosY, winW, winH})
            sb->setEnabled(useCustom);
        connect(useCustomGeometry, &QCheckBox::toggled, this, [winPosX, winPosY, winW, winH](bool checked) {
            for (QSpinBox *sb : {winPosX, winPosY, winW, winH})
                sb->setEnabled(checked);
        });
        auto *alwaysOnTop = makeBoolCombo(
            winSettings.contains("alwaysOnTop")
                ? boolToCombo(winSettings["alwaysOnTop"].toBool())
                : "General", this);

        auto *posRow = new QHBoxLayout;
        posRow->addWidget(new QLabel("X:"));
        posRow->addWidget(winPosX);
        posRow->addWidget(new QLabel("Y:"));
        posRow->addWidget(winPosY);
        auto *sizeRow = new QHBoxLayout;
        sizeRow->addWidget(new QLabel("W:"));
        sizeRow->addWidget(winW);
        sizeRow->addWidget(new QLabel("H:"));
        sizeRow->addWidget(winH);

        winOvForm->addRow(useCustomGeometry);
        winOvForm->addRow("Position:", posRow);
        winOvForm->addRow("Size:", sizeRow);
        winOvForm->addRow("Always on Top:", alwaysOnTop);
        ovLayout->addWidget(winOv);

        // -- UI overrides --
        auto *uiOv = new QGroupBox("UI");
        auto *uiOvForm = new QFormLayout(uiOv);
        auto *ovGridCellSize = makeOverrideSpinBox(0, 400, libCfg.settings["ui"].toObject()["gridCellSize"].toInt(0), this);
        auto *ovCategoryBtnSize = makeOverrideSpinBox(0, 300, libCfg.settings["ui"].toObject()["categoryButtonSize"].toInt(0), this);
        auto *ovRecentLimit = makeOverrideSpinBox(0, 1000, libCfg.settings["ui"].toObject()["recentLimit"].toInt(0), this);
        auto *ovRecentEnabled = makeBoolCombo(
            libCfg.settings["ui"].toObject().contains("recentEnabled")
                ? boolToCombo(libCfg.settings["ui"].toObject()["recentEnabled"].toBool())
                : "General", this);
        uiOvForm->addRow("Grid Cell Size:", ovGridCellSize);
        uiOvForm->addRow("Category Button Size:", ovCategoryBtnSize);
        uiOvForm->addRow("Recent Limit:", ovRecentLimit);
        uiOvForm->addRow("Enable Recent Usage:", ovRecentEnabled);
        ovLayout->addWidget(uiOv);

        // -- Behavior overrides --
        auto *bhvOv = new QGroupBox("Behavior");
        auto *bhvOvForm = new QFormLayout(bhvOv);
        auto *ovCopyDbl = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("copyOnDoubleClick")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["copyOnDoubleClick"].toBool())
                : "General", this);
        auto *ovHighlight = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("highlightOnClick")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["highlightOnClick"].toBool())
                : "General", this);
        auto *ovAnimThumb = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("animateThumbnails")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["animateThumbnails"].toBool())
                : "General", this);
        auto *animThumbRow = new QWidget(this);
        auto *animThumbLayout = new QHBoxLayout(animThumbRow);
        animThumbLayout->setContentsMargins(0, 0, 0, 0);
        auto *animThumbInfoBtn = makeInfoButton("Animate Thumbnails",
            "Enabling this may cause lag or stutter when the library contains "
            "many animated files. It is not recommended to enable it when there "
            "are too many animated files.", animThumbRow);
        animThumbLayout->addWidget(ovAnimThumb, 1);
        animThumbLayout->addWidget(animThumbInfoBtn, 0, Qt::AlignLeft);
        auto *ovAnimPrev = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("animatePreview")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["animatePreview"].toBool())
                : "General", this);
        auto *ovTag = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("showFileTypeTag")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["showFileTypeTag"].toBool())
                : "General", this);
        auto *ovStickerName = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("showStickerName")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["showStickerName"].toBool())
                : "General", this);
        auto *ovStickerSize = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("showStickerSize")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["showStickerSize"].toBool())
                : "General", this);
        auto *ovCategoryName = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("showCategoryName")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["showCategoryName"].toBool())
                : "General", this);
        auto *ovCategoryCount = makeBoolCombo(
            libCfg.settings["behavior"].toObject().contains("showCategoryCount")
                ? boolToCombo(libCfg.settings["behavior"].toObject()["showCategoryCount"].toBool())
                : "General", this);
        bhvOvForm->addRow("Copy on Double-Click:", ovCopyDbl);
        bhvOvForm->addRow("Highlight on Click:", ovHighlight);
        bhvOvForm->addRow("Animate Thumbnails:", animThumbRow);
        bhvOvForm->addRow("Animate Preview:", ovAnimPrev);
        bhvOvForm->addRow("Show File Type Tag:", ovTag);
        bhvOvForm->addRow("Show Sticker Name:", ovStickerName);
        bhvOvForm->addRow("Show Sticker Size:", ovStickerSize);
        bhvOvForm->addRow("Show Category Name:", ovCategoryName);
        bhvOvForm->addRow("Show Category Count:", ovCategoryCount);
        ovLayout->addWidget(bhvOv);

        auto *resetBtn = new QPushButton("Reset", this);
        connect(resetBtn, &QPushButton::clicked, this, [this, useCustomGeometry, winPosX, winPosY, winW, winH,
                                                        alwaysOnTop, ovGridCellSize, ovCategoryBtnSize, ovRecentLimit,
                                                        ovRecentEnabled, ovCopyDbl, ovHighlight, ovAnimThumb, ovAnimPrev, ovTag,
                                                        ovStickerName, ovStickerSize, ovCategoryName, ovCategoryCount]() {
            useCustomGeometry->setChecked(false);
            QPoint defPos = m_config->getWindowPosition();
            QSize defSize = m_config->getWindowSize();
            winPosX->setValue(defPos.x());
            winPosY->setValue(defPos.y());
            winW->setValue(defSize.width());
            winH->setValue(defSize.height());
            alwaysOnTop->setCurrentIndex(0);
            ovGridCellSize->setValue(0);
            ovCategoryBtnSize->setValue(0);
            ovRecentLimit->setValue(0);
            ovRecentEnabled->setCurrentIndex(0);
            for (QComboBox *cb : {ovCopyDbl, ovHighlight, ovAnimThumb, ovAnimPrev, ovTag,
                                  ovStickerName, ovStickerSize, ovCategoryName, ovCategoryCount})
                cb->setCurrentIndex(0);
        });
        ovLayout->addWidget(resetBtn);

        overrideWidget->setVisible(false);

        LibraryEditWidgets w;
        w.pathEdit = pathEdit;
        w.hotkeyBtn = hotkeyBtn;
        w.enabledCheck = enabledCheck;
        w.useCustomGeometry = useCustomGeometry;
        w.winPosX = winPosX; w.winPosY = winPosY;
        w.winW = winW; w.winH = winH;
        w.alwaysOnTop = alwaysOnTop;
        w.gridCellSize = ovGridCellSize;
        w.categoryButtonSize = ovCategoryBtnSize;
        w.recentLimit = ovRecentLimit;
        w.recentEnabled = ovRecentEnabled;
        w.copyOnDblClick = ovCopyDbl;
        w.highlightOnClick = ovHighlight;
        w.animateThumbnails = ovAnimThumb;
        w.animatePreview = ovAnimPrev;
        w.showFileTypeTag = ovTag;
        w.showStickerName = ovStickerName;
        w.showStickerSize = ovStickerSize;
        w.showCategoryName = ovCategoryName;
        w.showCategoryCount = ovCategoryCount;
        w.overrideWidget = overrideWidget;
        w.libId = libCfg.id;
        m_libs[i] = w;

        cardLayout->addLayout(pathRow);
        cardLayout->addLayout(hkRow);
        cardLayout->addWidget(overrideToggle);
        cardLayout->addWidget(overrideWidget);

        // Statistic stats toggle
        auto *statsToggle = new QPushButton("Show Statistic", card);
        statsToggle->setCheckable(true);
        statsToggle->setChecked(false);
        auto *statsWidget = new QWidget(card);
        statsWidget->setVisible(false);
        auto *statsLayout = new QVBoxLayout(statsWidget);
        statsLayout->setContentsMargins(0, 0, 0, 0);
        auto *statsLabel = new QLabel(statsWidget);
        statsLabel->setWordWrap(true);
        statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        statsLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        statsLayout->addWidget(statsLabel);
        auto *statsTable = new QTableWidget(statsWidget);
        statsTable->setColumnCount(3);
        statsTable->setHorizontalHeaderLabels({"Category", "Stickers", "Size"});
        statsTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
        statsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        statsTable->verticalHeader()->setVisible(false);
        statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        statsTable->setSelectionMode(QAbstractItemView::NoSelection);
        statsTable->setAlternatingRowColors(true);
        statsTable->setVisible(false);
        statsLayout->addWidget(statsTable);
        auto *statsWatcher = new QFutureWatcher<QStringList>(card);
        connect(statsWatcher, &QFutureWatcher<QStringList>::finished, statsToggle, [statsToggle, statsLabel, statsTable, statsWatcher]()
                {
            if (!statsToggle->isChecked())
                return;
            QStringList data = statsWatcher->result();
            if (data.isEmpty()) {
                statsLabel->setText("No categories found");
                statsTable->setVisible(false);
                return;
            }
            statsLabel->setText(data.first());
            bool hasRows = data.size() > 1;
            statsTable->setVisible(hasRows);
            statsTable->setRowCount(hasRows ? data.size() - 1 : 0);
            for (int r = 0; r < data.size() - 1; ++r) {
                const QStringList parts = data[r + 1].split('\t');
                for (int c = 0; c < 3; ++c) {
                    auto *item = new QTableWidgetItem(parts.value(c));
                    item->setTextAlignment(Qt::AlignCenter);
                    statsTable->setItem(r, c, item);
                }
            } });
        connect(statsToggle, &QPushButton::toggled, this, [this, pathEdit, statsWidget, statsToggle, statsWatcher](bool checked)
                {
            statsWidget->setVisible(checked);
            if (!checked || statsWatcher->isRunning())
                return;
            QString path = pathEdit->text().trimmed();
            if (path.isEmpty() || !QDir(path).exists()) {
                QMessageBox::warning(this, "Library Structure",
                    "Please set a valid library path first.");
                statsToggle->setChecked(false);
                return;
            }
            statsWatcher->setFuture(QtConcurrent::run([path]() { return computeLibraryStats(path); })); });
        cardLayout->addWidget(statsToggle);
        cardLayout->addWidget(statsWidget);

        w.statsToggle = statsToggle;
        w.statsWidget = statsWidget;
        w.statsLabel = statsLabel;
        w.statsTable = statsTable;
        w.statsWatcher = statsWatcher;

        connect(hotkeyBtn, &HotkeyCaptureButton::hotkeyChanged, this, &LibrarySettingsPage::validateAllHotkeys);
        connect(enabledCheck, &QCheckBox::toggled, this, &LibrarySettingsPage::validateAllHotkeys);
        connect(browseBtn, &QPushButton::clicked, this, [this, idx]() { browseLibrary(idx); });
        connect(delBtn, &QPushButton::clicked, this, [this, idx]() { removeLibrary(idx); });
        connect(overrideToggle, &QPushButton::toggled, this, [this, idx](bool checked) {
            if (idx < m_libs.size() && m_libs[idx].overrideWidget)
                m_libs[idx].overrideWidget->setVisible(checked);
            auto *btn = qobject_cast<QPushButton *>(sender());
            if (btn) btn->setText(checked ? "Hide Overrides" : "Show Overrides");
        });

        m_listLayout->addWidget(card);
    }
    syncCardIndices();
    validateAllHotkeys();
}

void LibrarySettingsPage::swapLibraries(int a, int b)
{
    if (a < 0 || b < 0 || a >= m_libs.size() || b >= m_libs.size() || a == b)
        return;

    QLayoutItem *item = m_listLayout->takeAt(a);
    m_listLayout->insertItem(b, item);
    m_libs.swapItemsAt(a, b);

    // id is the order marker: renumber 0..n-1 in the new visual order
    for (int i = 0; i < m_libs.size(); ++i)
        m_libs[i].libId = i;

    syncCardIndices();
    validateAllHotkeys();
}

void LibrarySettingsPage::syncCardIndices()
{
    for (int i = 0; i < m_listLayout->count(); ++i)
    {
        if (auto *card = dynamic_cast<DraggableLibraryCard *>(m_listLayout->itemAt(i)->widget()))
            card->m_listIndex = i;
    }
}

void LibrarySettingsPage::addLibrary() {
    QString path = QFileDialog::getExistingDirectory(this, "Select Sticker Library Folder", QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) return;

    QString canonical = QDir(path).canonicalPath();
    auto libs = m_config->getLibraries();
    for (const auto &lib : libs)
        if (QDir(lib.path).canonicalPath() == canonical)
            return;

    QChar letter = QChar(static_cast<char>('E' + qMin(static_cast<int>(libs.size()), 25)));
    QString hk = QString("Ctrl+Shift+%1").arg(letter);
    LibraryConfig lib(path, hk, true);
    libs.append(lib);
    m_config->setLibraries(libs);

    LibraryEditWidgets w = {};
    m_libs.append(w);
    rebuildList();
}

void LibrarySettingsPage::removeLibrary(int index) {
    if (index < 0 || index >= m_libs.size()) return;
    QString path = m_libs[index].pathEdit ? m_libs[index].pathEdit->text() : QString();
    auto libs = m_config->getLibraries();
    if (!path.isEmpty())
    {
        for (int i = 0; i < libs.size(); ++i)
        {
            if (libs[i].path == path)
            {
                libs.remove(i);
                break;
            }
        }
    }
    m_config->setLibraries(libs);
    m_libs.remove(index);
    rebuildList();
}

void LibrarySettingsPage::browseLibrary(int index) {
    if (index < 0 || index >= m_libs.size()) return;
    QString path = QFileDialog::getExistingDirectory(this, "Select Sticker Library Folder",
                                                     m_libs[index].pathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
    if (!path.isEmpty()) {
        m_libs[index].pathEdit->setText(path);
    }
}

LibraryConfig LibrarySettingsPage::collectOne(int index) const {
    if (index < 0 || index >= m_libs.size()) return {};

    const auto &w = m_libs[index];
    LibraryConfig lib;
    lib.id = w.libId;
    lib.path = w.pathEdit->text();
    lib.hotkey = w.hotkeyBtn->hotkey();
    lib.enabled = w.enabledCheck->isChecked();

    QJsonObject settings;

    // Window
    QJsonObject win;
    win["customGeometry"] = w.useCustomGeometry->isChecked();
    if (w.useCustomGeometry->isChecked()) {
        QJsonArray pos = {w.winPosX->value(), w.winPosY->value()};
        QJsonArray sz = {w.winW->value(), w.winH->value()};
        win["position"] = pos;
        win["size"] = sz;
    }
    if (w.alwaysOnTop->currentText() != "General")
        win["alwaysOnTop"] = (w.alwaysOnTop->currentText() == "On");
    if (!win.isEmpty()) settings["window"] = win;

    // UI
    QJsonObject ui;
    if (w.gridCellSize->value() != 0) ui["gridCellSize"] = w.gridCellSize->value();
    if (w.categoryButtonSize->value() != 0) ui["categoryButtonSize"] = w.categoryButtonSize->value();
    if (w.recentLimit->value() != 0) ui["recentLimit"] = w.recentLimit->value();
    if (w.recentEnabled->currentText() != "General")
        ui["recentEnabled"] = (w.recentEnabled->currentText() == "On");
    if (!ui.isEmpty()) settings["ui"] = ui;

    // Behavior
    QJsonObject bhv;
    auto setBool = [&](QComboBox *cb, const QString &key) {
        if (cb->currentText() != "General")
            bhv[key] = (cb->currentText() == "On");
    };
    setBool(w.copyOnDblClick, "copyOnDoubleClick");
    setBool(w.highlightOnClick, "highlightOnClick");
    setBool(w.animateThumbnails, "animateThumbnails");
    setBool(w.animatePreview, "animatePreview");
    setBool(w.showFileTypeTag, "showFileTypeTag");
    setBool(w.showStickerName, "showStickerName");
    setBool(w.showStickerSize, "showStickerSize");
    setBool(w.showCategoryName, "showCategoryName");
    setBool(w.showCategoryCount, "showCategoryCount");
    if (!bhv.isEmpty()) settings["behavior"] = bhv;

    lib.settings = settings;
    return lib;
}

void LibrarySettingsPage::validateAllHotkeys() {
    for (int i = 0; i < m_libs.size(); ++i) {
        bool conflict = false;
        if (m_libs[i].hotkeyBtn && m_libs[i].enabledCheck && m_libs[i].enabledCheck->isChecked())
            conflict = hotkeyInUse(i, m_libs[i].hotkeyBtn->hotkey());
        if (m_libs[i].hotkeyBtn)
            m_libs[i].hotkeyBtn->setConflict(conflict);
    }
}

bool LibrarySettingsPage::hotkeyInUse(int skipIdx, const QString &hk) const {
    if (hk.isEmpty()) return false;
    for (int j = 0; j < m_libs.size(); ++j) {
        if (j == skipIdx || !m_libs[j].hotkeyBtn || !m_libs[j].enabledCheck ||
            !m_libs[j].enabledCheck->isChecked())
            continue;
        if (ShortcutCompare::compareShortcutKeys(hk, m_libs[j].hotkeyBtn->hotkey()))
            return true;
    }
    return false;
}

bool LibrarySettingsPage::collectToConfig() {
    QVector<LibraryConfig> libs;
    libs.reserve(m_libs.size());
    for (int i = 0; i < m_libs.size(); ++i)
        libs.append(collectOne(i));

    for (int i = 0; i < libs.size(); ++i) {
        if (libs[i].hotkey.isEmpty() || !libs[i].enabled) continue;
        for (int j = i + 1; j < libs.size(); ++j) {
            if (libs[j].hotkey.isEmpty() || !libs[j].enabled) continue;
            if (ShortcutCompare::compareShortcutKeys(libs[i].hotkey, libs[j].hotkey)) {
                QMessageBox::warning(this, "Duplicate Hotkey",
                    QString("Libraries \"%1\" and \"%2\" share the same hotkey: %3\n"
                            "Change one of them before saving.")
                        .arg(QFileInfo(libs[i].path).fileName(),
                             QFileInfo(libs[j].path).fileName(),
                             libs[i].hotkey));
                return false;
            }
        }
    }

    m_config->setLibraries(libs);
    return true;
}
