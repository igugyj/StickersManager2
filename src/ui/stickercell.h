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

#endif // STICKERCELL_H
