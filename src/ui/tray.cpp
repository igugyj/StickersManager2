#include "tray.h"
#include "appinfo.h"
#include <QApplication>
#include <QCoreApplication>
#include <QStyle>
#include <QDebug>
#include "launcher.hpp"
#include "custommenu.h"

TrayIcon *TrayIcon::m_instance = nullptr;

TrayIcon *TrayIcon::instance()
{
    if (!m_instance)
    {
        m_instance = new TrayIcon();
    }
    return m_instance;
}

void TrayIcon::showMessage(const QString &title, const QString &msg,
                           MessageIcon icon, int timeout)
{
    if (m_instance)
    {
        m_instance->QSystemTrayIcon::showMessage(title, msg, icon, timeout);
    }
}

TrayIcon::TrayIcon(QObject *parent)
    : QSystemTrayIcon(parent)
{
    QString ico_path = ":/assets/st.png";
    QFileInfo ico(ico_path);
    if (ico.exists())
    {
        setIcon(QIcon(ico_path));
    }
    else
    {
        QIcon icon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
        setIcon(icon);
    }

    setToolTip(QString("%1 %2").arg(AppInfo::name()).arg(AppInfo::version()));

    menu = new CustomMenu();

    showSubMenu = new CustomMenu("Show");
    action_settings = new QAction("Settings", this);
    action_rescan = new QAction("Rescan", this);
    QAction *action_openPath = new QAction("Open App Dir", this);
    QAction *exitAction = new QAction("Exit", this);

    connect(exitAction, &QAction::triggered, []()
            { QCoreApplication::quit(); });

    connect(action_openPath, &QAction::triggered, []()
            {
        QString appDir = QCoreApplication::applicationDirPath();
        launch(appDir); });

    menu->addMenu(showSubMenu);
    menu->addActions({action_rescan, action_settings, action_openPath});
    menu->addSeparator();
    menu->addAction(exitAction);

    this->setContextMenu(menu);
    this->show();
}

void TrayIcon::updateShowMenu(const QVector<LibraryConfig> &libraries)
{
    showSubMenu->clear();

    for (int i = 0; i < libraries.size(); ++i)
    {
        const LibraryConfig &lib = libraries[i];
        if (lib.path.isEmpty())
            continue;

        QFileInfo dirInfo(lib.path);
        QString menuText = dirInfo.fileName();
        if (menuText.isEmpty())
        {
            menuText = lib.path;
        }

        QAction *action = new QAction(menuText, this);
        action->setData(lib.path);
        showSubMenu->addAction(action);
    }
}

TrayIcon::~TrayIcon()
{
    delete menu;
    m_instance = nullptr;
}
