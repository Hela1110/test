#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // 避免窗口切换（登录 -> 主窗）瞬间无可见窗口而退出应用
    app.setQuitOnLastWindowClosed(false);
    
    // 设置应用程序信息
    QCoreApplication::setOrganizationName("Shopping System");
    QCoreApplication::setApplicationName("Shopping Client");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    // Init simple file logging next to the executable
    const QString logPath = QCoreApplication::applicationDirPath() + "/client.log";
    g_logFile = new QFile(logPath);
    if (g_logFile->open(QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(qtFileMessageHandler);
        qInfo() << "Application starting. Log:" << logPath;
    }

    // 加载样式表
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
    }
    
    // 创建并显示登录窗口
    LoginWindow loginWindow;
    qInfo() << "LoginWindow created and shown";
    loginWindow.show();
    
    return app.exec();
}