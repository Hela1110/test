#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QIcon>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include "login/loginwindow.h"

// Simple global log file and message handler
static QFile* g_logFile = nullptr;
static QMutex g_logMutex;
static void qtFileMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    // 日志可能来自非 GUI 线程，使用互斥与原子写防止竞争/交错
    if (!g_logFile) return;
    if (!g_logFile->isOpen()) return;
    const char* level = "INFO";
    switch (type) {
    case QtDebugMsg:   level = "DEBUG"; break;
    case QtInfoMsg:    level = "INFO";  break;
    case QtWarningMsg: level = "WARN";  break;
    case QtCriticalMsg:level = "ERROR"; break;
    case QtFatalMsg:   level = "FATAL"; break;
    }
    quintptr tid = reinterpret_cast<quintptr>(QThread::currentThreadId());
    const QString line = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]")
                       + QString::fromLatin1(" [") + QString::fromLatin1(level) + QString::fromLatin1("] ")
                       + QString::fromLatin1("(T0x") + QString::number(tid, 16) + QString::fromLatin1(") ")
                       + msg + QLatin1Char('\n');
    QMutexLocker locker(&g_logMutex);
    g_logFile->write(line.toUtf8());
    g_logFile->flush();
}

// Minimal startup trace to diagnose early exits before logger ready
static void writeStartupTrace(const QString &stage)
{
    QFile f(QCoreApplication::applicationDirPath() + "/startup.trace");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]")
           << " " << stage << '\n';
    }
}

int main(int argc, char *argv[]) {
    // Pre-application trace using argv[0] directory
    {
        QString exePath = QString::fromLocal8Bit(argv[0]);
        QFileInfo fi(exePath);
        QString preTracePath = fi.absolutePath() + "/startup.trace";
        QFile f(preTracePath);
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]")
               << " pre-0: entering main()" << '\n';
        }
    }

    // Qt 6 默认启用高 DPI 处理与像素映射，无需显式设置 AA_UseHighDpiPixmaps
    QApplication app(argc, argv);
    writeStartupTrace("stage-1: QApplication constructed");
    // 避免窗口切换（登录 -> 主窗）瞬间无可见窗口而退出应用
    app.setQuitOnLastWindowClosed(false);
    writeStartupTrace("stage-2: setQuitOnLastWindowClosed(false)");
    
    // 设置应用程序信息
    QCoreApplication::setOrganizationName("Shopping System");
    QCoreApplication::setApplicationName("Shopping Client");
    QCoreApplication::setApplicationVersion("1.0.0");
    writeStartupTrace("stage-3: app meta set");
    
    // Init simple file logging next to the executable (fixed path)
    QString logPath = QCoreApplication::applicationDirPath() + "/client.log";
    g_logFile = new QFile(logPath);
    if (g_logFile->open(QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(qtFileMessageHandler);
        qInfo() << "Application starting. Log:" << logPath;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [](){ qInfo() << "aboutToQuit received"; });
    writeStartupTrace("stage-4: logger initialized (or attempted)");

    // 设置全局窗口图标
    QIcon appIcon(":/icons/mall.svg");
    app.setWindowIcon(appIcon);

    // 加载橙色主题样式表
    QFile styleFile(":/styles/orange_theme.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
        qInfo() << "Orange theme stylesheet loaded successfully";
    } else {
        qWarning() << "Failed to load orange theme stylesheet, trying fallback";
        // 备用方案:加载原始样式
        QFile fallbackFile(":/styles/style.qss");
        if (fallbackFile.open(QFile::ReadOnly)) {
            QString style = QLatin1String(fallbackFile.readAll());
            app.setStyleSheet(style);
        }
    }
    writeStartupTrace("stage-5: stylesheet loaded (if any)");
    
    // 创建并显示登录窗口
    LoginWindow loginWindow;
    writeStartupTrace("stage-6: LoginWindow constructed");
    qInfo() << "LoginWindow created and shown";
    loginWindow.show();
    writeStartupTrace("stage-7: LoginWindow shown");
    
    writeStartupTrace("stage-8: entering app.exec()");
    int code = app.exec();
    writeStartupTrace(QString("stage-9: app.exec() returned %1").arg(code));
    return code;
}