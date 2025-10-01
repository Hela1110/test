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
#include <QFontMetrics>
#include <QThread>
#include <QCoreApplication>

namespace {
// 统一规范服务器/历史中的时间戳为 yyyy-MM-dd HH:mm:ss
static QString normalizeTs(const QString &raw) {
    if (raw.isEmpty()) {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }
    // 优先解析 ISO8601，其次按常见格式兜底
    QDateTime dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid()) dt = QDateTime::fromString(raw, "yyyy-MM-dd HH:mm:ss");
    if (!dt.isValid()) dt = QDateTime::fromString(raw, "yyyy/M/d H:m:s");
    if (!dt.isValid()) dt = QDateTime::fromString(raw, "yyyy.MM.dd HH:mm:ss");
    if (!dt.isValid()) dt = QDateTime::currentDateTime();
    if (dt.timeSpec() == Qt::UTC) dt = dt.toLocalTime();
    return dt.toString("yyyy-MM-dd HH:mm:ss");
}
}

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ChatWindow) {
    ui->setupUi(this);
    // 追加在线用户标签与对等方输入框、清空按钮
    onlineLabel = new QLabel(tr("在线用户: (未知)"), this);
    onlineCombo = new QComboBox(this);
    onlineCombo->setEditable(false);
    onlineCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    peerEdit = new QLineEdit(this);
    // 隐藏该输入框：由下拉框 onlineCombo 负责切换对话对象
    peerEdit->setVisible(false);
    peerEdit->setEnabled(false);
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
    // 移除“清空会话”按钮，统一使用“删除与对方的历史”（带撤销）
    // 新增：删除与对方的历史（仅私聊有效）
    auto *delPeerBtn = new QPushButton(tr("删除与对方的历史"), this);
    auto *refreshBtn = new QPushButton(tr("刷新"), this);
    auto *sendOrderBtn = new QPushButton(tr("发送订单"), this);
    connect(delPeerBtn, &QPushButton::clicked, this, [this](){
        if (!socket) return;
        const QString to = peerEdit? peerEdit->text().trimmed() : QString();
        if (to.isEmpty() || to == QStringLiteral("全体")) {
            // 群聊删除：仅 admin 允许
            if (username == QLatin1String("admin")) {
                auto reply = QMessageBox::question(this, tr("确认删除"), tr("确定要清空公共聊天（全体）的历史记录吗？此操作可在 3 秒内撤销。"),
                                               QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
                if (reply != QMessageBox::Yes) return;
                pendingDeletePeer = QStringLiteral("__GLOBAL__");
            } else {
                appendSystem(tr("[系统] 请选择具体用户进行删除，群聊仅管理员支持该操作"));
                return;
            }
        } else {
            // 私聊删除
            auto reply = QMessageBox::question(this, tr("确认删除"), tr("确定要删除与 %1 的历史记录吗？此操作可在 3 秒内撤销。").arg(to),
                                           QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes) return;
            pendingDeletePeer = to;
        }
        // 设置待删除状态并显示撤销按钮，3 秒后真正发送删除请求
        if (!deleteTimer) {
            deleteTimer = new QTimer(this);
            deleteTimer->setSingleShot(true);
            connect(deleteTimer, &QTimer::timeout, this, [this](){
                if (!socket) return;
                if (pendingDeletePeer.isEmpty()) return; // 已撤销
                QJsonObject r; r["type"] = "chat_delete"; if (!username.isEmpty()) r["username"] = username; r["peer"] = pendingDeletePeer;
                QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                if (pendingDeletePeer == QLatin1String("__GLOBAL__")) appendSystem(tr("[系统] 正在清空公共聊天历史..."));
                else appendSystem(tr("[系统] 正在删除与 %1 的历史...").arg(pendingDeletePeer));
                pendingDeletePeer.clear();
                if (undoDeleteBtn) undoDeleteBtn->setVisible(false);
            });
        }
        deleteTimer->start(3000);
        if (!undoDeleteBtn) {
            undoDeleteBtn = new QPushButton(tr("撤销删除"), this);
            undoDeleteBtn->setToolTip(tr("在 3 秒内撤销删除操作"));
            ui->horizontalLayout->addWidget(undoDeleteBtn);
            connect(undoDeleteBtn, &QPushButton::clicked, this, [this](){
                if (deleteTimer && deleteTimer->isActive()) deleteTimer->stop();
                pendingDeletePeer.clear();
                if (undoDeleteBtn) undoDeleteBtn->setVisible(false);
                appendSystem(tr("[系统] 已撤销删除操作"));
            });
        }
        undoDeleteBtn->setVisible(true);
        if (pendingDeletePeer == QLatin1String("__GLOBAL__"))
            appendSystem(tr("[系统] 将在 3 秒后清空公共聊天历史，可点击‘撤销删除’取消。 "));
        else
            appendSystem(tr("[系统] 将在 3 秒后删除与 %1 的历史，可点击‘撤销删除’取消。 ").arg(to));
    });
    ui->horizontalLayout->addWidget(delPeerBtn);
    connect(refreshBtn, &QPushButton::clicked, this, [this](){ initChat(); });
    ui->horizontalLayout->addWidget(refreshBtn);
    // 发送订单（售后申请）
    connect(sendOrderBtn, &QPushButton::clicked, this, [this](){
        if (!socket) { appendSystem(tr("[系统] 未连接服务器")); return; }
        // 拉取当前用户订单，标记来源为 chat，便于区分展示
        QJsonObject r; r["type"] = "get_orders"; if (!username.isEmpty()) r["username"] = username; r["origin"] = "chat";
        QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
        appendSystem(tr("[系统] 正在载入你的订单列表…"));
    });
    ui->horizontalLayout->addWidget(sendOrderBtn);

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

    // 默认选中“全体”，确保进入页面即可群聊
    if (onlineCombo) onlineCombo->setCurrentIndex(0);
    if (peerEdit) peerEdit->setText("");
    initChat();
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
    // 不做本地回显，等待服务端推送，避免重复显示
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
    if (to.isEmpty() || to == QStringLiteral("全体")) {
        // 群聊清空仅 admin 可用
        if (username == QLatin1String("admin")) {
            auto reply = QMessageBox::question(this, tr("确认删除"), tr("确定要清空公共聊天（全体）的历史记录吗？"),
                                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes) return;
            QJsonObject r; r["type"] = "chat_delete"; if (!username.isEmpty()) r["username"] = username; r["peer"] = QStringLiteral("__GLOBAL__");
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            appendSystem(tr("[系统] 正在请求清空公共聊天历史..."));
        } else {
            appendSystem(tr("[系统] 请选择具体用户进行删除，群聊仅管理员支持该操作"));
        }
        return;
    }
    // 私聊删除
    {
        auto reply = QMessageBox::question(this, tr("确认删除"), tr("确定要删除与 %1 的历史记录吗？").arg(to),
                                          QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        QJsonObject r; r["type"] = "chat_delete"; if (!username.isEmpty()) r["username"] = username; r["peer"] = to;
        QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
        appendSystem(tr("[系统] 正在请求删除与 %1 的历史...").arg(to));
    }
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
    // 文本 + 简易 HTML 气泡（内置“售后申请”卡片渲染）
    QString who = isSelf ? tr("我") : from;
    QString toText = to.isEmpty()? tr("(群)") : to;
    QString bubbleColor = isSelf ? "#C8F7C5" : "#F0F0F0"; // 绿 / 灰
    QString align = isSelf ? "right" : "left";

    // 检测 refund_request JSON 并构造卡片内容
    bool isRefundCard = false; qint64 oid = 0; QString reason;
    {
        QJsonParseError perr{};
        QJsonDocument jd = QJsonDocument::fromJson(content.toUtf8(), &perr);
        if (perr.error == QJsonParseError::NoError && jd.isObject()) {
            auto o = jd.object();
            if (o.value("type").toString() == QLatin1String("refund_request")) {
                isRefundCard = true;
                oid = static_cast<qint64>(o.value("orderId").toDouble());
                reason = o.value("reason").toString();
            }
        }
    }

    QString contentHtml;
    if (isRefundCard) {
        // 卡片样式：标题 + 订单ID + 原因；保留原始 JSON 在 tooltip，便于复制
        QString title = QStringLiteral("🧾 %1").arg(tr("售后申请"));
        QString idLine = tr("订单ID: #%1").arg(oid);
        QString rsLine = tr("原因: %1").arg(reason.isEmpty()? tr("(未填写)") : reason.toHtmlEscaped());
        contentHtml = QString("<div style='border-left:4px solid #f39c12; padding:6px 10px; background:#fffbe6;'>"
                              "<div style='font-weight:600; color:#8a6d3b; margin-bottom:4px;'>%1</div>"
                              "<div style='color:#333; margin-bottom:2px;'>%2</div>"
                              "<div style='color:#555;'>%3</div>"
                              "</div>"
                              // 附带一行可见文本，方便服务端无法解析 JSON 时手工识别
                              "<div style='color:#999;font-size:12px;margin-top:4px;'>ORDER_ID=%4</div>")
                              .arg(title).arg(idLine).arg(rsLine).arg(oid);
    } else {
        contentHtml = content.toHtmlEscaped();
    }

    QString html = QString("<div style='text-align:%1; margin:6px 8px;'>"
                          "<div style='display:inline-block; max-width:72%%; background:%2; border-radius:8px; padding:8px 10px; box-shadow:0 1px 2px rgba(0,0,0,.1);'>"
                          "<div style='color:#666;font-size:12px;margin-bottom:4px;'>%3 → %4 · %5</div>"
                          "<div style='white-space:pre-wrap; font-size:14px; color:#222;'>%6</div>"
                          "</div></div>")
                          .arg(align).arg(bubbleColor).arg(who).arg(toText).arg(ts).arg(contentHtml);
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
    if (!ui) return;
    ui->chatList->clear();
    if (!socket) return;
    QJsonObject r; r["type"] = "chat_init"; if (!username.isEmpty()) r["username"] = username; const QString to = peerEdit? peerEdit->text().trimmed():QString(); if (!to.isEmpty()) r["peer"] = to; r["limit"] = 50;
    QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
}

void ChatWindow::handleMessage(const QJsonObject &msg) {
    // 如果当前线程不是 GUI 线程，则投递到 GUI 线程处理
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, msg]() { handleMessageUi(msg); }, Qt::QueuedConnection);
        return;
    }
    handleMessageUi(msg);
}

void ChatWindow::handleMessageUi(const QJsonObject &msg) {
    const QString type = msg.value("type").toString();
    if (type == QLatin1String("chat_init_response")) {
        // 历史（用气泡）
    if (!ui) return; ui->chatList->clear();
        const QJsonArray arr = msg.value("messages").toArray();
        for (const auto &v : arr) {
            const auto o = v.toObject();
            const QString ts = normalizeTs(o.value("createdAt").toString());
            const QString from = o.value("from").toString();
            const QString to = o.value("to").toString();
            const QString content = o.value("content").toString();
            bool isSelf = (!username.isEmpty() && from == username);
            appendBubble(from, to, content, ts, isSelf);
        }
        // 在线用户
        QStringList ons; for (const auto &v : msg.value("onlineUsers").toArray()) ons << v.toString();
        if (onlineLabel) {
            const QString fullText = tr("在线用户: ") + (ons.isEmpty()? tr("(无)") : ons.join(", "));
            QFontMetrics fm(onlineLabel->font());
            int maxW = onlineLabel->width();
            if (maxW < 80) maxW = 480; // 初次可能宽度尚未计算完，给个合理上限
            const QString elided = fm.elidedText(fullText, Qt::ElideRight, maxW);
            onlineLabel->setText(elided);
            onlineLabel->setToolTip(fullText);
        }
        if (onlineCombo) {
            // 以当前 peerEdit 文本决定应选项：空=全体，否则选该用户
            QString desired = peerEdit ? peerEdit->text().trimmed() : QString();
            if (desired.isEmpty()) desired = QStringLiteral("全体");

            // 排序：全体 -> admin(始终展示) -> 其他（在线）
            QStringList others;
            for (const auto &u : ons) {
                if (u == QLatin1String("admin")) continue; // admin 单独固定插入
                others << u;
            }
            others.removeAll(QString());
            others.sort(Qt::CaseInsensitive);

            onlineCombo->blockSignals(true);
            onlineCombo->clear();
            onlineCombo->addItem(QStringLiteral("全体"));
            // 始终展示 admin
            onlineCombo->addItem(QStringLiteral("admin"));
            for (const auto &u : others) onlineCombo->addItem(u);
            // 选择与当前 peer 对应的项
            int idx = onlineCombo->findText(desired);
            if (idx >= 0) onlineCombo->setCurrentIndex(idx);
            onlineCombo->blockSignals(false);
        }
    } else if (type == QLatin1String("chat_message")) {
        const QString ts = normalizeTs(msg.value("createdAt").toString());
        const QString from = msg.value("from").toString();
        const QString to = msg.value("to").toString();
        const QString content = msg.value("content").toString();
        // 仅显示当前会话相关消息：
    if (!ui) return; const QString currentPeer = peerEdit ? peerEdit->text().trimmed() : QString();
        bool show = false;
        if (currentPeer.isEmpty()) {
            // 群聊窗口只显示群消息（to 为空）
            show = to.isEmpty();
        } else {
            // 私聊只显示双方互发消息
            const bool iAm = !username.isEmpty();
            show = (iAm && from == username && to == currentPeer) || (iAm && from == currentPeer && to == username);
        }
        if (show) {
            appendBubble(from, to, content, ts, (!username.isEmpty() && from == username));
        }
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
    } else if (type == QLatin1String("orders_response")) {
        // 仅处理 origin=chat 的订单列表（聊天里的“发送订单”）
        const QString origin = msg.value("origin").toString();
        if (origin != QLatin1String("chat")) return;
        QJsonArray arr = msg.value("orders").toArray();
        // 过滤出已支付订单
        struct OrderRow { qint64 id; QString label; };
        QVector<OrderRow> list;
        for (const auto &v : arr) {
            const auto o = v.toObject();
            const QString status = o.value("status").toString();
            if (status != QLatin1String("PAID")) continue;
            const qint64 id = o.value("orderId").toVariant().toLongLong();
            const QString price = QString::number(o.value("total_price").toDouble(), 'f', 2);
            const QString time = o.value("order_time").toString();
            const QString label = tr("订单 #%1  金额 ￥%2  时间 %3").arg(id).arg(price).arg(time);
            list.push_back({id, label});
        }
        if (list.isEmpty()) {
            appendSystem(tr("[系统] 没有可申请售后的已支付订单"));
            return;
        }
        // 弹出简单选择框
        QStringList options; for (const auto &r : list) options << r.label;
        bool ok = false;
        QString chosen = QInputDialog::getItem(this, tr("选择订单"), tr("请选择要提交售后申请的订单"), options, 0, false, &ok);
        if (!ok || chosen.isEmpty()) return;
        qint64 orderId = -1;
        for (const auto &r : list) { if (r.label == chosen) { orderId = r.id; break; } }
        if (orderId <= 0) return;
    // 可选：填写原因（带预设下拉，可编辑）
    QStringList presets;
    presets << tr("尺码不合适")
        << tr("质量/瑕疵问题")
        << tr("发错货/漏发")
        << tr("七天无理由退货")
        << tr("拍错/不想要了")
        << tr("其他");
    bool okReason = false;
    QString reason = QInputDialog::getItem(this,
                           tr("申请原因(可选)"),
                           tr("请选择或填写退款原因"),
                           presets,
                           0, /* current */
                           true, /* editable */
                           &okReason);
    if (!okReason) reason.clear();
        // 组装 JSON 内容并发送给 admin
    QJsonObject content; content["type"] = "refund_request"; content["orderId"] = static_cast<double>(orderId); if (!reason.trimmed().isEmpty()) content["reason"] = reason.trimmed();
        QJsonDocument contentDoc(content);
        if (socket) {
            // 切换到与 admin 的会话，便于查看消息
            if (onlineCombo) {
                int idx = onlineCombo->findText(QStringLiteral("admin"));
                if (idx >= 0) onlineCombo->setCurrentIndex(idx);
            }
            if (peerEdit) peerEdit->setText(QStringLiteral("admin"));
            QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["to"] = QStringLiteral("admin"); r["content"] = QString::fromUtf8(contentDoc.toJson(QJsonDocument::Compact));
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            appendSystem(tr("[系统] 已提交售后申请：订单 #%1，等待客服处理…").arg(orderId));
        }
    }
}

// 已移除“清空会话”按钮