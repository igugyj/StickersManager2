#ifndef IMAGEPREVIEWDIALOG_H
#define IMAGEPREVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QMovie>
#include <QBuffer>
#include <QFutureWatcher>
#include <QVBoxLayout>
#include <QVector>
#include <QPushButton>

class ImagePreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImagePreviewDialog(const QString &filePath, bool animatePreview,
                                const QVector<QString> &siblings, QWidget *parent = nullptr);
    ~ImagePreviewDialog();

    QString getCurrentFilePath() const { return m_filePath; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void currentFileChanged(const QString &filePath);

private slots:
    void onPrev();
    void onNext();

private:
    void buildUi();
    void showAt(int index);
    void loadAndDisplayImage(const QString &filePath);
    void adjustDialogSize(const QSize &originalSize);
    void updateInfoPanel();
    void updateNavigationButtons(const QPoint &mousePos); // 新增辅助函数

    QLabel *m_imageLabel;
    QPixmap m_originalPixmap;
    bool m_animatePreview = false;
    QMovie *m_movie = nullptr;
    QFutureWatcher<QByteArray> *m_animWatcher = nullptr;
    QSize m_displaySize;

    QString m_filePath;
    QVector<QString> m_siblings;
    int m_index = 0;
    QSize m_frameSize;

    QWidget *m_imageArea = nullptr;
    QLabel *m_infoLabel = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QLabel *m_boundaryLabel = nullptr;
};

#endif // IMAGEPREVIEWDIALOG_H