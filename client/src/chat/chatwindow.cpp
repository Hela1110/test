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
#include <QColor>
#include <QSize>
#include <QFont>
#include <QComboBox>
#include <QMessageBox>
#include <QTimer>
#include <QHBoxLayout>
#include <QListWidget>

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ChatWindow) {
    ui->setupUi(this);
    // 追加在线用户标签与对等方输入框、清空按钮
    onlineLabel = new QLabel(tr("在线用户: (未知)"), this);
    onlineCombo = new QComboBox(this);
    onlineCombo->setEditable(false);
    onlineCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    peerEdit = new QLineEdit(this);
    // 顶部条：在线下拉 + 标签（初始化仅包含“全体”，真实在线列表在 chat_init_response 收到后重建）
    onlineCombo->clear();
    onlineCombo->addItem(QStringLiteral("全体"));
    auto *bar = new QHBoxLayout();
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(8);
    bar->addWidget(new QLabel(tr("在线:"), this));
    bar->addWidget(onlineCombo);
    bar->addSpacing(6);
    bar->addWidget(onlineLabel);
    auto *host = new QWidget(this);
    host->setLayout(bar);
    ui->verticalLayout->insertWidget(0, host);
    // 添加“清空会话”按钮
    auto *clearBtn = new QPushButton(tr("清空会话"), this);
    connect(clearBtn, &QPushButton::clicked, this, &ChatWindow::on_clearButton_clicked);
    ui->horizontalLayout->addWidget(clearBtn);
    // 新增：删除与对方的历史（仅私聊有效）
    auto *delPeerBtn = new QPushButton(tr("删除与对方的历史"), this);
    connect(delPeerBtn, &QPushButton::clicked, this, [this](){
        if (!socket) return;
        const QString to = peerEdit? peerEdit->text().trimmed() : QString();
        if (to.isEmpty() || to == QStringLiteral("全体")) {
            appendSystem(tr("[系统] 请选择具体用户进行删除，群聊不支持该操作"));
            return;
        }
        // 确认对话框
        auto reply = QMessageBox::question(this, tr("确认删除"), tr("确定要删除与 %1 的历史记录吗？此操作可在 5 秒内撤销。").arg(to),
                                           QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        // 设置待删除状态并显示撤销按钮，5 秒后真正发送删除请求
        pendingDeletePeer = to;
        if (!deleteTimer) {
            deleteTimer = new QTimer(this);
            deleteTimer->setSingleShot(true);
            connect(deleteTimer, &QTimer::timeout, this, [this](){
                if (!socket) return;
                if (pendingDeletePeer.isEmpty()) return; // 已撤销
                QJsonObject r; r["type"] = "chat_delete"; if (!username.isEmpty()) r["username"] = username; r["peer"] = pendingDeletePeer;
                QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                appendSystem(tr("[系统] 正在删除与 %1 的历史...").arg(pendingDeletePeer));
                pendingDeletePeer.clear();
                if (undoDeleteBtn) undoDeleteBtn->setVisible(false);
            });
        }
        deleteTimer->start(5000);
        if (!undoDeleteBtn) {
            undoDeleteBtn = new QPushButton(tr("撤销删除"), this);
            undoDeleteBtn->setToolTip(tr("在 5 秒内撤销删除操作"));
            ui->horizontalLayout->addWidget(undoDeleteBtn);
            connect(undoDeleteBtn, &QPushButton::clicked, this, [this](){
                if (deleteTimer && deleteTimer->isActive()) deleteTimer->stop();
                pendingDeletePeer.clear();
                if (undoDeleteBtn) undoDeleteBtn->setVisible(false);
                appendSystem(tr("[系统] 已撤销删除操作"));
            });
        }
        undoDeleteBtn->setVisible(true);
        appendSystem(tr("[系统] 将在 5 秒后删除与 %1 的历史，可点击‘撤销删除’取消。 ").arg(to));
    });
    ui->horizontalLayout->addWidget(delPeerBtn);

    // 选择在线用户即切换对话对象
    connect(onlineCombo, &QComboBox::currentTextChanged, this, [this](const QString &u){
        if (!peerEdit) return;
        const QString t = u.trimmed();
        // 选择“全体”表示群聊（peer 置空）
        if (t == QStringLiteral("全体")) {
            peerEdit->setText("");
            initChat();
            return;
        }
        // 忽略选择自身
        if (!username.isEmpty() && t == username) return;
        if (t.isEmpty()) return;
        peerEdit->setText(t);
        initChat();
    });

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
    // 本地追加（右侧绿色气泡）
    appendBubble(username.isEmpty()?QStringLiteral("我"):username, peerEdit?peerEdit->text().trimmed():QString(), message,
                     QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), /*isSelf=*/true);
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

void ChatWindow::appendSystem(const QString &text) {
    auto *item = new QListWidgetItem(text);
    QFont f = item->font(); f.setItalic(true);
    item->setFont(f);
    item->setForeground(QColor(120,120,120));
    ui->chatList->addItem(item);
    ui->chatList->scrollToBottom();
}

void ChatWindow::appendBubble(const QString &from, const QString &to, const QString &content, const QString &ts, bool isSelf) {
    // 文本 + 简易 HTML 气泡
    QString arrow = isSelf ? QStringLiteral("▶") : QStringLiteral("◀");
    QString who = isSelf ? tr("我") : from;
    QString toText = to.isEmpty()? tr("(群)") : to;
    QString bubbleColor = isSelf ? "#C8F7C5" : "#F0F0F0"; // 绿 / 灰
    QString align = isSelf ? "right" : "left";
    QString html = QString("<div style='text-align:%1; margin:6px 8px;'>"
                          "<div style='display:inline-block; max-width:72%%; background:%2; border-radius:8px; padding:8px 10px; box-shadow:0 1px 2px rgba(0,0,0,.1);'>"
                          "<div style='color:#666;font-size:12px;margin-bottom:4px;'>%3 → %4 · %5</div>"
                          "<div style='white-space:pre-wrap; font-size:14px; color:#222;'>%6</div>"
                          "</div></div>")
                          .arg(align).arg(bubbleColor).arg(who).arg(toText).arg(ts).arg(content.toHtmlEscaped());
    auto *item = new QListWidgetItem();
    auto *lbl = new QLabel(html);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // 依据列表可视宽度计算合适的高度
    int vpw = ui->chatList->viewport()->width();
    int maxWidth = qMax(200, vpw - 24); // 预留内边距
    lbl->setMaximumWidth(maxWidth);
    lbl->setMinimumWidth(qMin(maxWidth, 420));
    QSize sz = lbl->sizeHint();
    item->setSizeHint(QSize(maxWidth, sz.height() + 12));
    ui->chatList->addItem(item);
    ui->chatList->setItemWidget(item, lbl);
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
        // 历史（用气泡）
        ui->chatList->clear();
        const QJsonArray arr = msg.value("messages").toArray();
        for (const auto &v : arr) {
            const auto o = v.toObject();
            const QString ts = o.value("createdAt").toString();
            const QString from = o.value("from").toString();
            const QString to = o.value("to").toString();
            const QString content = o.value("content").toString();
            bool isSelf = (!username.isEmpty() && from == username);
            appendBubble(from, to, content, ts, isSelf);
        }
        // 在线用户
        QStringList ons; for (const auto &v : msg.value("onlineUsers").toArray()) ons << v.toString();
        if (onlineLabel) onlineLabel->setText(tr("在线用户: ") + (ons.isEmpty()? tr("(无)"): ons.join(", ")));
        if (onlineCombo) {
            // 以当前 peerEdit 文本决定应选项：空=全体，否则选该用户
            QString desired = peerEdit ? peerEdit->text().trimmed() : QString();
            if (desired.isEmpty()) desired = QStringLiteral("全体");

            // 排序：全体 -> admin -> 其他
            bool hasAdmin = false;
            QStringList others;
            for (const auto &u : ons) {
                if (u == QLatin1String("admin")) { hasAdmin = true; continue; }
                others << u;
            }
            others.removeAll(QString());
            others.sort(Qt::CaseInsensitive);

            onlineCombo->blockSignals(true);
            onlineCombo->clear();
            onlineCombo->addItem(QStringLiteral("全体"));
            if (hasAdmin) onlineCombo->addItem(QStringLiteral("admin"));
            for (const auto &u : others) onlineCombo->addItem(u);
            // 选择与当前 peer 对应的项
            int idx = onlineCombo->findText(desired);
            if (idx >= 0) onlineCombo->setCurrentIndex(idx);
            onlineCombo->blockSignals(false);
        }
    } else if (type == QLatin1String("chat_message")) {
        const QString ts = msg.value("createdAt").toString();
        const QString from = msg.value("from").toString();
        const QString to = msg.value("to").toString();
        const QString content = msg.value("content").toString();
    appendBubble(from, to, content, ts, (!username.isEmpty() && from == username));
    } else if (type == QLatin1String("presence")) {
        // 简单提示
        const QString ev = msg.value("event").toString();
        const QString who = msg.value("username").toString();
        appendSystem(QString("[系统] 用户 %1 %2").arg(who).arg(ev == "online" ? tr("上线") : tr("离线")));
        // 触发重载在线列表
        initChat();
    } else if (type == QLatin1String("chat_delete_response")) {
        bool ok = msg.value("success").toBool();
        const int n = msg.value("deleted").toInt();
        if (ok) appendSystem(tr("[系统] 会话已清空，删除条目: ") + QString::number(n));
        else appendSystem(tr("[系统] 删除失败：") + msg.value("message").toString());
        initChat();
    }
}

void ChatWindow::on_clearButton_clicked() { sendDelete(); }