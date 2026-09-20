#include "mainwindow.h"
#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QKeySequence>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include "tray.h"
#include "custommenu.h"

static const QString kRecentCategoryName = "Recent";

MainWindow::MainWindow(ConfigManager *config, const LibraryConfig &libConfig, QWidget *parent)
    : QMainWindow(parent), m_config(config), m_library(nullptr), m_thumbnailCache(nullptr),
      m_searchTimer(new QTimer(this)), m_libConfig(libConfig), m_highlightManager(m_cellMap) {
    refreshEffectiveSettings();
    m_thumbnailCache = new ThumbnailCache(m_eff.thumbnailCacheSize, this);
    m_library = new StickerLibrary(this);

    connect(m_thumbnailCache, &ThumbnailCache::thumbnailReady,
            this, &MainWindow::onThumbnailLoaded);

    initUI();
    m_recents.load(m_libConfig.path);
    m_recents.setLimit(m_eff.recentLimit);
    loadLibrary();
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::delayedSearch);
}

MainWindow::~MainWindow() {
    delete m_thumbnailCache;
    delete m_library;
}

LibraryConfig MainWindow::getLibraryConfig() const {
    return m_libConfig;
}

void MainWindow::updateLibraryConfig(const LibraryConfig &lib) {
    m_libConfig = lib;
    refreshEffectiveSettings();
}

void MainWindow::refreshEffectiveSettings() {
    m_eff.windowSize = m_config->getEffectiveWindowSize(m_libConfig);
    m_eff.windowPos = m_config->getEffectiveWindowPosition(m_libConfig);
    m_eff.alwaysOnTop = m_config->getEffectiveAlwaysOnTop(m_libConfig);
    m_eff.categoryButtonSize = m_config->getEffectiveCategoryButtonSize(m_libConfig);
    m_eff.gridCellSize = m_config->getEffectiveGridCellSize(m_libConfig);
    m_eff.gridColumns = m_config->getEffectiveGridColumns(m_libConfig);
    m_eff.recentLimit = m_config->getEffectiveRecentLimit(m_libConfig);
    m_eff.recentEnabled = m_config->getEffectiveRecentEnabled(m_libConfig);
    m_eff.thumbnailCacheSize = m_config->getEffectiveThumbnailCacheSize(m_libConfig);
    m_eff.animateThumbnails = m_config->getEffectiveAnimateThumbnails(m_libConfig);
    m_eff.animatePreview = m_config->getEffectiveAnimatePreview(m_libConfig);
    m_eff.showFileTypeTag = m_config->getEffectiveShowFileTypeTag(m_libConfig);
    m_eff.showStickerName = m_config->getEffectiveShowStickerName(m_libConfig);
    m_eff.showStickerSize = m_config->getEffectiveShowStickerSize(m_libConfig);
    m_eff.showCategoryName = m_config->getEffectiveShowCategoryName(m_libConfig);
    m_eff.showCategoryCount = m_config->getEffectiveShowCategoryCount(m_libConfig);
    m_eff.highlightOnClick = m_config->getEffectiveHighlightOnClick(m_libConfig);
    m_eff.copyOnDoubleClick = m_config->getEffectiveCopyOnDoubleClick(m_libConfig);
}

void MainWindow::applySettings() {
    refreshEffectiveSettings();
    QString savedSearch = m_searchInput->text();
    QString savedCatSearch = m_categorySearchInput->text();
    QString savedCat = m_currentCategory;

    m_thumbnailCache->setMaxSize(m_eff.thumbnailCacheSize);
    m_recents.setLimit(m_eff.recentLimit);

    populateCategories();
    recalculateGridColumns();

    if (!savedSearch.isEmpty()) {
        m_searchTimer->stop();
        m_searchInput->setText(savedSearch);
        m_searchTimer->stop();
        performSearch();
    } else if (!savedCat.isEmpty()) {
        showCategory(savedCat);
    }
    m_categorySearchInput->setText(savedCatSearch);

    QTimer::singleShot(0, this, &MainWindow::updateCellVisibility);
}

void MainWindow::initUI() {
    QSize windowSize = m_eff.windowSize;
    QPoint windowPos = m_eff.windowPos;
    setGeometry(windowPos.x(), windowPos.y(), windowSize.width(), windowSize.height());
    Qt::WindowFlags flags = Qt::Window;
    if (m_eff.alwaysOnTop)
        flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    QWidget *leftPanel = createCategoryPanel();
    mainLayout->addWidget(leftPanel, 0);
    QWidget *rightPanel = createStickerPanel();
    mainLayout->addWidget(rightPanel, 1);

    QShortcut *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, &MainWindow::hide);
}

QWidget *MainWindow::createCategoryPanel() {
    QWidget *panel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    int buttonSize = m_eff.categoryButtonSize;
    m_categoryPanel = panel;
    panel->setMaximumWidth(buttonSize + 20);
    panel->setMinimumWidth(buttonSize + 20);

    m_categorySearchInput = new QLineEdit();
    m_categorySearchInput->setPlaceholderText("Search...");
    m_categorySearchInput->setClearButtonEnabled(true);
    connect(m_categorySearchInput, &QLineEdit::textChanged, this, &MainWindow::onCategorySearchTextChanged);
    layout->addWidget(m_categorySearchInput);

    m_categoryScroll = new QScrollArea();
    m_categoryScroll->setWidgetResizable(true);
    m_categoryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_categoryScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_categoryContainer = new QWidget();
    m_categoryLayout = new QVBoxLayout(m_categoryContainer);
    m_categoryLayout->setAlignment(Qt::AlignTop);
    m_categoryLayout->setSpacing(8);
    m_categoryLayout->setContentsMargins(5, 5, 5, 5);
    m_categoryScroll->setWidget(m_categoryContainer);
    layout->addWidget(m_categoryScroll);
    return panel;
}

QWidget *MainWindow::createStickerPanel() {
    QWidget *panel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search...");
    m_searchInput->setClearButtonEnabled(true);
    connect(m_searchInput, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    layout->addWidget(m_searchInput);

    m_stickerScroll = new QScrollArea();
    m_stickerScroll->setWidgetResizable(false);
    m_stickerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_stickerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_stickerContainer = new QWidget();
    m_stickerScroll->setWidget(m_stickerContainer);
    connect(m_stickerScroll->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::updateCellVisibility);
    layout->addWidget(m_stickerScroll);

    return panel;
}

void MainWindow::loadLibrary() {
    QString libraryPath = m_libConfig.path;
    if (libraryPath.isEmpty() || !QDir(libraryPath).exists()) {
        libraryPath = QFileDialog::getExistingDirectory(this, "Select Stickers Library", QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
        if (libraryPath.isEmpty()) return;
        m_libConfig.path = libraryPath;
        m_config->addLibrary(m_libConfig);
        m_config->saveConfig();
        m_recents.load(m_libConfig.path);
        m_recents.setLimit(m_eff.recentLimit);
    }
    if (m_library->setLibraryPath(libraryPath)) {
        populateCategories();
    }
}

void MainWindow::populateCategories()
{
    QLayoutItem *child;
    while ((child = m_categoryLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    m_categoryButtons.clear();
    m_pendingCategoryButtons.clear();

    QMap<QString, QVector<QString> > categories = m_library->getCategories();
    int buttonSize = m_eff.categoryButtonSize;
    if (m_categoryPanel) {
        m_categoryPanel->setMaximumWidth(buttonSize + 20);
        m_categoryPanel->setMinimumWidth(buttonSize + 20);
    }

    auto addCategoryButton = [&](const QString &categoryName, const QString &firstSticker, int count, bool showClock = false) {
        CategoryButton *button = new CategoryButton(categoryName, buttonSize);
        button->setStickerCount(count);
        button->setShowName(m_eff.showCategoryName);
        button->setShowCount(m_eff.showCategoryCount);
        button->setShowClock(showClock);

        connect(button, &CategoryButton::clicked, this, &MainWindow::onCategoryClicked);
        connect(button, &CategoryButton::customContextMenuRequested,
                this, &MainWindow::onCategoryContextMenuRequested);
        m_categoryLayout->addWidget(button);
        m_categoryButtons[button] = categoryName;

        if (!firstSticker.isEmpty()) {
            QPixmap cached = m_thumbnailCache->get(firstSticker);
            if (!cached.isNull()) {
                button->setThumbnail(cached);
            } else {
                m_pendingCategoryButtons[firstSticker] = button;
                m_thumbnailCache->loadThumbnailAsync(firstSticker, QSize(buttonSize, buttonSize));
            }
        }
    };

    // Recent pseudo-category first — hidden entirely when there are no usage records
    // or when the Recent feature is disabled (default or per-library override)
    QVector<QString> recentPaths = m_recents.paths();
    if (m_eff.recentEnabled && !recentPaths.isEmpty()) {
        addCategoryButton(kRecentCategoryName, QString(), recentPaths.size(), true);
    }

    for (const QString &categoryName: categories.keys()) {
        QVector<QString> stickers = categories[categoryName];
        if (stickers.isEmpty()) continue;
        addCategoryButton(categoryName, stickers.first(), stickers.size());
    }
    m_categoryLayout->addStretch();

    if (m_eff.recentEnabled && !recentPaths.isEmpty())
        showCategory(kRecentCategoryName);
    else if (!categories.isEmpty())
        showCategory(categories.keys().first());
}

void MainWindow::showCategory(const QString &categoryName) {
    if (categoryName == kRecentCategoryName && !m_eff.recentEnabled)
        return;
    m_currentCategory = categoryName;
    m_searchInput->clear();
    m_categorySearchInput->clear();

    if (categoryName == kRecentCategoryName)
        displayStickers(m_recents.paths());
    else
        displayStickers(m_library->getCategories().value(categoryName));
    QTimer::singleShot(0, this, &MainWindow::updateCellVisibility);
}

void MainWindow::displayStickers(const QVector<QString> &stickers)
{
    clearStickerCells();
    m_currentStickers = stickers;

    relayoutGrid();
    QTimer::singleShot(0, this, &MainWindow::updateCellVisibility);
}

void MainWindow::clearStickerCells() {
    for (auto it = m_cellMap.begin(); it != m_cellMap.end(); ++it)
        delete it.value();
    m_cellMap.clear();
    m_currentCells.clear();
}

void MainWindow::relayoutGrid() {
    int cellSize = m_eff.gridCellSize;
    int step = cellSize + m_gridSpacing;
    int scrollWidth = m_stickerScroll->viewport()->width();

    int cols = scrollWidth / step;
    if (cols < 1)
        cols = 1;
    cols = qMax(m_eff.gridColumns, cols);
    m_gridColumns = cols;

    int count = m_currentStickers.size();
    int rows = (count + cols - 1) / cols;
    int contentW = qMax(scrollWidth, 10 + cols * cellSize + (cols - 1) * m_gridSpacing);
    int contentH = 10 + rows * cellSize + (rows > 0 ? (rows - 1) * m_gridSpacing : 0);
    m_stickerContainer->setFixedSize(contentW, contentH);

    // reposition existing cells for the new column layout
    for (int i = 0; i < count; ++i) {
        auto it = m_cellMap.constFind(m_currentStickers[i]);
        if (it == m_cellMap.constEnd())
            continue;
        StickerCell *cell = it.value();
        int row = i / cols, col = i % cols;
        cell->move(5 + col * step, 5 + row * step);
    }
}

void MainWindow::updateVisibleCells() {
    if (!m_stickerScroll || m_currentStickers.isEmpty())
        return;

    int cellSize = m_eff.gridCellSize;
    int step = cellSize + m_gridSpacing;
    int cols = m_gridColumns;

    int scrollVal = m_stickerScroll->verticalScrollBar()->value();
    int viewportH = m_stickerScroll->viewport()->height();
    const int margin = 1;

    int firstRow = qMax(0, scrollVal / step - margin);
    int lastRow = (scrollVal + viewportH) / step + margin;
    int firstIdx = qMax(0, firstRow * cols);
    int lastIdx = qMin(m_currentStickers.size() - 1, lastRow * cols + cols - 1);

    QSize targetSize(cellSize - 10, cellSize - 10);

    // create cells for the visible range
    for (int i = firstIdx; i <= lastIdx; ++i) {
        const QString &path = m_currentStickers[i];
        if (m_cellMap.contains(path))
            continue;

        StickerCell *cell = new StickerCell(path, cellSize, m_stickerContainer);
        cell->setAnimateEnabled(m_eff.animateThumbnails);
        cell->setShowTag(m_eff.showFileTypeTag);
        cell->setShowFileName(m_eff.showStickerName);
        cell->setShowFileSize(m_eff.showStickerSize);
        cell->setHighlightEnabled(m_eff.highlightOnClick);
        cell->setCopyOnDoubleClick(m_eff.copyOnDoubleClick);
        connect(cell, &StickerCell::clicked, this, &MainWindow::onStickerClicked);
        connect(cell, &StickerCell::doubleClicked, this, &MainWindow::onStickerDoubleClicked);
        connect(cell, &StickerCell::rightClicked, this, &MainWindow::onStickerRightClicked);
        cell->setToolTip(QFileInfo(path).fileName());

        int row = i / cols, col = i % cols;
        cell->move(5 + col * step, 5 + row * step);
        cell->show();

        m_currentCells.append(cell);
        m_cellMap[path] = cell;

        QPixmap cached = m_thumbnailCache->get(path);
        if (!cached.isNull() && cached.size() == targetSize) {
            cell->setThumbnail(cached);
        } else {
            m_thumbnailCache->loadThumbnailAsync(path, targetSize);
        }
    }

    // remove cells that scrolled far outside the visible range
    for (auto it = m_cellMap.begin(); it != m_cellMap.end();) {
        StickerCell *cell = it.value();
        int row = (cell->y() - 5) / step;
        if (row < firstRow || row > lastRow) {
            it = m_cellMap.erase(it);
            m_currentCells.removeAll(cell);
            delete cell;
        } else {
            ++it;
        }
    }
    m_highlightManager.refresh();
}

void MainWindow::onThumbnailLoaded(const QString &filePath, const QPixmap &pixmap)
{
    QMetaObject::invokeMethod(this, [this, filePath, pixmap]() {
        handleThumbnailLoaded(filePath, pixmap);
    }, Qt::QueuedConnection);
}

void MainWindow::handleThumbnailLoaded(const QString &filePath, const QPixmap &pixmap) {
    if (pixmap.isNull()) return;

    if (m_cellMap.contains(filePath)) {
        StickerCell *cell = m_cellMap[filePath];
        if (cell) {
            cell->setThumbnail(pixmap);
        }
    }

    if (m_pendingCategoryButtons.contains(filePath)) {
        CategoryButton *btn = m_pendingCategoryButtons[filePath];
        if (btn) {
            btn->setThumbnail(pixmap);
        }
        m_pendingCategoryButtons.remove(filePath);
    }
}

void MainWindow::applyWindowSettings() {
    setWindowFlag(Qt::WindowStaysOnTopHint, m_eff.alwaysOnTop);
    setGeometry(
        m_eff.windowPos.x(),
        m_eff.windowPos.y(),
        m_eff.windowSize.width(),
        m_eff.windowSize.height()
    );
}

void MainWindow::showWindow() {
    applyWindowSettings();
    if (isHidden()) {
        update();
        show();
        recalculateGridColumns();
    }
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}

void MainWindow::reloadLibrary() {
    qDebug() << "Reloading library...";
    if (m_library) {
        m_recents.load(m_libConfig.path);
        m_recents.setLimit(m_eff.recentLimit);
        if (m_library->scanLibrary()) {
            populateCategories();
            recalculateGridColumns();
        }
    }
}

void MainWindow::performSearch() {
    QString keyword = m_searchInput->text().trimmed();
    if (keyword.isEmpty()) {
        if (!m_currentCategory.isEmpty()) showCategory(m_currentCategory);
        return;
    }
    displayStickers(m_library->searchStickers(keyword));
    QTimer::singleShot(0, this, &MainWindow::updateCellVisibility);
}

void MainWindow::onSearchTextChanged(const QString &text) {
    if (text.isEmpty()) {
        if (!m_currentCategory.isEmpty()) showCategory(m_currentCategory);
    } else {
        m_searchTimer->stop();
        m_searchTimer->start(m_config->getSearchDelayMs());
    }
}

void MainWindow::onCategorySearchTextChanged(const QString &text) {
    for (auto it = m_categoryButtons.begin(); it != m_categoryButtons.end(); ++it) {
        CategoryButton *button = it.key();
        QString categoryName = it.value();
        if (text.isEmpty() || categoryName.contains(text, Qt::CaseInsensitive)) {
            button->show();
        } else {
            button->hide();
        }
    }
}

void MainWindow::onCategoryClicked() {
    CategoryButton *button = qobject_cast<CategoryButton *>(sender());
    if (button && m_categoryButtons.contains(button)) {
        showCategory(m_categoryButtons[button]);
    }
}

void MainWindow::onCategoryContextMenuRequested(const QPoint &pos) {
    auto *button = qobject_cast<CategoryButton *>(sender());
    if (!button || !m_categoryButtons.contains(button))
        return;
    if (m_categoryButtons[button] != kRecentCategoryName)
        return;

    CustomMenu menu(this);
    QAction *clearAct = menu.addAction("Clear Recent Records");
    if (menu.exec(button->mapToGlobal(pos)) == clearAct) {
        m_recents.clear();
        populateCategories();
    }
}

void MainWindow::onStickerClicked(const QString &filePath) {
    m_highlightManager.setHighlightedPath(m_eff.highlightOnClick ? filePath : QString());
}

void MainWindow::onStickerRightClicked(const QString &filePath) {
    m_highlightManager.setHighlightedPath(m_eff.highlightOnClick ? filePath : QString());
    if (m_previewDlg) {
        m_previewDlg->accept();
        m_previewDlg = nullptr;
    } else {
        QString category = QFileInfo(filePath).dir().dirName();
        QVector<QString> siblings = m_library->getCategories().value(category);
        m_previewDlg = new ImagePreviewDialog(filePath, m_eff.animatePreview, siblings, this);
        connect(m_previewDlg, &ImagePreviewDialog::finished,
                this, &MainWindow::onPreviewClosed);
        connect(m_previewDlg, &ImagePreviewDialog::currentFileChanged,
                this, &MainWindow::onPreviewFileChanged);
        m_previewDlg->show();
    }
}

void MainWindow::onPreviewFileChanged(const QString &filePath) {
    qDebug() << "[PREVIEW] onPreviewFileChanged:" << filePath << "highlightOnClick:" << m_eff.highlightOnClick;
    int idx = m_currentStickers.indexOf(filePath);
    if (idx >= 0) {
        int cols = m_gridColumns;
        int cellSize = m_eff.gridCellSize;
        int step = cellSize + m_gridSpacing;
        int row = idx / cols;
        int targetY = 5 + row * step;
        qDebug() << "[PREVIEW] scroll to row" << row << "targetY" << targetY;
        m_stickerScroll->verticalScrollBar()->setValue(targetY);
        updateVisibleCells();
    }
    m_highlightManager.setHighlightedPath(m_eff.highlightOnClick ? filePath : QString());
}

void MainWindow::onPreviewClosed() {
    m_previewDlg = nullptr;
    m_highlightManager.clearHighlight();
}

void MainWindow::onStickerDoubleClicked(const QString &filePath) {
    if (m_eff.recentEnabled) {
        m_recents.recordUse(filePath);
        if (!refreshRecentButton()) {
            // 首次使用：Recent 按钮还不存在，重建类目面板（会创建 Recent 并切为当前视图）
            populateCategories();
        } else if (m_currentCategory == kRecentCategoryName) {
            displayStickers(m_recents.paths());
        }
    }
    hide();
}

bool MainWindow::refreshRecentButton() {
    CategoryButton *recentBtn = nullptr;
    for (auto it = m_categoryButtons.begin(); it != m_categoryButtons.end(); ++it) {
        if (it.value() == kRecentCategoryName) {
            recentBtn = it.key();
            break;
        }
    }
    if (!recentBtn)
        return false;

    recentBtn->setStickerCount(m_recents.paths().size());
    return true;
}

void MainWindow::delayedSearch() {
    performSearch();
}

void MainWindow::updateCellVisibility() {
    if (!m_stickerScroll) return;
    updateVisibleCells();
    QRect viewportRect = m_stickerScroll->viewport()->rect();
    for (StickerCell *cell : m_currentCells) {
        QRect cellRect(cell->mapTo(m_stickerScroll->viewport(), QPoint(0, 0)), cell->size());
        cell->setInViewport(viewportRect.intersects(cellRect));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    recalculateGridColumns();
    updateCellVisibility();
}

void MainWindow::recalculateGridColumns() {
    if (!m_stickerScroll || !m_stickerContainer)
        return;

    int cellSize = m_eff.gridCellSize;
    int step = cellSize + m_gridSpacing;
    int scrollWidth = m_stickerScroll->viewport()->width();

    int calculatedColumns = scrollWidth / step;
    if (calculatedColumns < 1)
        calculatedColumns = 1;
    int newColumns = qMax(m_eff.gridColumns, calculatedColumns);

    if (newColumns != m_gridColumns) {
        m_gridColumns = newColumns;
        relayoutGrid();
        updateCellVisibility();
    } else {
        relayoutGrid();
    }
}