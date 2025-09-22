#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>

namespace Ui {
class LoginWindow;
}

class LoginWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

private slots:
    void on_loginButton_clicked();
    void on_registerButton_clicked();
    void onConnected();
    void onDisconnected();
    void onReadyRead();

private:
    Ui::LoginWindow *ui;
    QTcpSocket *socket;
    void setupUi();
    void connectToServer();
    void initializeSocket();
};

#endif // LOGINWINDOW_H