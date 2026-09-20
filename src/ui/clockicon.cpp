#include "clockicon.h"

#include <QPainter>

QPixmap makeClockIcon(const QColor &accent, int size) {
    QPixmap source(":/assets/clock.png");
    if (source.isNull())
        return QPixmap();

    QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (scaled.isNull())
        return QPixmap();

    QPixmap result(scaled.size());
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.drawPixmap(0, 0, scaled);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(result.rect(), accent);
    p.end();

    return result;
}