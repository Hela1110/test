#include "chat/chatwindow.h"
#include "ui_chatwindow.h"
#include <QDateTime>

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ChatWindow) {
    ui->setupUi(this);

    // 连接回车键发送消息的信号槽
    connect(ui->messageInput, &QTextEdit::returnPressed,
            this, &ChatWindow::on_messageInput_returnPressed);
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

void ChatWindow::sendMessage() {
    QString message = ui->messageInput->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }

    // 获取当前时间
    QString currentTime = QDateTime::currentDateTime().toString("hh:mm:ss");

    // 创建消息项
    QListWidgetItem* item = new QListWidgetItem;
    item->setText(QString("[%1] 我: %2").arg(currentTime).arg(message));
    ui->chatList->addItem(item);

    // 清空输入框
    ui->messageInput->clear();

    // 滚动到最新消息
    ui->chatList->scrollToBottom();

    // TODO: 发送消息到服务器
}