#pragma once
#include <QDir>
#include <QString>
#include <QDirIterator>
#include <QFileInfo>
#include <QStringList>
#include <QImageReader>

// 字节数 → 可读字符串 (B/KB/MB/GB)
static QString formatBytes(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

// 文件格式字符串（QImageReader::format 优先，兜底扩展名）
static QString formatFileType(const QString &filePath) {
    QImageReader reader(filePath);
    QByteArray fmt = reader.format();
    return fmt.isEmpty() ? QFileInfo(filePath).suffix().toUpper() : QString(fmt).toUpper();
}

// 目录递归大小（字节）
static qint64 dirSize(const QString &path) {
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    qint64 total = 0;
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

// 预览/缓存辅助文件（.preview*）—— 扫描时跳过
static bool isPreviewFile(const QString &fileName) {
    return fileName.startsWith(".preview") || fileName.contains(".preview.");
}

// 遍历库内 sticker 文件（非递归，跳过 .preview 文件），
// 按目录顺序回调 (categoryName, QFileInfo)。
template <typename Fn>
static void forEachStickerFile(const QString &libPath, Fn &&fn) {
    QDir root(libPath);
    const QStringList categoryDirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &categoryName : categoryDirs) {
        QDir categoryDir(libPath + "/" + categoryName);
        const auto files = categoryDir.entryInfoList(QDir::Files);
        for (const QFileInfo &fi : files)
            if (!isPreviewFile(fi.fileName()))
                fn(categoryName, fi);
    }
}