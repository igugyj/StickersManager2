#include "stickercell.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QResizeEvent>
#include <QFileInfo>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>
#include <QBuffer>
#include <QFontMetrics>
#include <QtConcurrent>
#include "tray.h"
#include "imageloader.h"
#include "fsutil.hpp"
#include "labeltagst.hpp"

StickerCell::StickerCell(const QString &filePath, int cellSize, QWidget *parent)
    : QFrame(parent)
      , m_filePath(filePath)
      , m_cellSize(cellSize)
      , m_isHighlighted(false)
      , m_hasRealThumbnail(false) {
    setFixedSize(cellSize, cellSize);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    // 图片标签
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(false); // 让Qt自动处理缩放

    // 使用布局确保标签填满整个单元格
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(0);
    layout->addWidget(m_imageLabel);

    // 文件类型标签（左下角覆盖）
    QFileInfo fi(m_filePath);
    m_tagLabel = new QLabel(formatFileType(m_filePath), this);
    m_tagLabel->setStyleSheet(kOverlayLabelStyle);
    m_tagLabel->adjustSize();
    m_tagLabel->move(7, cellSize - m_tagLabel->height() - 7);
    m_tagLabel->hide();

    // 文件名标签（左上角覆盖，不含后缀）
    m_fileNameLabel = new QLabel(fi.completeBaseName(), this);
    m_fileNameLabel->setStyleSheet(kOverlayLabelStyle);
    QFontMetrics nameFm(m_fileNameLabel->font());
    m_fileNameLabel->setText(nameFm.elidedText(fi.completeBaseName(), Qt::ElideRight, cellSize - 24));
    m_fileNameLabel->adjustSize();
    m_fileNameLabel->move(7, 7);

    // 文件大小标签（右下角覆盖）
    m_sizeLabel = new QLabel(formatBytes(fi.size()), this);
    m_sizeLabel->setStyleSheet(kOverlayLabelStyle);
    m_sizeLabel->adjustSize();
    m_sizeLabel->move(cellSize - m_sizeLabel->width() - 7,
                      cellSize - m_sizeLabel->height() - 7);

    // 初始设置为占位图
    setPlaceholder();
}

StickerCell::~StickerCell() {
    if (m_animWatcher) {
        m_animWatcher->cancel();
    }
    if (m_movie) {
        m_movie->stop();
    }
}

void StickerCell::setThumbnail(const QPixmap &pixmap) {
    if (pixmap.isNull()) {
        qWarning() << "Attempting to set empty thumbnail to cell:" << m_filePath;
        return;
    }

    m_currentPixmap = pixmap;
    m_hasRealThumbnail = true;

    // 缩略图已在加载线程按 cell 尺寸缩放好，直接显示
    m_imageLabel->setPixmap(pixmap);
    if (m_showTag) m_tagLabel->show();

    if (m_animateEnabled && ImageLoader::isAnimated(m_filePath) && m_inViewport) {
        loadAnimation();
    }

    // 强制更新显示
    m_imageLabel->update();
    update();
    qDebug() << "StickerCell: Thumbnail set, size:" << pixmap.size();
}

void StickerCell::setPlaceholder() {
    // 创建一个简单的占位图
    // QPixmap placeholder(m_cellSize - 10, m_cellSize - 10);
    // placeholder.fill(Qt::transparent); // 使用透明背景，避免覆盖
    //
    // m_imageLabel->setPixmap(placeholder);
    m_imageLabel->clear();
    m_imageLabel->setText("..."); // 或者显示加载中
    m_tagLabel->hide();
    m_hasRealThumbnail = false;
}

void StickerCell::loadAnimation() {
    if (m_animWatcher || m_movie) return;

    QString filePath = m_filePath;
    m_animWatcher = new QFutureWatcher<QByteArray>(this);
    connect(m_animWatcher, &QFutureWatcher<QByteArray>::finished, this, [this]() {
        if (!m_animWatcher || m_animWatcher->isCanceled()) return;
        QByteArray data = m_animWatcher->result();
        m_animWatcher->deleteLater();
        m_animWatcher = nullptr;
        if (data.isEmpty() || !m_inViewport) {
            // 不可见则不创建 QMovie，丢弃数据
            return;
        }
        auto *buffer = new QBuffer();
        buffer->setData(data);
        m_movie = new QMovie(buffer);
        // 默认 CacheNone — 只保留当前帧，不缓存全部帧到内存
        buffer->setParent(m_movie);
        connect(m_movie, &QMovie::frameChanged, this, [this](int) {
            if (!m_movie) return;
            QPixmap framePix = m_movie->currentPixmap();
            if (framePix.isNull()) return;
            QSize labelSize = m_imageLabel->size();
            if (labelSize.isEmpty()) return;
            m_imageLabel->setPixmap(framePix.scaled(
                labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        });
        m_movie->start();
    });
    m_animWatcher->setFuture(QtConcurrent::run([filePath]() -> QByteArray {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll();
    }));
}

void StickerCell::unloadAnimation() {
    if (m_animWatcher) {
        m_animWatcher->cancel();
        delete m_animWatcher;
        m_animWatcher = nullptr;
    }
    if (m_movie) {
        m_movie->stop();
        delete m_movie;
        m_movie = nullptr;
    }
}

void StickerCell::setAnimateEnabled(bool enabled) {
    m_animateEnabled = enabled;
    if (!enabled) {
        unloadAnimation();
    }
}

void StickerCell::setInViewport(bool visible) {
    if (m_inViewport == visible) return;
    m_inViewport = visible;
    if (visible && m_animateEnabled) {
        if (ImageLoader::isAnimated(m_filePath)) {
            loadAnimation();
        }
    } else {
        unloadAnimation();
    }
}

void StickerCell::setShowTag(bool show) {
    m_showTag = show;
    if (show && m_hasRealThumbnail) {
        m_tagLabel->show();
    } else {
        m_tagLabel->hide();
    }
}

void StickerCell::setShowFileName(bool show) {
    m_showFileName = show;
    m_fileNameLabel->setVisible(show);
}

void StickerCell::setShowFileSize(bool show) {
    m_showFileSize = show;
    m_sizeLabel->setVisible(show);
}

void StickerCell::setHighlighted(bool on) {
    if (m_isHighlighted == on) return;
    m_isHighlighted = on;
    if (on) {
        QColor hl = QApplication::palette().color(QPalette::Highlight);
        QString ss = QString("QFrame { background-color: %1; border: 2px solid %2; }")
                          .arg(hl.lighter(180).name()).arg(hl.name());
        qDebug() << "[CELL] setHighlighted ON:" << m_filePath << "bg=" << hl.lighter(180).name() << "border=" << hl.name() << "ss=" << ss;
        setStyleSheet(ss);
    } else {
        qDebug() << "[CELL] setHighlighted OFF:" << m_filePath;
        setStyleSheet(QString());
    }
    update();
}

void StickerCell::clearHighlight() {
    setHighlighted(false);
}

void StickerCell::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton)
        emit rightClicked(m_filePath);
    else
        emit clicked(m_filePath);
}

void StickerCell::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!m_copyOnDblClick) return;

    if (event->button() == Qt::RightButton)
    {
        return;
    }

    if (!QFile::exists(m_filePath)) {
        qWarning() << "File does not exist:" << m_filePath;
        TrayIcon::showMessage("Warning", "File not found: " + m_filePath);
        return;
    }

    // 2. 检查文件是否可读
    QFileInfo fileInfo(m_filePath);
    if (!fileInfo.isReadable()) {
        qWarning() << "File is not readable:" << m_filePath;
        TrayIcon::showMessage("Warning", "File not accessible: " + m_filePath);
        return;
    }

    // 3. 获取剪贴板
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning() << "Cannot get clipboard";
        return;
    }

    // 4. 清理剪贴板内容（可选，防止冲突）
    clipboard->clear();

    // 5. 创建MIME数据
    QMimeData *mimeData = new QMimeData();

    // 设置文件URL（支持文件拖拽/粘贴）
    QList<QUrl> urls;
    urls.append(QUrl::fromLocalFile(m_filePath));
    mimeData->setUrls(urls);
    // 设置文本（作为备份）
    mimeData->setText(m_filePath);

    // 6. 设置剪贴板数据 - 使用QApplication::clipboard()而不是局部变量
    QApplication::clipboard()->setMimeData(mimeData);

    // 7. 强制同步剪贴板（Windows可能需要）
#ifdef Q_OS_WINDOWS
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#endif

    // 8. 验证复制是否成功
    QTimer::singleShot(50, this, [this]() {
        const QMimeData *clipboardData = QApplication::clipboard()->mimeData();
        if (clipboardData && clipboardData->hasUrls()) {
            QString firstUrl = clipboardData->urls().first().toLocalFile();
            if (QFileInfo(firstUrl).canonicalFilePath() == QFileInfo(m_filePath).canonicalFilePath()) {
                qDebug() << "Successfully copied file to clipboard:" << m_filePath;
                TrayIcon::showMessage("Info", "File copied to clipboard: " + QFileInfo(m_filePath).fileName());
            } else {
                qWarning() << "Clipboard content mismatch";
            }
        } else {
            qWarning() << "Clipboard does not contain file URL";
        }
    });

    // 9. 发射双击信号
    emit doubleClicked(m_filePath);
}

void StickerCell::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);

    QSize labelSize = m_imageLabel->size();

    if (m_movie && m_movie->state() == QMovie::Running) {
        QPixmap framePix = m_movie->currentPixmap();
        if (!framePix.isNull()) {
            m_imageLabel->setPixmap(framePix.scaled(
                labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}
