#include "stickerlibrary.h"
#include "imageloader.h"
#include "fsutil.hpp"
#include <QDebug>
#include <QFileInfo>

StickerLibrary::StickerLibrary(QObject *parent)
    : QObject(parent) {
}

bool StickerLibrary::setLibraryPath(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        return false;
    }

    m_libraryPath = path;
    return scanLibrary();
}

bool StickerLibrary::scanLibrary() {
    if (m_libraryPath.isEmpty()) {
        return false;
    }

    m_categories.clear();
    m_allStickers.clear();
    m_searchIndex.clear();

    QDir libraryDir(m_libraryPath);
    if (!libraryDir.exists()) {
        return false;
    }

    // Collect files in walk order, grouped by category (order preserved).
    QVector<QString> categoryOrder;
    QMap<QString, QVector<QString>> stickerLists;
    forEachStickerFile(m_libraryPath, [&](const QString &categoryName, const QFileInfo &fi) {
        if (!isValidImage(fi.absoluteFilePath()))
            return;
        if (!stickerLists.contains(categoryName))
            categoryOrder.append(categoryName);
        stickerLists[categoryName].append(fi.absoluteFilePath());
    });

    int totalStickers = 0;
    for (const QString &categoryName : categoryOrder) {
        QVector<QString> &imageFiles = stickerLists[categoryName];
        std::sort(imageFiles.begin(), imageFiles.end());
        m_categories[categoryName] = imageFiles;
        for (const QString &path : imageFiles) {
            m_searchIndex.insert(path, QFileInfo(path).fileName().toLower());
            m_allStickers.append(path);
        }
        totalStickers += imageFiles.size();
        qDebug() << "Category" << categoryName << "loaded" << imageFiles.size() << "stickers";
    }

    qDebug() << "Sticker library scan complete, total" << m_categories.size() << "categories,"
             << totalStickers << "stickers";
    return true;
}

QVector<QString> StickerLibrary::searchStickers(const QString &keyword) {
    if (keyword.isEmpty()) {
        return QVector<QString>();
    }
    QString lowerKeyword = keyword.toLower();
    QVector<QString> results;

    for (const QString &stickerPath: m_allStickers) {
        if (m_searchIndex.value(stickerPath).contains(lowerKeyword)) {
            results.append(stickerPath);
        }
    }

    return results;
}

bool StickerLibrary::isValidImage(const QString &filePath) {
    return ImageLoader::isFormatSupported(filePath);
}
