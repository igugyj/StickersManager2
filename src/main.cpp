#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QStyleFactory>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QIcon>
#include "log.hpp"
#include "configmanager.h"
#include "appcontroller.h"

int main(int argc, char *argv[])
{
    initLogFile();
    qInstallMessageHandler(messageHandler); // 始终安装：控制台 + 日志文件双写
    QApplication app(argc, argv);
    app.setApplicationName("Stickers Manager");
    app.setWindowIcon(QIcon(":/assets/st.png"));
    QString userName = QFileInfo(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).fileName();
    userName = !userName.isEmpty() ? userName : "unknown";
    // 测试版（Debug 构建，CMake 注入 SM_TEST_BUILD）用独立锁名，可与正式版同时运行；
    // Release 构建锁名保持不变
#ifdef SM_TEST_BUILD
    QString lockName = QString("%1_%2_debug.lock").arg(userName).arg(app.applicationName());
#else
    QString lockName = QString("%1_%2.lock").arg(userName).arg(app.applicationName());
#endif
    QLockFile lockFile(QDir::temp().absoluteFilePath(lockName));

    if (!lockFile.tryLock(100))
    {
        QMessageBox::warning(nullptr, "Warning", "There is already an instance running!");
        return 1;
    }
    app.setQuitOnLastWindowClosed(false);
    QStringList styles = QStyleFactory::keys();
    if (styles.contains("Fusion", Qt::CaseInsensitive))
        app.setStyle("Fusion");

    // 业务逻辑（窗口管理 / 全局热键 / 托盘 / 设置对话框）见 src/ui/appcontroller.*
    ConfigManager config;
    AppController controller(config);
    controller.startup();

    return app.exec();
}
