#include "categorybutton.h"
#include "clockicon.h"
#include <QIcon>
#include <QDebug>
#include <QLabel>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QStyle>
#include "labeltagst.hpp"

CategoryButton::CategoryButton(const QString &categoryName,
                               int buttonSize,
                               QWidget *parent)
    : QPushButton(parent),
      m_categoryName(categoryName),
      m_buttonSize(buttonSize)
{
    setFixedSize(buttonSize, buttonSize);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip(categoryName);
    setContextMenuPolicy(Qt::CustomContextMenu);

    QColor bg = palette().color(QPalette::Button);
    QColor hl = palette().color(QPalette::Highlight);

    setStyleSheet(QString(R"(
        CategoryButton {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 8px;
        }
        CategoryButton:hover {
            border: 2px solid %3;
            background-color: %4;
        }
        CategoryButton:pressed {
            background-color: %3;
            border: 2px solid %3;
        }
    )")
        .arg(bg.name())
        .arg(bg.darker(130).name())
        .arg(hl.name())
        .arg(hl.lighter(185).name()));

    // 名称标签（左下角覆盖）
    m_nameLabel = new QLabel(m_categoryName, this);
    m_nameLabel->setStyleSheet(kOverlayLabelStyle);

    // 数量标签（右下角覆盖）
    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet(kOverlayLabelStyle);

    updateOverlayLabels();
}

void CategoryButton::setStickerCount(int count)
{
    setToolTip(m_categoryName + QString(" (%1)").arg(count));
    m_countLabel->setText(QString::number(count));
    updateOverlayLabels();
}

void CategoryButton::setShowName(bool show)
{
    m_showName = show;
    updateOverlayLabels();
}

void CategoryButton::setShowCount(bool show)
{
    m_showCount = show;
    updateOverlayLabels();
}

void CategoryButton::setShowClock(bool show)
{
    m_showClock = show;
    updateIcon();
}

void CategoryButton::updateOverlayLabels()
{
    m_nameLabel->setVisible(m_showName);
    m_countLabel->setVisible(m_showCount);

    m_countLabel->adjustSize();
    m_nameLabel->adjustSize();

    int margin = 3;
    int countW = m_showCount ? m_countLabel->width() : 0;
    int maxNameW = m_buttonSize - 2 * margin - countW - 12;
    if (maxNameW < 20)
        maxNameW = 20;

    QFontMetrics fm(m_nameLabel->font());
    m_nameLabel->setText(fm.elidedText(m_categoryName, Qt::ElideRight, maxNameW));
    m_nameLabel->adjustSize();
    m_nameLabel->move(margin, m_buttonSize - m_nameLabel->height() - margin);
    m_countLabel->move(m_buttonSize - m_countLabel->width() - margin,
                       m_buttonSize - m_countLabel->height() - margin);
}

void CategoryButton::setThumbnail(const QPixmap &pixmap)
{
    if (!pixmap.isNull())
    {
        m_currentPixmap = pixmap;
        updateIcon();
    }
}

void CategoryButton::resizeEvent(QResizeEvent *event)
{
    QPushButton::resizeEvent(event);
    if (m_showClock || !m_currentPixmap.isNull())
    {
        updateIcon();
    }
}

void CategoryButton::updateIcon()
{
    int iconSize = m_buttonSize - 10;
    if (iconSize < 10)
        iconSize = 10;

    if (m_showClock) {
        // Recent 类目：预览图
        setIcon(QIcon(makeClockIcon(palette().color(QPalette::Text), iconSize)));
        setIconSize(QSize(iconSize, iconSize));
        return;
    }

    if (m_currentPixmap.isNull())
        return;

    QPixmap scaledPixmap = m_currentPixmap.scaled(
        iconSize,
        iconSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    setIcon(QIcon(scaledPixmap));
    setIconSize(QSize(iconSize, iconSize));
}