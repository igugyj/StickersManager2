#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QObject>
#include <QPixmap>
#include <QCache>
#include <QDir>
#include <QMutex>

class AsyncThumbnailLoader;

class ThumbnailCache : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailCache(int maxSize = 200, QObject *parent = nullptr);
    ~ThumbnailCache();

    QPixmap get(const QString &key);

    void loadThumbnailAsync(const QString &imagePath, const QSize &targetSize);

    void cancelAllLoads();
    void clear();

    int maxSize() const { return m_cache.maxCost(); }
    void setMaxSize(int size) { m_cache.setMaxCost(size); }

signals:
    void thumbnailReady(const QString &imagePath, const QPixmap &thumbnail);

private slots:
    void onAsyncThumbnailLoaded(const QString &filePath, const QPixmap &pixmap);

private:
    QCache<QString, QPixmap> m_cache;
    QMutex m_cacheMutex;
    AsyncThumbnailLoader *m_asyncLoader;
};

#endif // THUMBNAILCACHE_H
