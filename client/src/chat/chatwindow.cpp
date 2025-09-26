#include "chat/chatwindow.h"
#include "ui_chatwindow.h"
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <QKeyEvent>

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ChatWindow) {
    ui->setupUi(this);
    // 追加在线用户标签与对等方输入框、清空按钮
    onlineLabel = new QLabel(tr("在线用户: (未知)"), this);
    peerEdit = new QLineEdit(this);
    peerEdit->setPlaceholderText(tr("对话对象，空为群聊"));
    auto *bar = new QHBoxLayout();
    bar->addWidget(peerEdit); bar->addSpacing(8); bar->addWidget(onlineLabel);
    auto *host = new QWidget(this);
    host->setLayout(bar);
    ui->verticalLayout->insertWidget(0, host);
    // 添加“清空会话”按钮
    auto *clearBtn = new QPushButton(tr("清空会话"), this);
    connect(clearBtn, &QPushButton::clicked, this, &ChatWindow::on_clearButton_clicked);
    ui->horizontalLayout->addWidget(clearBtn);

    // 回车发送已在 ui 中通过 returnPressed 信号连接
    // 额外事件过滤：Enter 发送，Shift+Enter 换行
    if (ui->messageInput) ui->messageInput->installEventFilter(this);
}

ChatWindow::~ChatWindow() {
    delete ui;
}

void ChatWindow::on_sendButton_clicked() {
    sendMessage();
}

void ChatWindow::on_messageInput_returnPressed() {
    // 如果按下Shift+Enter，则插入换行符
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        ui->messageInput->insertPlainText("\n");
    } else {
        // 否则发送消息
        sendMessage();
    }
}

bool ChatWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->messageInput && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                // 允许换行
                return false;
            } else {
                sendMessage();
                return true; // 吞掉回车
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void ChatWindow::sendMessage() {
    QString message = ui->messageInput->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }
    // 本地追加
    appendLine(QString("[%1] 我: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(message));
    ui->messageInput->clear();
    // 发送到服务器
    if (socket) {
        QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["content"] = message;
        const QString to = peerEdit? peerEdit->text().trimmed() : QString(); if (!to.isEmpty()) r["to"] = to;
        QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
    }
}

void ChatWindow::sendDelete() {
    if (!socket) return;
    const QString to = peerEdit? peerEdit->text().trimmed() : QString();
    if (to.isEmpty()) { appendLine(tr("[系统] 请输入要清空的对话对象")); return; }
    QJsonObject r; r["type"] = "chat_delete"; if (!username.isEmpty()) r["username"] = username; r["peer"] = to;
    QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
}

void ChatWindow::appendLine(const QString &text) {
    auto *item = new QListWidgetItem(text);
    ui->chatList->addItem(item);
    ui->chatList->scrollToBottom();
}

void ChatWindow::initChat() {
    ui->chatList->clear();
    if (!socket) return;
    QJsonObject r; r["type"] = "chat_init"; if (!username.isEmpty()) r["username"] = username; const QString to = peerEdit? peerEdit->text().trimmed():QString(); if (!to.isEmpty()) r["peer"] = to; r["limit"] = 50;
    QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
}

void ChatWindow::handleMessage(const QJsonObject &msg) {
    const QString type = msg.value("type").toString();
    if (type == QLatin1String("chat_init_response")) {
        // 历史
        ui->chatList->clear();
        const QJsonArray arr = msg.value("messages").toArray();
        for (const auto &v : arr) {
            const auto o = v.toObject();
            const QString ts = o.value("createdAt").toString();
            const QString from = o.value("from").toString();
            const QString to = o.value("to").toString();
            const QString content = o.value("content").toString();
            appendLine(QString("[%1] %2%3: %4").arg(ts).arg(from).arg(to.isEmpty()?" -> 全体":(" -> "+to)).arg(content));
        }
        // 在线用户
        QStringList ons; for (const auto &v : msg.value("onlineUsers").toArray()) ons << v.toString();
        if (onlineLabel) onlineLabel->setText(tr("在线用户: ") + (ons.isEmpty()? tr("(无)"): ons.join(", ")));
    } else if (type == QLatin1String("chat_message")) {
        const QString ts = msg.value("createdAt").toString();
        const QString from = msg.value("from").toString();
        const QString to = msg.value("to").toString();
        const QString content = msg.value("content").toString();
        appendLine(QString("[%1] %2%3: %4").arg(ts).arg(from).arg(to.isEmpty()?" -> 全体":(" -> "+to)).arg(content));
    } else if (type == QLatin1String("presence")) {
        // 简单提示
        const QString ev = msg.value("event").toString();
        const QString who = msg.value("username").toString();
        appendLine(QString("[系统] 用户 %1 %2").arg(who).arg(ev == "online" ? tr("上线") : tr("离线")));
        // 触发重载在线列表
        initChat();
    } else if (type == QLatin1String("chat_delete_response")) {
        appendLine(tr("[系统] 会话已清空"));
        initChat();
    }
}

void ChatWindow::on_clearButton_clicked() { sendDelete(); }