#ifndef ASYNCTHUMBNAILLOADER_H
#define ASYNCTHUMBNAILLOADER_H

#include <QObject>
#include <QRunnable>
#include <QThreadPool>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QPixmap>
#include <QSize>
#include <QPair>
#include <QVector>

class AsyncThumbnailLoader : public QObject {
    Q_OBJECT

public:
    explicit AsyncThumbnailLoader(QObject *parent = nullptr);
    ~AsyncThumbnailLoader();

    void loadThumbnail(const QString &filePath, const QSize &targetSize);
    void cancelAll();

signals:
    void thumbnailLoaded(const QString &filePath, const QPixmap &pixmap);

private slots:
    void handleThumbnailLoaded(const QString &filePath, const QPixmap &pixmap);

private:
    QThreadPool *m_threadPool;
    QMutex m_mutex;
    QSet<QString> m_pendingLoads;

    class LoadTask : public QRunnable {
    public:
        LoadTask(const QString &filePath, const QSize &targetSize, QObject *receiver);
        void run() override;

    private:
        QString m_filePath;
        QSize m_targetSize;
        QObject *m_receiver;
    };

    static QImage loadThumbnailInternal(const QString &filePath, const QSize &targetSize);
};

#endif // ASYNCTHUMBNAILLOADER_H
