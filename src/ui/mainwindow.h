#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QShortcut>
#include <QScrollArea>
#include <QLineEdit>
#include <QTimer>
#include <QMap>
#include <QPointer>

#include "configmanager.h"
#include "stickerlibrary.h"
#include "thumbnailcache.h"
#include "stickercell.h"
#include "categorybutton.h"
#include "imagepreviewdialog.h"
#include "recentusage.h"
#include "highlighmanager.h"

// Per-library effective settings, cached once per (config, lib) change so scroll
// hot paths don't re-resolve QJsonObject lookups for every cell.
struct EffectiveSettings {
    QSize windowSize;
    QPoint windowPos;
    bool alwaysOnTop = false;
    int categoryButtonSize = 90;
    int gridCellSize = 120;
    int gridColumns = 3;
    int recentLimit = 100;
    bool recentEnabled = true;
    int thumbnailCacheSize = 200;
    bool animateThumbnails = false;
    bool animatePreview = true;
    bool showFileTypeTag = true;
    bool showStickerName = true;
    bool showStickerSize = true;
    bool showCategoryName = true;
    bool showCategoryCount = true;
    bool highlightOnClick = true;
    bool copyOnDoubleClick = true;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ConfigManager *config, const LibraryConfig &libConfig, QWidget *parent = nullptr);
    ~MainWindow();

    void showWindow();
    void applyWindowSettings();

    LibraryConfig getLibraryConfig() const;
    void updateLibraryConfig(const LibraryConfig &lib);
    void applySettings();

public slots:
    void reloadLibrary();
    void performSearch();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onCategorySearchTextChanged(const QString &text);
    void onCategoryClicked();
    void onCategoryContextMenuRequested(const QPoint &pos);
    void onStickerClicked(const QString &filePath);
    void onStickerDoubleClicked(const QString &filePath);
    void onStickerRightClicked(const QString &filePath);
    void delayedSearch();
    void onThumbnailLoaded(const QString &filePath, const QPixmap &pixmap);
    void handleThumbnailLoaded(const QString &filePath, const QPixmap &pixmap);
    void onPreviewFileChanged(const QString &filePath);
    void onPreviewClosed();

private:
    void initUI();
    void refreshEffectiveSettings();
    QWidget *createCategoryPanel();
    QWidget *createStickerPanel();
    void loadLibrary();
    void populateCategories();
    void showCategory(const QString &categoryName);
    void displayStickers(const QVector<QString> &stickers);
    void clearStickerCells();
    void relayoutGrid();
    void updateVisibleCells();
    void recalculateGridColumns();
    void updateCellVisibility();
    bool refreshRecentButton();

    ConfigManager *m_config;
    StickerLibrary *m_library;
    ThumbnailCache *m_thumbnailCache;
    RecentUsageStore m_recents;
    LibraryConfig m_libConfig;
    EffectiveSettings m_eff;

    QWidget *m_categoryPanel = nullptr;
    QScrollArea *m_categoryScroll;
    QWidget *m_categoryContainer;
    QVBoxLayout *m_categoryLayout;
    QScrollArea *m_stickerScroll;
    QWidget *m_stickerContainer;
    QLineEdit *m_searchInput;
    QLineEdit *m_categorySearchInput;

    QTimer *m_searchTimer;
    QString m_currentCategory;
    QVector<QString> m_currentStickers;
    QVector<StickerCell *> m_currentCells;
    int m_gridColumns = 1;
    int m_gridSpacing = 8;

    QMap<CategoryButton *, QString> m_categoryButtons;
    QMap<QString, CategoryButton *> m_pendingCategoryButtons;
    QMap<QString, StickerCell *> m_cellMap;
    QPointer<ImagePreviewDialog> m_previewDlg;
    HighlightManager m_highlightManager;
};

#endif // MAINWINDOW_H
