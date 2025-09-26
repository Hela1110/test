#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

// 前置声明，避免在头文件中包含大量 Qt 头
class QTcpSocket;
class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTimer;
class QJsonObject;

namespace Ui {
class ChatWindow;
}

class ChatWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();
    void setSocket(QTcpSocket *s) { socket = s; }
    void setUsername(const QString &u) { username = u; }
    void initChat();
    void handleMessage(const QJsonObject &msg);

private slots:
    void on_sendButton_clicked();
    void on_messageInput_returnPressed();
    void on_clearButton_clicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::ChatWindow *ui;
    QTcpSocket *socket = nullptr;
    QString username;
    QLineEdit *peerEdit = nullptr;
    QLabel *onlineLabel = nullptr; // 保留但不再作为主显示
    QComboBox *onlineCombo = nullptr; // 在线用户下拉
    QPushButton *undoDeleteBtn = nullptr; // 撤销删除按钮（短暂显示）
    QTimer *deleteTimer = nullptr;       // 延时删除定时器
    QString pendingDeletePeer;           // 待删除的对话对象
    void sendMessage();
    void sendDelete();
    void appendLine(const QString &text);
    void appendBubble(const QString &from, const QString &to, const QString &content, const QString &ts, bool isSelf);
    void appendSystem(const QString &text);
};

#endif // CHATWINDOW_H