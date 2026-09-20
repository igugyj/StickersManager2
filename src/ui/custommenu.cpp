#include "custommenu.h"
#include <QFile>

CustomMenu::CustomMenu(QWidget *parent)
    : QMenu(parent), m_style(new CustomMenuStyle())
{
    initStyle();
}

CustomMenu::CustomMenu(const QString &title, QWidget *parent)
    : QMenu(title, parent), m_style(new CustomMenuStyle())
{
    initStyle();
}

void CustomMenu::initStyle()
{
    // 设置自定义样式
    setStyle(m_style);

    // 设置无边框和透明背景，确保圆角正确显示
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    applyStyle();
}

void CustomMenu::applyStyle()
{
    QFile styleFile(":/assets/menu.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString styleSheet = QLatin1String(styleFile.readAll());
        setStyleSheet(styleSheet);
        styleFile.close();
    }
}
