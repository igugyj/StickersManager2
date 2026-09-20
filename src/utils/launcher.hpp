#pragma once

#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QMetaObject>
#include "tray.h"

static void notifyTray(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon)
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), [title, msg, icon]()
                              { TrayIcon::showMessage(title, msg, icon, 5000); }, Qt::QueuedConnection);
}

static QFuture<void> launch(const QString &path)
{
    qDebug() << "launching: " << path;
    return QtConcurrent::run([path]() -> bool
                             {
        try {
            bool success = false;

            // 对于URL，使用QDesktopServices
            if (path.startsWith("http://") || path.startsWith("https://") ||
                path.startsWith("ftp://") || path.startsWith("file://")) {
                success = QDesktopServices::openUrl(QUrl(path));
            }
                // 对于本地文件/文件夹，使用QDesktopServices
            else {
                QFileInfo fileInfo(path);
                if (fileInfo.exists()) {
                    QUrl localUrl = QUrl::fromLocalFile(QDir::toNativeSeparators(path));
                    success = QDesktopServices::openUrl(localUrl);
                } else {
                    qWarning() << "File or directory does not exist:" << path;
                    notifyTray(QObject::tr("Warning"),
                               QObject::tr("File or directory does not exist: %1").arg(path),
                               QSystemTrayIcon::Warning);
                    return false;
                }
            }

            if (!success) {
                qWarning() << "Failed to open:" << path;
                notifyTray(QObject::tr("Warning"),
                           QObject::tr("Failed to open: %1").arg(path),
                           QSystemTrayIcon::Warning);
                return false;
            }

            return true;
        } catch (const std::exception &e) {
            qCritical() << "Exception occurred while launching" << path << ":" << e.what();
            notifyTray(QObject::tr("Error"),
                       QObject::tr("Exception occurred while launching: %1 \n%2").arg(path).arg(
                               e.what()),
                       QSystemTrayIcon::Critical);
            return false;
        } catch (...) {
            qCritical() << "Unknown exception occurred while launching" << path;
            notifyTray(QObject::tr("Error"),
                       QObject::tr("Unknown exception occurred while launching: %1").arg(path),
                       QSystemTrayIcon::Critical);
            return false;
        } });
}
