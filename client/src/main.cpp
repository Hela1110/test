#include <QApplication>
#include "login/loginwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    QCoreApplication::setOrganizationName("Shopping System");
    QCoreApplication::setApplicationName("Shopping Client");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    // 加载样式表
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
    }
    
    // 创建并显示登录窗口
    LoginWindow loginWindow;
    loginWindow.show();
    
    return app.exec();
}