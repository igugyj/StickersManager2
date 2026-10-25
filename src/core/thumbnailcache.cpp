#include "thumbnailcache.h"
#include "asyncthumbnailloader.h"
#include <QDebug>
#include <QPainter>

ThumbnailCache::ThumbnailCache(int maxSize, QObject *parent)
    : QObject(parent)
      , m_cache(maxSize)
      , m_asyncLoader(new AsyncThumbnailLoader(this)) {
    connect(m_asyncLoader, &AsyncThumbnailLoader::thumbnailLoaded,
            this, &ThumbnailCache::onAsyncThumbnailLoaded);

    qDebug() << "Thumbnail cache initialized, max size:" << maxSize;
}

ThumbnailCache::~ThumbnailCache() {
    cancelAllLoads();
    clear();
}

QPixmap ThumbnailCache::get(const QString &key) {
    QMutexLocker locker(&m_cacheMutex);
    const QPixmap *p = m_cache.object(key);
    return p ? *p : QPixmap();
}

void ThumbnailCache::loadThumbnailAsync(const QString &imagePath, const QSize &targetSize) {
    QPixmap cached;
    {
        QMutexLocker locker(&m_cacheMutex);
        if (const QPixmap *p = m_cache.object(imagePath))
            cached = *p;
    }

    if (!cached.isNull()) {
        emit thumbnailReady(imagePath, cached);
        return;
    }

    m_asyncLoader->loadThumbnail(imagePath, targetSize);
}

void ThumbnailCache::cancelAllLoads() {
    m_asyncLoader->cancelAll();
}

void ThumbnailCache::clear() {
    QMutexLocker locker(&m_cacheMutex);
    m_cache.clear();
}

ThumbnailCache::Stats ThumbnailCache::stats() {
    QMutexLocker locker(&m_cacheMutex);
    // QCache 不可遍历：剪掉已被 LRU 淘汰的台账键，剩余即当前驻留
    for (auto it = m_bytesByKey.begin(); it != m_bytesByKey.end();) {
        if (!m_cache.contains(it.key()))
            it = m_bytesByKey.erase(it);
        else
            ++it;
    }
    Stats s;
    s.count = m_cache.size();
    for (const auto &v : std::as_const(m_bytesByKey))
        s.bytes += v;
    return s;
}

void ThumbnailCache::onAsyncThumbnailLoaded(const QString &filePath, const QPixmap &pixmap) {
    // Every load failure already surfaces as a "Load Failed" placeholder from the
    // async loader, so pixmap is never null here; the guard is defensive only.
    if (pixmap.isNull()) {
        emit thumbnailReady(filePath, pixmap);
        return;
    }

    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(filePath, new QPixmap(pixmap), 1);
        m_bytesByKey.insert(filePath,
                            qint64(pixmap.width()) * pixmap.height() * pixmap.depth() / 8);
    }

    emit thumbnailReady(filePath, pixmap);
}
