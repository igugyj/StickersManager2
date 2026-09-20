#pragma once
#include "updatechecker.h"

static void checkupdate(bool flag)
{
    if (!flag)
        return;
    auto *checker = new UpdateChecker();
    QObject::connect(checker, &UpdateChecker::finished, [checker](bool success, const QString &latestVersion, const QString &)
                     {
            if (success && UpdateChecker::compareVersions(latestVersion, AppInfo::version()) > 0) {
                TrayIcon::showMessage("Update Available",
                                      "Stickers Manager " + latestVersion + " is now available.");
            }
            checker->deleteLater(); });
    checker->check();
}