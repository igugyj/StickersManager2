#include "imagepreviewdialog.h"
#include <QApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QImageReader>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QClipboard>
#include <QMimeData>
#include <QtConcurrent>
#include <QDebug>
#include "imageloader.h"
#include "tray.h"
#include "fsutil.hpp"

ImagePreviewDialog::ImagePreviewDialog(const QString &filePath, bool animatePreview,
                                       const QVector<QString> &siblings, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), m_animatePreview(animatePreview), m_siblings(siblings), m_filePath(filePath)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setStyleSheet(
        "ImagePreviewDialog{background-color:rgba(0,0,0,200);}"
        "ImagePreviewDialog QLabel,ImagePreviewDialog QPushButton{color:#fff;}"
        "ImagePreviewDialog QPushButton{background-color:rgba(255,255,255,30);"
        "border:none;padding:4px 8px;border-radius:3px;}"
        "ImagePreviewDialog QPushButton:hover{background-color:rgba(255,255,255,60);}"
        "ImagePreviewDialog QPushButton:disabled{color:rgba(255,255,255,80);}");

    m_index = qMax(0, m_siblings.indexOf(filePath));
    buildUi();
    showAt(m_index);
    QApplication::instance()->installEventFilter(this);
}

ImagePreviewDialog::~ImagePreviewDialog()
{
    QApplication::instance()->removeEventFilter(this);
    if (m_animWatcher)
    {
        m_animWatcher->cancel();
    }
    if (m_movie)
    {
        m_movie->stop();
    }
}

void ImagePreviewDialog::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 8);
    root->setSpacing(0);

    // ── Top: info bar ──
    m_infoLabel = new QLabel(this);
    m_infoLabel->setContentsMargins(12, 6, 12, 6);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_infoLabel->setStyleSheet(
        "QLabel{background-color:rgba(0,0,0,200);color:#fff;padding:4px 12px;}");
    root->addWidget(m_infoLabel);

    // ── Middle: image area with overlaid prev/next buttons ──
    m_imageArea = new QWidget(this);
    m_imageArea->setStyleSheet("QWidget{background-color:rgba(0,0,0,200);}");
    m_imageArea->setMouseTracking(true); // 捕获子部件鼠标事件

    QVBoxLayout *imageLayout = new QVBoxLayout(m_imageArea);
    imageLayout->setContentsMargins(0, 0, 0, 0);

    m_imageLabel = new QLabel(m_imageArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    imageLayout->addWidget(m_imageLabel);
    imageLayout->setAlignment(m_imageLabel, Qt::AlignCenter);

    // Left arrow
    m_prevBtn = new QPushButton(QChar(0x25C0), m_imageArea);
    m_prevBtn->setFixedSize(40, 60);
    m_prevBtn->setVisible(false);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setStyleSheet(
        "QPushButton{background-color:rgba(120,180,255,120);color:#fff;"
        "border:none;font-size:20px;border-radius:20px;}"
        "QPushButton:hover{background-color:rgba(120,180,255,220);}");

    // Right arrow
    m_nextBtn = new QPushButton(QChar(0x25B6), m_imageArea);
    m_nextBtn->setFixedSize(40, 60);
    m_nextBtn->setVisible(false);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setStyleSheet(
        "QPushButton{background-color:rgba(120,180,255,120);color:#fff;"
        "border:none;font-size:20px;border-radius:20px;}"
        "QPushButton:hover{background-color:rgba(120,180,255,220);}");

    connect(m_prevBtn, &QPushButton::clicked, this, &ImagePreviewDialog::onPrev);
    connect(m_nextBtn, &QPushButton::clicked, this, &ImagePreviewDialog::onNext);

    // Boundary hint
    m_boundaryLabel = new QLabel(m_imageArea);
    m_boundaryLabel->setStyleSheet(
        "QLabel{background-color:rgba(120,180,255,80);color:#fff;"
        "padding:6px 14px;border-radius:12px;font-size:13px;}");
    m_boundaryLabel->setFixedHeight(30);
    m_boundaryLabel->hide();

    root->addWidget(m_imageArea, 1);
}

void ImagePreviewDialog::showAt(int index)
{
    if (!m_siblings.isEmpty())
    {
        m_index = qBound(0, index, m_siblings.size() - 1);
        m_filePath = m_siblings.at(m_index);
    }

    if (m_movie)
    {
        m_movie->stop();
        delete m_movie;
        m_movie = nullptr;
    }
    if (m_animWatcher)
    {
        m_animWatcher->cancel();
        delete m_animWatcher;
        m_animWatcher = nullptr;
    }
    m_originalPixmap = QPixmap();
    m_frameSize = QSize();

    loadAndDisplayImage(m_filePath);
    emit currentFileChanged(m_filePath);
}

void ImagePreviewDialog::loadAndDisplayImage(const QString &filePath)
{
    if (m_animatePreview && ImageLoader::isAnimated(filePath))
    {
        QImageReader reader(filePath);
        QSize frameSize = reader.size();
        if (!frameSize.isValid())
        {
            m_imageLabel->setText("Failed to load image");
            return;
        }

        m_frameSize = frameSize;
        adjustDialogSize(frameSize);
        m_imageLabel->setText("Loading...");

        m_animWatcher = new QFutureWatcher<QByteArray>(this);
        connect(m_animWatcher, &QFutureWatcher<QByteArray>::finished, this, [this]()
                {
            if (!m_animWatcher || m_animWatcher->isCanceled()) return;
            QByteArray data = m_animWatcher->result();
            m_animWatcher->deleteLater();
            m_animWatcher = nullptr;
            if (data.isEmpty()) {
                m_imageLabel->setText("Failed to load");
                return;
            }
            auto *buffer = new QBuffer();
            buffer->setData(data);
            m_movie = new QMovie(buffer);
            buffer->setParent(m_movie);
            connect(m_movie, &QMovie::frameChanged, this, [this](int) {
                if (!m_movie) return;
                QPixmap framePix = m_movie->currentPixmap();
                if (framePix.isNull()) return;
                m_imageLabel->setPixmap(framePix.scaled(
                    m_displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            });
            if (m_animatePreview)
            {
                m_movie->start();
            }
            else
            {
                m_movie->stop();
            }
            updateInfoPanel(); });
        m_animWatcher->setFuture(QtConcurrent::run([filePath]() -> QByteArray
                                                   {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) return {};
            return file.readAll(); }));
        return;
    }

    QImage image = ImageLoader::loadImage(filePath);
    m_originalPixmap = QPixmap::fromImage(image);

    if (m_originalPixmap.isNull())
    {
        qWarning() << "Failed to load image for preview:" << filePath;
        m_imageLabel->setText("Failed to load image");
        return;
    }

    adjustDialogSize(m_originalPixmap.size());
    m_imageLabel->setPixmap(m_originalPixmap.scaled(
        m_displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    updateInfoPanel();
}

void ImagePreviewDialog::adjustDialogSize(const QSize &originalSize)
{
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int maxWidth = static_cast<int>(screenGeometry.width() * 0.8);
    int maxHeight = static_cast<int>(screenGeometry.height() * 0.8) - 120;

    m_displaySize = originalSize.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio);

    m_imageLabel->setFixedSize(m_displaySize);
    layout()->activate();
    adjustSize();

    QPoint center = screenGeometry.center() - rect().center();
    move(center);
}

void ImagePreviewDialog::updateInfoPanel()
{
    QFileInfo fi(m_filePath);
    QString fmt = formatFileType(m_filePath);
    QSize dim = m_originalPixmap.isNull() ? m_frameSize : m_originalPixmap.size();
    m_infoLabel->setText(
        QString("<b>%1</b> · %2 · %3 \u00d7 %4 px · %5 · %6")
            .arg(fi.fileName())
            .arg(fmt)
            .arg(dim.width())
            .arg(dim.height())
            .arg(formatBytes(fi.size()))
            .arg(fi.lastModified().toString(QLocale::system().dateFormat(QLocale::ShortFormat))));
}

void ImagePreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (!m_prevBtn || !m_nextBtn || !m_imageArea)
        return;
    int imgH = m_imageArea->height();
    int btnH = m_prevBtn->height();
    int y = (imgH - btnH) / 2;
    m_prevBtn->move(10, y);
    m_nextBtn->move(m_imageArea->width() - m_nextBtn->width() - 10, y);
}

// 辅助函数：更新导航按钮状态
void ImagePreviewDialog::updateNavigationButtons(const QPoint &mousePos)
{
    if (!m_imageArea)
        return;
    QPoint local = m_imageArea->mapFromGlobal(mousePos);
    int w = m_imageArea->width();
    bool inImage = m_imageArea->rect().contains(local);
    bool leftZone = inImage && local.x() < w * 0.10;
    bool rightZone = inImage && local.x() > w * 0.90;
    bool canPrev = m_siblings.size() > 1 && m_index > 0;
    bool canNext = m_siblings.size() > 1 && m_index < m_siblings.size() - 1;

    m_prevBtn->setVisible(leftZone && canPrev);
    m_nextBtn->setVisible(rightZone && canNext);

    if ((leftZone && !canPrev) || (rightZone && !canNext))
    {
        m_boundaryLabel->setText(leftZone ? "First" : "Last");
        m_boundaryLabel->adjustSize();
        int x = (m_imageArea->width() - m_boundaryLabel->width()) / 2;
        int y = static_cast<int>(m_imageArea->height() * 0.1);
        m_boundaryLabel->move(x, y);
        m_boundaryLabel->raise();
        m_boundaryLabel->show();
    }
    else
    {
        m_boundaryLabel->hide();
    }
}

// 事件过滤器：捕获 m_imageArea 及其子部件的鼠标移动
bool ImagePreviewDialog::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj)
    if (event->type() == QEvent::MouseMove && m_imageArea)
    {
        QPoint local = m_imageArea->mapFromGlobal(QCursor::pos());
        if (m_imageArea->rect().contains(local))
        {
            updateNavigationButtons(QCursor::pos());
        }
        else if (m_prevBtn->isVisible() || m_nextBtn->isVisible() || m_boundaryLabel->isVisible())
        {
            m_prevBtn->hide();
            m_nextBtn->hide();
            m_boundaryLabel->hide();
        }
    }
    return false;
}

// ── 鼠标按键与键盘事件 ──

void ImagePreviewDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        accept();
}

void ImagePreviewDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
        accept();
        break;
    case Qt::Key_Left:
        onPrev();
        break;
    case Qt::Key_Right:
        onNext();
        break;
    default:
        QDialog::keyPressEvent(event);
    }
}

// ── Navigation ──

void ImagePreviewDialog::onPrev() { showAt(m_index - 1); }
void ImagePreviewDialog::onNext() { showAt(m_index + 1); }
