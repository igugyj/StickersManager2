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

    // 内存量化埋点：条目数 + 估算字节（QCache 不可遍历，用 key→bytes 台账 +
    // 按当前 size 剪除被 LRU 淘汰的键来校正，仅统计诊断用）
    struct Stats {
        int count = 0;
        qint64 bytes = 0;
    };
    Stats stats();

signals:
    void thumbnailReady(const QString &imagePath, const QPixmap &thumbnail);

private slots:
    void onAsyncThumbnailLoaded(const QString &filePath, const QPixmap &pixmap);

private:
    QCache<QString, QPixmap> m_cache;
    QMutex m_cacheMutex;
    QMap<QString, qint64> m_bytesByKey; // 台账：key → 缓存时的像素字节
    AsyncThumbnailLoader *m_asyncLoader;
};

#endif // THUMBNAILCACHE_H
