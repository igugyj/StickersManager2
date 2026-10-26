#ifndef LIBRARYSETTINGSPAGE_H
#define LIBRARYSETTINGSPAGE_H

#include <QWidget>
#include <QVector>
#include <QString>

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

// 卡片结构检查结果（TODO ②：路径无效/无类目/无图片时卡片挂黄字提示）
struct StructureCheck {
    QString path;     // 检查时的路径（finished 里比对，防过期结果）
    QString message;  // 空 = 结构正常（不显示提示）
};

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
    // structure hint (yellow label under the path row, TODO ②)
    QLabel *structHint = nullptr;
    QFutureWatcher<StructureCheck> *structWatcher = nullptr;
};

class LibrarySettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit LibrarySettingsPage(ConfigManager *config, QWidget *parent = nullptr);
    bool collectToConfig();
    // 公开供 SettingsDialog 切页时调用（与设置热键的双向冲突刷新）
    void validateAllHotkeys();
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
    bool hotkeyInUse(int skipIdx, const QString &hk) const;
    void swapLibraries(int a, int b);
    void syncCardIndices();
    void startStructureCheck(int index);
    LibraryConfig collectOne(int index) const;
};

#endif // LIBRARYSETTINGSPAGE_H
