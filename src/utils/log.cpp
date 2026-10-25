#include "log.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMutexLocker>

#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 单一全局实例：避免头文件 static 导致每个编译单元各有一份副本
// （setLogLevel 改不到 messageHandler 所用副本的隐患）
LogLevel g_logLevel = defaultLogLevel();
QMutex g_logMutex;

// 初始化日志文件
void initLogFile() {
    // 创建日志目录（包括父目录）
    QDir().mkpath("log");

    // 初始化时清空原有日志文件
    QFileInfo fileInfo(QT_LOG_FILE);
    if (fileInfo.exists()) {
        QFile::remove(QT_LOG_FILE);
    }
}

// 默认日志等级（按构建类型区分）：控制台构建 = Debug，否则 = Warning
LogLevel defaultLogLevel() {
#ifdef CONSOLE
    return LogLevel::Debug;
#else
    return LogLevel::Warning;
#endif
}

QString logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info: return "Info";
        case LogLevel::Warning: return "Warning";
        case LogLevel::Critical: return "Critical";
        case LogLevel::Fatal: return "Fatal";
    }
    return "Debug";
}

LogLevel logLevelFromString(const QString &name, LogLevel def) {
    static const struct { const char *name; LogLevel level; } map[] = {
        {"Debug", LogLevel::Debug},
        {"Info", LogLevel::Info},
        {"Warning", LogLevel::Warning},
        {"Critical", LogLevel::Critical},
        {"Fatal", LogLevel::Fatal},
    };
    for (const auto &e : map) {
        if (name.compare(QLatin1String(e.name), Qt::CaseInsensitive) == 0)
            return e.level;
    }
    return def;
}

// 设置日志等级（外部调用接口，支持动态调整）
void setLogLevel(LogLevel level) {
    QMutexLocker locker(&g_logMutex);
    g_logLevel = level;
}

// 读取日志等级（加锁，保证与 messageHandler 的过滤判断一致）
LogLevel getLogLevel() {
    QMutexLocker locker(&g_logMutex);
    return g_logLevel;
}

// 将QtMsgType转换为自定义LogLevel
static LogLevel qtMsgTypeToLogLevel(QtMsgType type) {
    switch (type) {
        case QtDebugMsg: return LogLevel::Debug;
        case QtInfoMsg: return LogLevel::Info;
        case QtWarningMsg: return LogLevel::Warning;
        case QtCriticalMsg: return LogLevel::Critical;
        case QtFatalMsg: return LogLevel::Fatal;
        default: return LogLevel::Info;
    }
}

static const char *logLevelName(QtMsgType type) {
    switch (type) {
        case QtDebugMsg: return "Debug";
        case QtInfoMsg: return "Info";
        case QtWarningMsg: return "Warning";
        case QtCriticalMsg: return "Critical";
        case QtFatalMsg: return "Fatal";
        default: return "Unknown";
    }
}

// 控制台输出：Windows 下用 WriteConsoleW 直写 UTF-16 宽字符，
// 绕开控制台代码页（GBK 下写 UTF-8 字节会中文乱码）；
// 输出被重定向（管道/文件，拿到的不是控制台句柄）时回退为 UTF-8 字节写 stderr
static void writeToConsole(const QString &txt) {
#ifdef Q_OS_WIN
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        DWORD written = 0;
        WriteConsoleW(h,
                      reinterpret_cast<const wchar_t *>(txt.utf16()),
                      static_cast<DWORD>(txt.size()),
                      &written,
                      nullptr);
        WriteConsoleW(h, L"\n", 1, &written, nullptr);
        return;
    }
#endif
    const QByteArray u = txt.toUtf8();
    std::fwrite(u.constData(), 1, static_cast<size_t>(u.size()), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

// 文件输出（UTF-8，追加）
static void writeToFile(const QString &txt) {
    QFile logFile(QT_LOG_FILE);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream textStream(&logFile);
    textStream.setEncoding(QStringConverter::Utf8); // 确保中文正常显示
    textStream << txt << Qt::endl;
}

// 自定义消息处理函数（核心）
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context)

    // 1. 加锁保证线程安全，级别不够直接返回
    QMutexLocker locker(&g_logMutex);
    if (static_cast<int>(qtMsgTypeToLogLevel(type)) < static_cast<int>(g_logLevel))
        return;

    // 2. 拼接日志内容
    QString logTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString txt = QString("[%1] [%2]: %3")
                      .arg(logTime)
                      .arg(QString::fromLatin1(logLevelName(type)))
                      .arg(msg);

    // 3. 输出到控制台（仅 CONSOLE 构建；非控制台构建没有句柄，写了也无处可见）
#ifdef CONSOLE
    writeToConsole(txt);
#endif

    // 4. 写入日志文件
    writeToFile(txt);
}
