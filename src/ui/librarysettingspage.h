#ifndef LIBRARYSETTINGSPAGE_H
#define LIBRARYSETTINGSPAGE_H

#include <QWidget>
#include <QVector>

class ConfigManager;
struct LibraryConfig;
class QVBoxLayout;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QTableWidget;
template <typename T> class QFutureWatcher;
class HotkeyCaptureButton;

struct LibraryEditWidgets {
    int libId = -1;
    QLineEdit *pathEdit;
    HotkeyCaptureButton *hotkeyBtn;
    QCheckBox *enabledCheck;
    // overrides
    QCheckBox *useCustomGeometry;
    QSpinBox *winPosX, *winPosY, *winW, *winH;
    QComboBox *alwaysOnTop;
    QSpinBox *gridCellSize;
    QSpinBox *categoryButtonSize;
    QSpinBox *recentLimit;
    QComboBox *recentEnabled;
    QComboBox *animateThumbnails;
    QComboBox *animatePreview;
    QComboBox *showFileTypeTag;
    QComboBox *showStickerName;
    QComboBox *showStickerSize;
    QComboBox *showCategoryName;
    QComboBox *showCategoryCount;
    QComboBox *copyOnDblClick;
    QComboBox *highlightOnClick;
    QWidget *overrideWidget;
    // structure stats
    QPushButton *statsToggle = nullptr;
    QWidget *statsWidget = nullptr;
    QLabel *statsLabel = nullptr;
    QTableWidget *statsTable = nullptr;
    QFutureWatcher<QStringList> *statsWatcher = nullptr;
};

class LibrarySettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit LibrarySettingsPage(ConfigManager *config, QWidget *parent = nullptr);
    bool collectToConfig();
private slots:
    void addLibrary();
    void removeLibrary(int index);
    void browseLibrary(int index);
private:
    ConfigManager *m_config;
    QVBoxLayout *m_listLayout;
    QVector<LibraryEditWidgets> m_libs;

    void buildFromConfig();
    void rebuildList();
    void validateAllHotkeys();
    bool hotkeyInUse(int skipIdx, const QString &hk) const;
    void swapLibraries(int a, int b);
    void syncCardIndices();
    LibraryConfig collectOne(int index) const;
};

#endif // LIBRARYSETTINGSPAGE_H
