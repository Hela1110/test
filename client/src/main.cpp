#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include "login/loginwindow.h"

// Simple global log file and message handler
static QFile* g_logFile = nullptr;
static void qtFileMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if (!g_logFile) return;
    if (!g_logFile->isOpen()) return;
    QTextStream ts(g_logFile);
    const char* level = nullptr;
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }
    ts << QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]")
       << " [" << level << "] " << msg << '\n';
    ts.flush();
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
    writeStartupTrace("stage-4: logger initialized (or attempted)");

    // 加载样式表
    QFile styleFile(":/styles/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
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