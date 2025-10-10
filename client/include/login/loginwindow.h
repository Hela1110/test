#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QList>
#include <QAbstractSocket>
#include <QShowEvent>
#include <QTimer>

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
    void onSocketError(QAbstractSocket::SocketError socketError);
    void sendHeartbeat();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::LoginWindow *ui;
    QTcpSocket *socket;
    QList<QByteArray> m_pendingWrites;
    QTimer m_reconnectTimer;
    QTimer m_heartbeatTimer;
    int m_reconnectAttempts = 0;
    void setupUi();
    void connectToServer();
    void initializeSocket();
    void sendJson(const QJsonObject& obj);
    void scheduleReconnect();
};

#endif // LOGINWINDOW_H