#ifndef CATEGORYBUTTON_H
#define CATEGORYBUTTON_H

#include <QPushButton>
#include <QString>
#include <QPixmap>

class QLabel;

class CategoryButton : public QPushButton {
    Q_OBJECT

public:
    explicit CategoryButton(const QString &categoryName,
                            int buttonSize,
                            QWidget *parent = nullptr);

    void setThumbnail(const QPixmap &pixmap);
    void setStickerCount(int count);
    void setShowName(bool show);
    void setShowCount(bool show);
    void setShowClock(bool show);

    QString getCategoryName() const { return m_categoryName; }

    // 内存量化埋点：预览图像素字节
    qint64 pixmapBytes() const {
        return m_currentPixmap.isNull()
                   ? 0
                   : qint64(m_currentPixmap.width()) * m_currentPixmap.height() * m_currentPixmap.depth() / 8;
    }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateIcon();
    void updateOverlayLabels();

    QString m_categoryName;
    int m_buttonSize;
    bool m_showName = true;
    bool m_showCount = true;
    bool m_showClock = false;
    QPixmap m_currentPixmap;
    QLabel *m_nameLabel;
    QLabel *m_countLabel;
};

#endif // CATEGORYBUTTON_H
