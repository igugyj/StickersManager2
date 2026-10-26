#ifndef STICKERCELL_H
#define STICKERCELL_H

#include <QFrame>
#include <QString>
#include <QPixmap>
#include <QMovie>
#include <QFutureWatcher>

class QLabel;

class StickerCell : public QFrame {
    Q_OBJECT

public:
    explicit StickerCell(const QString &filePath, int cellSize, QWidget *parent = nullptr);

    ~StickerCell();

    QString getFilePath() const { return m_filePath; }

    // 内存量化埋点：当前缩略图像素字节 + GIF 动画是否驻留（hide/show 日志用）
    qint64 pixmapBytes() const {
        return m_currentPixmap.isNull()
                   ? 0
                   : qint64(m_currentPixmap.width()) * m_currentPixmap.height() * m_currentPixmap.depth() / 8;
    }
    bool isAnimationLoaded() const { return m_movie != nullptr; }

    void setThumbnail(const QPixmap &pixmap);

    void clearHighlight();
    void setHighlighted(bool on);

    // 设置占位图（只在构造函数中调用一次）
    void setPlaceholder();

    void setAnimateEnabled(bool enabled);
    void setInViewport(bool visible);
    void setShowTag(bool show);
    void setShowFileName(bool show);
    void setShowFileSize(bool show);
    void setHighlightEnabled(bool enabled) { m_highlightEnabled = enabled; }
    void setCopyOnDoubleClick(bool enabled) { m_copyOnDblClick = enabled; }

signals:
    void clicked(const QString &filePath);

    void doubleClicked(const QString &filePath);

    void rightClicked(const QString &filePath);

    // 中键：打开所在文件夹并选中该文件（动作由 MainWindow::onStickerMiddleClicked 执行）
    void middleClicked(const QString &filePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void resizeEvent(QResizeEvent *event) override; // 添加重绘事件

private:
    void loadAnimation();
    void unloadAnimation();
    QString m_filePath;
    QLabel *m_imageLabel;
    QLabel *m_tagLabel;
    QLabel *m_fileNameLabel;
    QLabel *m_sizeLabel;
    bool m_isHighlighted;
    int m_cellSize;
    QPixmap m_currentPixmap; // 保存当前图片
    bool m_hasRealThumbnail; // 标记是否有真实缩略图
    bool m_animateEnabled = false;
    bool m_inViewport = true;
    bool m_showTag = true;
    bool m_showFileName = true;
    bool m_showFileSize = true;
    bool m_highlightEnabled = true;
    bool m_copyOnDblClick = true;
    QMovie *m_movie = nullptr;
    QFutureWatcher<QByteArray> *m_animWatcher = nullptr;
};

// 内存量化埋点：QMovie 创建/销毁全局计数（[mem] 行输出 mCreated/mDestroyed，
// 差值 > 0 即存在未释放的动画对象 —— 2026-10-25 隐藏后 private bytes 不回落排查）
int animationMoviesCreated();
int animationMoviesDestroyed();

#endif // STICKERCELL_H
