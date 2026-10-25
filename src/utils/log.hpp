#pragma once
#include <QtGlobal>
#include <QString>
#include <QMutex>

// 定义日志文件路径
#define QT_LOG_FILE "log/log.log"

// 定义日志等级枚举（和QtMsgType对应，便于使用）
enum class LogLevel {
    Debug = 0, // 调试信息（最低级）
    Info = 1, // 普通信息
    Warning = 2, // 警告
    Critical = 3, // 严重错误
    Fatal = 4 // 致命错误（最高级）
};

// 全局日志配置（单实例；读写都在 log.cpp 的锁内进行，勿直接改动）
extern LogLevel g_logLevel;
extern QMutex g_logMutex;

// 初始化日志文件（创建 log/ 目录并清空上次日志）
void initLogFile();

// 默认日志等级：CONSOLE 构建 = Debug（全量），否则 = Warning（按构建类型区分）
LogLevel defaultLogLevel();

// 等级 <-> 字符串（"Debug"/"Info"/"Warning"/"Critical"/"Fatal"）
// 未知字符串回退到 def
QString logLevelToString(LogLevel level);
LogLevel logLevelFromString(const QString &name, LogLevel def);

// 设置/读取当前日志等级（内部加锁，线程安全）
void setLogLevel(LogLevel level);
LogLevel getLogLevel();

// Qt 消息处理函数：控制台构建写控制台 + 文件，非控制台只写文件
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
