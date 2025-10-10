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
#include <QFileDialog>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QUrl>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QPainter>
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QRegularExpression>
#include <QDialog>
#include <QTextBrowser>
#include <functional>

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
    // 共享网络管理器用于图片下载
    http = new QNetworkAccessManager(this);
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
    // 切换下拉项 -> 切换会话对象（全体 = 群聊）并刷新
    connect(onlineCombo, &QComboBox::currentTextChanged, this, [this](const QString &t){
        if (peerEdit) {
            const QString val = t.trimmed();
            if (val == QStringLiteral("全体")) peerEdit->setText(QString());
            else peerEdit->setText(val);
        }
        initChat();
    });
    // 移除“清空会话”按钮，统一使用“删除与对方的历史”（带撤销）
    // 新增：删除与对方的历史（仅私聊有效）
    auto *delPeerBtn = new QPushButton(tr("删除与对方的历史"), this);
    auto *refreshBtn = new QPushButton(tr("刷新"), this);
    auto *sendOrderBtn = new QPushButton(tr("发送订单"), this);
    auto *sendImageBtn = new QPushButton(tr("发送图片"), this);
    // 将“发送图片/发送订单”按钮放到底部输入框右侧（紧挨“发送”按钮之前）
    if (ui && ui->horizontalLayout) {
        // 基础尺寸
        sendImageBtn->setMinimumWidth(88);
        sendOrderBtn->setMinimumWidth(88);
        // 插入到发送按钮之前
        int sendIdx = ui->horizontalLayout->indexOf(ui->sendButton);
        if (sendIdx < 0) {
            ui->horizontalLayout->addWidget(sendImageBtn);
            ui->horizontalLayout->addWidget(sendOrderBtn);
        } else {
            // 最终顺序：消息输入 | 发送图片 | 发送订单 | 发送
            ui->horizontalLayout->insertWidget(sendIdx, sendOrderBtn);
            ui->horizontalLayout->insertWidget(sendIdx, sendImageBtn);
        }
        // 绑定点击事件
        connect(sendImageBtn, &QPushButton::clicked, this, &ChatWindow::sendImage);
        connect(sendOrderBtn, &QPushButton::clicked, this, [this]() {
            if (!socket) { appendSystem(tr("[系统] 未连接服务器，无法获取订单")); return; }
            QJsonObject r; r["type"] = "get_orders"; r["origin"] = "chat";
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            appendSystem(tr("[系统] 正在获取可申请售后的订单…"));
        });
    }
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
    // 不将“删除与对方的历史”按钮放在底部输入栏，避免影响原有布局
    connect(refreshBtn, &QPushButton::clicked, this, [this](){ initChat(); });
}

// 将相对路径转成 http://localhost:8081/images/... 绝对地址
QString ChatWindow::toAbsoluteUrl(const QString &u) const {
    QString s = u.trimmed();
    if (s.isEmpty()) return s;
    if (s.startsWith("http://", Qt::CaseInsensitive) || s.startsWith("https://", Qt::CaseInsensitive)) return s;
    if (s.startsWith("/")) s = s.mid(1);
    if (!s.startsWith("images/", Qt::CaseInsensitive)) s = QStringLiteral("images/") + s;
    return QStringLiteral("http://localhost:8081/") + s;
}

// 图片渲染：下载 → 缩略（最大边 120）→ 放入一个容器 widget（缩略图 + 打开原图链接）
void ChatWindow::appendImageBubble(const QString &from, const QString &to, const QString &url, const QString &ts, bool isSelf) {
    QString who = isSelf ? tr("我") : from;
    QString toText = to.isEmpty()? tr("(群)") : to;
    // 根据全局样式推断是否为暗色主题
    const bool dark = qApp->palette().color(QPalette::Window).value() < 80 || qApp->styleSheet().contains("#1e1e1e");
    QString bubbleColor = isSelf ? (dark ? "#335d2f" : "#C8F7C5") : (dark ? "#2b2b2b" : "#F0F0F0");

    // 外层容器（文本头 + 缩略图 + 链接）
    auto *item = new QListWidgetItem();
    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container); v->setContentsMargins(0,0,0,0); v->setSpacing(0);
    auto *headLbl = new QLabel(QString("<div style='color:%1;font-size:12px;margin:4px 0;'>%2 → %3 · %4</div>")
                               .arg(dark? QStringLiteral("#aaaaaa"):QStringLiteral("#666"), who, toText, ts));
    headLbl->setTextFormat(Qt::RichText); v->addWidget(headLbl);
    auto *imgLbl = new QLabel(); imgLbl->setAlignment(Qt::AlignLeft|Qt::AlignTop); imgLbl->setMinimumSize(120,120); imgLbl->setMaximumSize(120,120); imgLbl->setScaledContents(true);
    auto *linkLbl = new QLabel(QString("<a href=\"%1\">%2</a>").arg(toAbsoluteUrl(url).toHtmlEscaped(), tr("打开原图")));
    linkLbl->setTextFormat(Qt::RichText); linkLbl->setTextInteractionFlags(Qt::TextBrowserInteraction); linkLbl->setOpenExternalLinks(true);
    auto *bubbleHost = new QWidget(); auto *bubbleLay = new QVBoxLayout(bubbleHost); bubbleLay->setContentsMargins(8,8,8,8); bubbleLay->setSpacing(4);
    bubbleHost->setStyleSheet(QString("background:%1; border-radius:8px; color:%2;")
                              .arg(bubbleColor, dark? QStringLiteral("#e8e8e8"):QStringLiteral("#000")));
    bubbleLay->addWidget(imgLbl); bubbleLay->addWidget(linkLbl);
    auto *alignHost = new QWidget(); auto *alignLay = new QHBoxLayout(alignHost); alignLay->setContentsMargins(8,6,8,6); alignLay->setSpacing(0);
    if (isSelf) { alignLay->addStretch(); alignLay->addWidget(bubbleHost,0,Qt::AlignRight|Qt::AlignTop);} else { alignLay->addWidget(bubbleHost,0,Qt::AlignLeft|Qt::AlignTop); alignLay->addStretch(); }
    v->addWidget(alignHost);
    int vpw = ui->chatList->viewport()->width(); int maxWidth = qMax(200, vpw - 24);
    container->setMaximumWidth(maxWidth); container->setMinimumWidth(qMin(maxWidth, 420)); item->setSizeHint(QSize(maxWidth, 120 + 36));
    ui->chatList->addItem(item); ui->chatList->setItemWidget(item, container); ui->chatList->scrollToBottom();

    // 工具：按最长边 120 缩放
    auto setThumb = [imgLbl](const QPixmap &px){ const int MAX=120; QSize s=px.size(); int w=s.width(), h=s.height(); double sc=qMin(1.0, qMin(double(MAX)/w,double(MAX)/h)); QPixmap sp=(sc<1.0)? px.scaled(int(w*sc), int(h*sc), Qt::KeepAspectRatio, Qt::SmoothTransformation):px; imgLbl->setMinimumSize(sp.size()); imgLbl->setMaximumSize(sp.size()); imgLbl->setPixmap(sp); };

    // 1) 本地路径：若本地可解码且不是自己发的，直接显示；否则上传→渲染 URL
    if (url.startsWith("file://", Qt::CaseInsensitive) || QDir::isAbsolutePath(url)) {
        QString fpath = url; if (fpath.startsWith("file://", Qt::CaseInsensitive)) fpath = QUrl(fpath).toLocalFile();
        QPixmap px; px.load(fpath);
        if (!px.isNull() && !isSelf) { setThumb(px); linkLbl->setText(QString("<a href=\"%1\">%2</a>").arg(QUrl::fromLocalFile(fpath).toString().toHtmlEscaped(), tr("打开原图"))); return; }
        QFileInfo fi(fpath);
        if (!fi.exists() || !fi.isFile()) {
            // 如果是 Windows 截图工具的临时路径，尝试自动改用剪贴板上传
            const QString low = fpath.toLower();
            if (low.contains("tempstate\\screenclip") || low.contains("tempstate/screenclip")) {
                appendSystem(tr("[系统] 该截图是临时文件，已尝试直接读取剪贴板并上传…"));
                if (pasteImageFromClipboard()) return; // 成功走剪贴板路径
            }
            imgLbl->setText(tr("[图片无效] 文件不存在（可能是临时文件）。请用‘发送图片’或直接粘贴图片。"));
            return;
        }
        appendSystem(tr("[系统] 正在上传图片…"));
        auto doPost = [this](const QString &base, const QString &path, const QFileInfo &fi, std::function<void(QNetworkReply*)> onReady){
            auto *m = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            QFile *f = new QFile(path, m); if (!f->open(QIODevice::ReadOnly)) { appendSystem(tr("[系统] 无法读取文件：") + path); m->deleteLater(); return; }
            QHttpPart part; QString mime = QMimeDatabase().mimeTypeForFile(fi).name(); if (!mime.startsWith("image/")) mime = "image/jpeg";
            part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime));
            part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
            part.setBodyDevice(f); m->append(part);
            QNetworkReply *r = http->post(QNetworkRequest{QUrl(base + "/api/upload/image")}, m); m->setParent(r);
            connect(r, &QNetworkReply::finished, this, [r, onReady](){ onReady(r); });
        };
        auto onOk = [this, setThumb, linkLbl, isSelf](const QString &finalUrl){ QUrl ru(finalUrl); QNetworkReply *r2 = http->get(QNetworkRequest{ru}); connect(r2, &QNetworkReply::finished, this, [this, setThumb, linkLbl, r2, ru, isSelf](){ r2->deleteLater(); if (r2->error()!=QNetworkReply::NoError) { linkLbl->setText(QString("<span style='color:#c00'>%1</span>").arg(r2->errorString().toHtmlEscaped())); return; } QByteArray d=r2->readAll(); QPixmap px; px.loadFromData(d); if (px.isNull()) { linkLbl->setText(tr("[图片无效]")); return; } setThumb(px); linkLbl->setText(QString("<a href=\"%1\">%2</a>").arg(ru.toString(QUrl::FullyEncoded).toHtmlEscaped(), tr("打开原图"))); if (isSelf && socket) { QJsonObject r; r["type"]= "chat_send"; if (!username.isEmpty()) r["username"]=username; r["content"]=ru.toString(QUrl::FullyEncoded); const QString toPeer = peerEdit? peerEdit->text().trimmed():QString(); if (!toPeer.isEmpty()) r["to"]=toPeer; QJsonDocument d2(r); QByteArray p=d2.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p); appendSystem(tr("[系统] 已补发图片URL，确保对端可见")); } }); };
        doPost(QStringLiteral("http://localhost:8081"), fpath, fi, [this, doPost, onOk, fpath, fi](QNetworkReply *r1){ if (r1->error()!=QNetworkReply::NoError) { r1->deleteLater(); doPost(QStringLiteral("http://localhost:8080"), fpath, fi, [this, onOk](QNetworkReply *r2){ if (r2->error()!=QNetworkReply::NoError) { QVariant sc=r2->attribute(QNetworkRequest::HttpStatusCodeAttribute); appendSystem(tr("[系统] 上传失败（端口回退后仍失败）：%1 (%2)").arg(r2->errorString(), sc.toString())); r2->deleteLater(); return; } QJsonParseError perr{}; QJsonDocument jd=QJsonDocument::fromJson(r2->readAll(), &perr); r2->deleteLater(); if (perr.error!=QJsonParseError::NoError || !jd.isObject()) { appendSystem(tr("[系统] 上传返回解析失败")); return; } QJsonObject o=jd.object(); if (!o.value("success").toBool()) { appendSystem(tr("[系统] 上传失败：") + o.value("message").toString()); return; } QString full=o.value("fullUrl").toString(); QString rel=o.value("url").toString(); QString finalUrl=!full.isEmpty()? full : toAbsoluteUrl(rel); if (finalUrl.isEmpty()) { appendSystem(tr("[系统] 上传成功但未返回URL")); return; } onOk(finalUrl); }); return; } QJsonParseError perr{}; QJsonDocument jd=QJsonDocument::fromJson(r1->readAll(), &perr); r1->deleteLater(); if (perr.error!=QJsonParseError::NoError || !jd.isObject()) { appendSystem(tr("[系统] 上传返回解析失败")); return; } QJsonObject o=jd.object(); if (!o.value("success").toBool()) { appendSystem(tr("[系统] 上传失败：") + o.value("message").toString()); return; } QString full=o.value("fullUrl").toString(); QString rel=o.value("url").toString(); QString finalUrl=!full.isEmpty()? full : toAbsoluteUrl(rel); if (finalUrl.isEmpty()) { appendSystem(tr("[系统] 上传成功但未返回URL")); return; } onOk(finalUrl); });
        return;
    }

    // 2) 远程 URL：下载显示，PNG 失败则尝试同名 .jpg
    auto tryDownload = [this, setThumb](const QUrl &u, bool isFallback){ QNetworkReply *reply = http->get(QNetworkRequest{u}); connect(reply, &QNetworkReply::finished, this, [this, setThumb, reply, u, isFallback](){ reply->deleteLater(); if (reply->error()!=QNetworkReply::NoError) { if (!isFallback) { const QString p=u.path(); if (p.endsWith(".png", Qt::CaseInsensitive)) { QUrl alt=u; QString np=p; np.chop(4); np += ".jpg"; alt.setPath(np); QMetaObject::invokeMethod(this, [this, setThumb, alt](){ QNetworkReply *r2=http->get(QNetworkRequest{alt}); connect(r2, &QNetworkReply::finished, this, [this, setThumb, r2](){ r2->deleteLater(); if (r2->error()!=QNetworkReply::NoError) return; QByteArray d=r2->readAll(); QPixmap px; px.loadFromData(d); if (px.isNull()) return; setThumb(px); }); }, Qt::QueuedConnection); return; } } return; } QByteArray data=reply->readAll(); QPixmap px; px.loadFromData(data); if (px.isNull()) { if (!isFallback) { const QString p=u.path(); if (p.endsWith(".png", Qt::CaseInsensitive)) { QUrl alt=u; QString np=p; np.chop(4); np += ".jpg"; alt.setPath(np); QMetaObject::invokeMethod(this, [this, setThumb, alt](){ QNetworkReply *r2=http->get(QNetworkRequest{alt}); connect(r2, &QNetworkReply::finished, this, [this, setThumb, r2](){ r2->deleteLater(); if (r2->error()!=QNetworkReply::NoError) return; QByteArray d=r2->readAll(); QPixmap px; px.loadFromData(d); if (px.isNull()) return; setThumb(px); }); }, Qt::QueuedConnection); return; } } return; } setThumb(px); }); };
    tryDownload(QUrl(toAbsoluteUrl(url)), false);
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
            // 优先识别结构化 JSON（订单卡片/售后卡片）
            auto renderText = [this,&from,&to,&content,&ts](bool isSelf){ appendBubble(from, to, content, ts, isSelf); };
            const bool isSelfMsg = (!username.isEmpty() && from == username);
            QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(content.toUtf8(), &perr);
            if (perr.error == QJsonParseError::NoError && jd.isObject()) {
                QJsonObject o = jd.object();
                const QString ctype = o.value("type").toString();
                if (ctype == QLatin1String("order_card")) { appendOrderBubble(from, to, o, ts, isSelfMsg); }
                else if (ctype == QLatin1String("refund_request")) { appendRefundBubble(from, to, o, ts, isSelfMsg); }
                else { renderText(isSelfMsg); }
            } else {
                // 容错：粗略匹配 refund_request / order_card
                QRegularExpression reRefund("\"type\"\\s*:\\s*\"refund_request\"", QRegularExpression::CaseInsensitiveOption);
                QRegularExpression reOrderCard("\"type\"\\s*:\\s*\"order_card\"", QRegularExpression::CaseInsensitiveOption);
                if (reRefund.match(content).hasMatch()) {
                    QJsonObject rr; rr["type"] = "refund_request";
                    QRegularExpression reId("\"orderId\"\\s*:\\s*(\\d+)"); auto m1 = reId.match(content); if (m1.hasMatch()) rr["orderId"] = m1.captured(1).toLongLong();
                    QRegularExpression reReason("\"reason\"\\s*:\\s*\"(.*?)\""); auto m2 = reReason.match(content); if (m2.hasMatch()) rr["reason"] = m2.captured(1);
                    appendRefundBubble(from, to, rr, ts, isSelfMsg);
                } else if (reOrderCard.match(content).hasMatch()) {
                    QJsonObject oc; oc["type"] = "order_card";
                    QRegularExpression reId("\"orderId\"\\s*:\\s*(\\d+)"); auto m1 = reId.match(content); if (m1.hasMatch()) oc["orderId"] = m1.captured(1).toLongLong();
                    appendOrderBubble(from, to, oc, ts, isSelfMsg);
                } else {
                    renderText(isSelfMsg);
                }
            }
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
        if (origin == QLatin1String("detail")) {
            // 从订单列表中找到 pendingDetailOrderId 并弹详情
            if (pendingDetailOrderId > 0) {
                for (const auto &v : msg.value("orders").toArray()) {
                    const auto o = v.toObject();
                    if (o.value("orderId").toVariant().toLongLong() == pendingDetailOrderId) {
                        showOrderDetailFromJson(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
                        pendingDetailOrderId = -1;
                        return;
                    }
                }
                appendSystem(tr("[系统] 未找到订单 #%1 的详情").arg(pendingDetailOrderId));
                pendingDetailOrderId = -1;
            }
            return;
        }
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
        // 组装订单卡片 JSON 并发送到当前会话（若未选择对端，则发送给 admin）
        QJsonObject card; card["type"] = "order_card"; card["orderId"] = static_cast<double>(orderId);
        // 附带最基本的展示字段
        card["summary"] = chosen; // 例如“订单 #123 金额 ￥xxx 时间 xxx”
        if (!reason.trimmed().isEmpty()) card["note"] = tr("售后申请: %1").arg(reason.trimmed());
        // 将完整订单对象一并附带，便于接收方双击查看详情
        QJsonObject fullOrder;
        for (const auto &v : arr) {
            const auto o = v.toObject();
            if (o.value("orderId").toVariant().toLongLong() == orderId) { fullOrder = o; break; }
        }
        if (!fullOrder.isEmpty()) card["orderJson"] = QString::fromUtf8(QJsonDocument(fullOrder).toJson(QJsonDocument::Compact));
        QJsonDocument cardDoc(card);
        if (socket) {
            QString dst = peerEdit? peerEdit->text().trimmed() : QString();
            if (dst.isEmpty()) dst = QStringLiteral("admin");
            QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["to"] = dst; r["content"] = QString::fromUtf8(cardDoc.toJson(QJsonDocument::Compact));
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            appendSystem(tr("[系统] 已发送订单卡片到 %1：#%2").arg(dst).arg(orderId));
        }
    }
}

ChatWindow::~ChatWindow() {
    delete ui;
}

bool ChatWindow::eventFilter(QObject *obj, QEvent *event) {
    // 订单卡片双击：打开详情
    if (event->type() == QEvent::MouseButtonDblClick) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && w->property("orderId").isValid()) {
            // 优先使用嵌入的 orderJson，否则仅提示订单号
            if (w->property("orderJson").isValid()) {
                showOrderDetailFromJson(w->property("orderJson").toString());
            } else {
                const qlonglong oid = w->property("orderId").toLongLong();
                // 改为在线拉取详情
                showOrderDetailById(oid);
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void ChatWindow::appendSystem(const QString &text) {
    if (!ui) return;
    auto *item = new QListWidgetItem();
    auto *lbl = new QLabel(QString("<div style='color:#888;'>%1</div>").arg(text.toHtmlEscaped()));
    lbl->setTextFormat(Qt::RichText);
    lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
    lbl->setOpenExternalLinks(true);
    lbl->setWordWrap(true);
    int vpw = ui->chatList->viewport()->width();
    int maxWidth = qMax(200, vpw - 24);
    lbl->setMaximumWidth(maxWidth);
    lbl->setMinimumWidth(qMin(maxWidth, 420));
    QSize sz = lbl->sizeHint();
    item->setSizeHint(QSize(maxWidth, sz.height() + 12));
    ui->chatList->addItem(item);
    ui->chatList->setItemWidget(item, lbl);
    ui->chatList->scrollToBottom();
}

void ChatWindow::appendBubble(const QString &from, const QString &to, const QString &content, const QString &ts, bool isSelf) {
    // 简单判断是否图片 URL
    const QString c = content.trimmed();
    const bool looksUrl = c.startsWith("http://", Qt::CaseInsensitive) || c.startsWith("https://", Qt::CaseInsensitive);
    const bool looksRel = c.startsWith("images/", Qt::CaseInsensitive) || c.startsWith("/images/", Qt::CaseInsensitive);
    const bool looksImg = c.endsWith(".png", Qt::CaseInsensitive) || c.endsWith(".jpg", Qt::CaseInsensitive) || c.endsWith(".jpeg", Qt::CaseInsensitive) || c.endsWith(".gif", Qt::CaseInsensitive) || c.endsWith(".webp", Qt::CaseInsensitive);
    const bool looksFile = c.startsWith("file://", Qt::CaseInsensitive) || QDir::isAbsolutePath(c);
    if ((looksUrl || looksRel || looksFile) && looksImg) {
        appendImageBubble(from, to, c, ts, isSelf);
        return;
    }

    // 识别结构化卡片（用于历史还原）：order_card / refund_request
    QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(c.toUtf8(), &perr);
    if (perr.error == QJsonParseError::NoError && jd.isObject()) {
        QJsonObject o = jd.object();
        const QString ctype = o.value("type").toString();
        if (ctype == QLatin1String("order_card")) { appendOrderBubble(from, to, o, ts, isSelf); return; }
        if (ctype == QLatin1String("refund_request")) { appendRefundBubble(from, to, o, ts, isSelf); return; }
    } else {
        QRegularExpression reRefund("\"type\"\\s*:\\s*\"refund_request\"", QRegularExpression::CaseInsensitiveOption);
        QRegularExpression reOrderCard("\"type\"\\s*:\\s*\"order_card\"", QRegularExpression::CaseInsensitiveOption);
        if (reRefund.match(c).hasMatch()) {
            QJsonObject rr; QRegularExpression reId("\"orderId\"\\s*:\\s*(\\d+)"); auto m1 = reId.match(c); if (m1.hasMatch()) rr["orderId"] = m1.captured(1).toLongLong(); rr["type"] = "refund_request"; appendRefundBubble(from, to, rr, ts, isSelf); return; }
        if (reOrderCard.match(c).hasMatch()) { QJsonObject oc; QRegularExpression reId("\"orderId\"\\s*:\\s*(\\d+)"); auto m1 = reId.match(c); if (m1.hasMatch()) oc["orderId"] = m1.captured(1).toLongLong(); oc["type"] = "order_card"; appendOrderBubble(from, to, oc, ts, isSelf); return; }
    }

    QString who = isSelf ? tr("我") : from;
    QString toText = to.isEmpty()? tr("(群)") : to;
    const bool dark = qApp->palette().color(QPalette::Window).value() < 80 || qApp->styleSheet().contains("#1e1e1e");
    QString bubbleColor = isSelf ? (dark ? "#335d2f" : "#C8F7C5") : (dark ? "#2b2b2b" : "#F0F0F0");

    // 构建气泡容器
    auto *item = new QListWidgetItem();
    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container); v->setContentsMargins(0,0,0,0); v->setSpacing(0);
    auto *headLbl = new QLabel(QString("<div style='color:%1;font-size:12px;margin:4px 0;'>%2 → %3 · %4</div>")
                               .arg(dark? QStringLiteral("#aaaaaa"):QStringLiteral("#666"), who, toText, ts));
    headLbl->setTextFormat(Qt::RichText); v->addWidget(headLbl);

    auto *textLbl = new QLabel();
    textLbl->setTextFormat(Qt::RichText);
    textLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
    textLbl->setOpenExternalLinks(true);
    textLbl->setWordWrap(true);
    // 强制长串也能换行显示完整
    QString safe = c.toHtmlEscaped(); safe.replace("\n", "<br/>");
    QString wrapped = QString("<div style='white-space:pre-wrap; word-break:break-all;'>%1</div>").arg(safe);
    textLbl->setText(wrapped);
    if (dark) textLbl->setStyleSheet("color:#e8e8e8;");

    auto *bubbleHost = new QWidget();
    auto *bubbleLay = new QVBoxLayout(bubbleHost); bubbleLay->setContentsMargins(8,8,8,8); bubbleLay->setSpacing(4);
    bubbleHost->setStyleSheet(QString("background:%1; border-radius:8px; color:%2;")
                              .arg(bubbleColor, dark? QStringLiteral("#e8e8e8"):QStringLiteral("#000")));
    bubbleLay->addWidget(textLbl);

    auto *alignHost = new QWidget();
    auto *alignLay = new QHBoxLayout(alignHost); alignLay->setContentsMargins(8,6,8,6); alignLay->setSpacing(0);
    if (isSelf) { alignLay->addStretch(); alignLay->addWidget(bubbleHost,0,Qt::AlignRight|Qt::AlignTop);} else { alignLay->addWidget(bubbleHost,0,Qt::AlignLeft|Qt::AlignTop); alignLay->addStretch(); }
    v->addWidget(alignHost);

    int vpw = ui->chatList->viewport()->width();
    int maxWidth = qMax(320, vpw - 24);
    // 仅设置最大宽度，保留自适应实际内容的最小宽度
    int cap = qMin(maxWidth - 16, 600);
    textLbl->setMaximumWidth(cap);
    bubbleHost->setMaximumWidth(cap + 16);
    container->setMaximumWidth(maxWidth);
    // 让布局根据内容自适应尺寸，再把 item 高度设置为 sizeHint
    container->adjustSize();
    item->setSizeHint(container->sizeHint());
    ui->chatList->addItem(item);
    ui->chatList->setItemWidget(item, container);
    ui->chatList->scrollToBottom();
}

void ChatWindow::sendMessage() {
    if (!ui) return;
    const QString raw = ui->messageInput->toPlainText().trimmed();
    if (raw.isEmpty()) return;
    // 若是本地图片路径则先上传
    if (trySendLocalImagePath(raw)) { ui->messageInput->clear(); return; }
    if (socket) {
        QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["content"] = raw;
        const QString to = peerEdit? peerEdit->text().trimmed() : QString();
        if (!to.isEmpty() && to != QStringLiteral("全体")) r["to"] = to;
        QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
    }
    ui->messageInput->clear();
}

void ChatWindow::on_sendButton_clicked() {
    sendMessage();
}

void ChatWindow::on_messageInput_returnPressed() {
    sendMessage();
}

// 已移除“清空会话”按钮

// 通过 HTTP 上传图片并发送为图片 URL 消息
void ChatWindow::sendImage() {
    // 选择文件
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择图片"), QString(), tr("图片 (*.png *.jpg *.jpeg *.gif *.webp)"));
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    // 端口回退上传：先试 8081，失败再试 8080
    appendSystem(tr("[系统] 正在上传图片…"));

    auto doPost = [this, &fi, filePath](const QString &base, std::function<void(QNetworkReply*)> onReady){
        // 为每次尝试构建全新的 multipart + QFile
        auto *m = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QFile *f = new QFile(filePath, m);
        if (!f->open(QIODevice::ReadOnly)) {
            appendSystem(tr("[系统] 无法读取文件：") + filePath);
            m->deleteLater();
            onReady(nullptr);
            return;
        }
        QHttpPart part; QString mime = QMimeDatabase().mimeTypeForFile(fi).name(); if (!mime.startsWith("image/")) mime = "image/jpeg";
        part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime));
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
        part.setBodyDevice(f); m->append(part);
        QNetworkReply *r = http->post(QNetworkRequest{QUrl(base + "/api/upload/image")}, m);
        m->setParent(r);
        connect(r, &QNetworkReply::finished, this, [r, onReady](){ onReady(r); });
    };

    auto onSuccess = [this](QNetworkReply *rep){
        QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(rep->readAll(), &perr);
        rep->deleteLater();
        if (perr.error != QJsonParseError::NoError || !jd.isObject()) { appendSystem(tr("[系统] 上传返回解析失败")); return; }
        QJsonObject o = jd.object(); if (!o.value("success").toBool()) { appendSystem(tr("[系统] 上传失败：") + o.value("message").toString()); return; }
        const QString full = o.value("fullUrl").toString();
        const QString rel = o.value("url").toString();
        const QString finalUrl = !full.isEmpty() ? full : (rel.isEmpty() ? QString() : toAbsoluteUrl(rel));
        if (finalUrl.isEmpty()) { appendSystem(tr("[系统] 上传成功但未返回URL")); return; }
        if (socket) {
            QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["content"] = finalUrl;
            const QString to = peerEdit? peerEdit->text().trimmed() : QString(); if (!to.isEmpty()) r["to"] = to;
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
        }
        appendSystem(tr("[系统] 图片已发送"));
    };

    doPost(QStringLiteral("http://localhost:8081"), [this, doPost, onSuccess, &fi, filePath](QNetworkReply *r1){
        if (!r1) return; // 打开失败
        if (r1->error() != QNetworkReply::NoError) {
            QVariant sc = r1->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            QVariant rs = r1->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
            appendSystem(tr("[系统] 上传失败：%1 (%2 %3)；尝试切换端口…").arg(r1->errorString(), sc.toString(), rs.toString()));
            r1->deleteLater();
            // 回退到 8080
            doPost(QStringLiteral("http://localhost:8080"), [this, onSuccess](QNetworkReply *r2){
                if (!r2) return;
                if (r2->error() != QNetworkReply::NoError) {
                    QVariant sc2 = r2->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                    appendSystem(tr("[系统] 上传失败（端口回退后仍失败）：%1 (%2)").arg(r2->errorString(), sc2.toString()));
                    r2->deleteLater();
                    return;
                }
                onSuccess(r2);
            });
            return;
        }
        onSuccess(r1);
    });
}

// 从剪贴板读取图片并上传发送；返回是否已处理
bool ChatWindow::pasteImageFromClipboard() {
    const QClipboard *cb = QGuiApplication::clipboard();
    if (!cb) return false;
    const QMimeData *md = cb->mimeData();
    if (!md) return false;
    QImage img;
    if (md->hasImage()) {
        img = qvariant_cast<QImage>(md->imageData());
    } else if (md->hasUrls()) {
        // 若剪贴板带有文件路径，尝试读取第一张图片文件
        const auto urls = md->urls();
        if (!urls.isEmpty()) {
            const QString path = urls.first().toLocalFile();
            if (!path.isEmpty()) img.load(path);
        }
    }
    if (img.isNull()) return false; // 非图片，交由默认粘贴流程

    // 将图片转为 JPEG，质量 90；避免客户端 PNG 插件依赖
    QByteArray jpg;
    jpg.reserve(256*1024);
    QBuffer buf(&jpg); buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPG", 90);
    buf.close();
    if (jpg.isEmpty()) { appendSystem(tr("[系统] 剪贴板图片转码失败")); return true; }

    // 端口回退上传（8081 → 8080）
    appendSystem(tr("[系统] 正在上传剪贴板图片…"));

    auto doPost = [this, &jpg](const QString &base, std::function<void(QNetworkReply*)> onReady){
        auto *m = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
        const QString fname = QString("pasted_%1.jpg").arg(QDateTime::currentMSecsSinceEpoch());
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fname)));
        part.setBody(jpg);
        m->append(part);
        QNetworkReply *r = http->post(QNetworkRequest{QUrl(base + "/api/upload/image")}, m);
        m->setParent(r);
        connect(r, &QNetworkReply::finished, this, [r, onReady](){ onReady(r); });
    };

    auto onSuccess = [this](QNetworkReply *rep){
        QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(rep->readAll(), &perr);
        rep->deleteLater();
        if (perr.error != QJsonParseError::NoError || !jd.isObject()) { appendSystem(tr("[系统] 上传返回解析失败")); return; }
        QJsonObject o = jd.object(); if (!o.value("success").toBool()) { appendSystem(tr("[系统] 上传失败：") + o.value("message").toString()); return; }
        const QString full = o.value("fullUrl").toString();
        const QString rel = o.value("url").toString();
        const QString finalUrl = !full.isEmpty() ? full : (rel.isEmpty() ? QString() : toAbsoluteUrl(rel));
        if (finalUrl.isEmpty()) { appendSystem(tr("[系统] 上传成功但未返回URL")); return; }
        if (socket) {
            QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["content"] = finalUrl;
            const QString to = peerEdit? peerEdit->text().trimmed() : QString(); if (!to.isEmpty()) r["to"] = to;
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
        }
        appendSystem(tr("[系统] 图片已发送"));
    };

    doPost(QStringLiteral("http://localhost:8081"), [this, doPost, onSuccess](QNetworkReply *r1){
        if (!r1) return;
        if (r1->error() != QNetworkReply::NoError) {
            QVariant sc = r1->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            QVariant rs = r1->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
            appendSystem(tr("[系统] 上传失败：%1 (%2 %3)；尝试切换端口…").arg(r1->errorString(), sc.toString(), rs.toString()));
            r1->deleteLater();
            doPost(QStringLiteral("http://localhost:8080"), [this, onSuccess](QNetworkReply *r2){
                if (!r2) return;
                if (r2->error() != QNetworkReply::NoError) {
                    QVariant sc2 = r2->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                    appendSystem(tr("[系统] 上传失败（端口回退后仍失败）：%1 (%2)").arg(r2->errorString(), sc2.toString()));
                    r2->deleteLater();
                    return;
                }
                onSuccess(r2);
            });
            return;
        }
        onSuccess(r1);
    });
    return true;
}

// 判断 message 是否是本地图片路径（绝对路径或 file://），如是则自动上传并把返回的 URL 通过 chat_send 发送。
bool ChatWindow::trySendLocalImagePath(const QString &message) {
    QString raw = message.trimmed();
    QString path;
    // 1) 显式 file:// 前缀
    if (raw.startsWith("file://", Qt::CaseInsensitive)) {
        QUrl u(raw);
        if (u.isLocalFile()) path = u.toLocalFile();
    }
    // 2) 先从整串中提取 file:// URL（支持任意位置、带%编码及大括号等）
    if (path.isEmpty()) {
        QRegularExpression reUrl("(file://[^\\s]+)", QRegularExpression::CaseInsensitiveOption);
        auto m = reUrl.match(raw);
        if (m.hasMatch()) {
            QUrl u(m.captured(1));
            if (u.isLocalFile()) path = u.toLocalFile();
        }
    }
    // 3) 提取 Windows 绝对路径（可能带引号/括号等）
    if (path.isEmpty()) {
        QString candidate = raw;
        // 去掉前后成对引号/尖括号
        if ((candidate.startsWith('"') && candidate.endsWith('"')) || (candidate.startsWith('\'') && candidate.endsWith('\'')) ||
            (candidate.startsWith('<') && candidate.endsWith('>'))) {
            candidate = candidate.mid(1, candidate.size()-2).trimmed();
        }
        // 若整串不是绝对路径，尝试用正则从中抓取第一个盘符路径（支持反斜杠或正斜杠分隔）
        if (!QDir::isAbsolutePath(candidate)) {
            // 反斜杠路径：C:\Users\...\file.png
            QRegularExpression reBack("([A-Za-z]:\\\\[^\\s\\\"<>]*\\.(png|jpg|jpeg|gif|webp))", QRegularExpression::CaseInsensitiveOption);
            auto m1 = reBack.match(raw);
            if (m1.hasMatch()) candidate = m1.captured(1);
            else {
                // 正斜杠路径：C:/Users/.../file.png
                QRegularExpression reFwd("([A-Za-z]:/[^\\s\\\"<>]*\\.(png|jpg|jpeg|gif|webp))", QRegularExpression::CaseInsensitiveOption);
                auto m2 = reFwd.match(raw);
                if (m2.hasMatch()) candidate = m2.captured(1);
            }
        }
        if (QDir::isAbsolutePath(candidate)) path = candidate;
    }
    // 仅处理存在的本地文件
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return false;
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        // Windows 截图临时路径：自动尝试剪贴板上传
        const QString low = path.toLower();
        if (low.contains("tempstate\\screenclip") || low.contains("tempstate/screenclip")) {
            appendSystem(tr("[系统] 检测到截图临时路径，尝试从剪贴板读取上传…"));
            if (pasteImageFromClipboard()) return true; // 已处理
        }
        return false;
    }
    // 判断是否为图片扩展名
    const QString low = fi.suffix().toLower();
    if (!(low == "png" || low == "jpg" || low == "jpeg" || low == "gif" || low == "webp")) return false;

    // 执行 HTTP 上传
    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QFile *file = new QFile(path, multi);
    if (!file->open(QIODevice::ReadOnly)) {
        appendSystem(tr("[系统] 无法读取文件：") + path);
        multi->deleteLater();
        return true; // 已拦截处理（失败）
    }
    QHttpPart filePart;
    QString mime = QMimeDatabase().mimeTypeForFile(fi).name();
    if (!mime.startsWith("image/")) mime = "image/jpeg";
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime));
    QString disp = QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName());
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, disp);
    filePart.setBodyDevice(file);
    multi->append(filePart);

    appendSystem(tr("[系统] 正在上传本地图片…"));
    auto doPost = [this](const QString &base, QHttpMultiPart *m, std::function<void(QNetworkReply*)> onReady){
        QNetworkReply *r = http->post(QNetworkRequest{QUrl(base + "/api/upload/image")}, m);
        m->setParent(r);
        connect(r, &QNetworkReply::finished, this, [r, onReady]() { onReady(r); });
    };
    auto handle = [this](QNetworkReply *rep, std::function<void()> onFailTryNext){
        auto finish = [this, rep]() {
            QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(rep->readAll(), &perr);
            rep->deleteLater();
            if (perr.error != QJsonParseError::NoError || !jd.isObject()) { appendSystem(tr("[系统] 上传返回解析失败")); return; }
            QJsonObject o = jd.object(); if (!o.value("success").toBool()) { appendSystem(tr("[系统] 上传失败：") + o.value("message").toString()); return; }
            const QString full = o.value("fullUrl").toString();
            const QString rel = o.value("url").toString();
            const QString finalUrl = !full.isEmpty() ? full : (rel.isEmpty() ? QString() : toAbsoluteUrl(rel));
            if (finalUrl.isEmpty()) { appendSystem(tr("[系统] 上传成功但未返回URL")); return; }
            if (socket) {
                QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["content"] = finalUrl;
                const QString to = peerEdit? peerEdit->text().trimmed() : QString(); if (!to.isEmpty()) r["to"] = to;
                QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            }
            appendSystem(tr("[系统] 图片已发送"));
        };
        if (rep->error() != QNetworkReply::NoError) {
            // 打印更详细的错误并尝试下一个端口
            QVariant sc = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            QVariant rs = rep->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
            appendSystem(tr("[系统] 上传失败：%1 (%2 %3)").arg(rep->errorString(), sc.toString(), rs.toString()));
            rep->deleteLater();
            onFailTryNext();
            return;
        }
        finish();
    };
    // 第一次尝试 8081
    auto *m1 = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    {
        // 重新构造 part（multi 不能复用）
        QFile *file2 = new QFile(path, m1);
        if (!file2->open(QIODevice::ReadOnly)) { appendSystem(tr("[系统] 无法读取文件：") + path); m1->deleteLater(); return true; }
        QHttpPart part2; QString mime2 = QMimeDatabase().mimeTypeForFile(fi).name(); if (!mime2.startsWith("image/")) mime2 = "image/jpeg";
        part2.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime2));
        part2.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
        part2.setBodyDevice(file2); m1->append(part2);
    }
    doPost(QStringLiteral("http://localhost:8081"), m1, [this, doPost, handle, path, fi](QNetworkReply *r1){
        handle(r1, [this, doPost, path, fi]() {
            // 回退到 8080
            auto *m2 = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            QFile *file3 = new QFile(path, m2);
            if (!file3->open(QIODevice::ReadOnly)) { appendSystem(tr("[系统] 无法读取文件：") + path); m2->deleteLater(); return; }
            QHttpPart part3; QString mime3 = QMimeDatabase().mimeTypeForFile(fi).name(); if (!mime3.startsWith("image/")) mime3 = "image/jpeg";
            part3.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime3));
            part3.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
            part3.setBodyDevice(file3); m2->append(part3);
            doPost(QStringLiteral("http://localhost:8080"), m2, [this](QNetworkReply *r2){
                QVariant sc = r2->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                if (r2->error() != QNetworkReply::NoError) {
                    appendSystem(tr("[系统] 上传失败（端口回退后仍失败）：%1 (%2)").arg(r2->errorString(), sc.toString()));
                    r2->deleteLater();
                    return;
                }
                QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(r2->readAll(), &perr);
                r2->deleteLater();
                if (perr.error != QJsonParseError::NoError || !jd.isObject()) { appendSystem(tr("[系统] 上传返回解析失败")); return; }
                QJsonObject o = jd.object(); if (!o.value("success").toBool()) { appendSystem(tr("[系统] 上传失败：") + o.value("message").toString()); return; }
                const QString full = o.value("fullUrl").toString();
                const QString rel = o.value("url").toString();
                const QString finalUrl = !full.isEmpty() ? full : (rel.isEmpty() ? QString() : toAbsoluteUrl(rel));
                if (finalUrl.isEmpty()) { appendSystem(tr("[系统] 上传成功但未返回URL")); return; }
                if (socket) {
                    QJsonObject r; r["type"] = "chat_send"; if (!username.isEmpty()) r["username"] = username; r["content"] = finalUrl;
                    const QString to = peerEdit? peerEdit->text().trimmed() : QString(); if (!to.isEmpty()) r["to"] = to;
                    QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                }
                appendSystem(tr("[系统] 图片已发送"));
            });
        });
    });
    return true;
}

// 订单详情 HTML 生成（与 MainWindow 的小票风格保持一致的简化版）
QString ChatWindow::buildOrderDetailHtml(const QJsonObject &orderObj) const {
    const qlonglong orderId = static_cast<qlonglong>(orderObj.value("orderId").toDouble());
    const QString status = orderObj.value("status").toString();
    QString timeStr = orderObj.value("order_time").toString();
    if (!timeStr.isEmpty()) timeStr.replace('T', ' ');
    const QJsonArray items = orderObj.value("items").toArray();
    QString html;
    html += QString("<div style='font-weight:700;font-size:14px;margin-bottom:6px;'>订单 #%1</div>").arg(orderId);
    if (!timeStr.isEmpty()) html += QString("<div style='color:#666;'>时间：%1</div>").arg(timeStr);
    if (!status.isEmpty()) html += QString("<div style='color:#666;margin-bottom:6px;'>状态：%1</div>").arg(status);
    html += "<table style='width:100%;border-collapse:collapse;'>";
    html += "<tr><th style='text-align:left;border-bottom:1px solid #eee;padding:4px 0;'>商品</th>"
            "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>数量</th>"
            "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>单价</th>"
            "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>小计</th></tr>";
    double sumOriginal = 0.0, sumEffective = 0.0;
    for (const auto &iv : items) {
        const QJsonObject it = iv.toObject();
        const QString name = it.value("name").toString();
        const int qty = it.value("quantity").toInt();
        const double price = it.value("price").toDouble();
        const double listPrice = it.contains("listPrice") ? it.value("listPrice").toDouble() : price;
        const bool onSale = it.value("onSale").toBool();
        const double dprice = it.contains("discountPrice") ? it.value("discountPrice").toDouble() : 0.0;
        const bool hasDiscount = onSale && dprice>0.0 && dprice<listPrice;
        const double unit = hasDiscount ? dprice : price;
        const double sub = unit * qty;
        sumOriginal += listPrice * qty;
        sumEffective += sub;
        html += QString("<tr><td style='padding:4px 0;'>%1</td><td style='text-align:right;padding:4px 0;'>%2</td><td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%3</td><td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%4</td></tr>")
                    .arg(name.toHtmlEscaped()).arg(qty).arg(QString::number(unit,'f',2)).arg(QString::number(sub,'f',2));
    }
    if (items.isEmpty()) { sumOriginal = orderObj.value("total_price").toDouble(); sumEffective = sumOriginal; }
    const qint64 cents = static_cast<qint64>(qRound64(sumEffective * 100.0));
    const qint64 threshold = 20000, stepOff = 2000; // 每满200减20
    const qint64 times = (cents>0 ? (cents/threshold) : 0);
    const qint64 promoOff = times * stepOff;
    const qint64 finalPay = qMax<qint64>(0, cents - promoOff);
    html += QString("<tr><td colspan='4' style='border-top:1px solid #eee;padding-top:6px;text-align:right;'>");
    if (sumOriginal > sumEffective + 1e-6) html += QString("<div style='color:#999;text-decoration:line-through;'>原价合计：&yen;&nbsp;%1</div>").arg(QString::number(sumOriginal,'f',2));
    if (promoOff > 0) {
        html += QString("<div>商品折后：&yen;&nbsp;%1</div>").arg(QString::number(sumEffective,'f',2));
        html += QString("<div style='color:#43A047;'>满减：-&yen;&nbsp;%1</div>").arg(QString::number(promoOff/100.0,'f',2));
    }
    html += QString("<div style='font-weight:700;color:#E53935;'>应付：&yen;&nbsp;%1</div>").arg(QString::number(finalPay/100.0,'f',2));
    html += "</td></tr></table>";
    return html;
}

void ChatWindow::showOrderDetailFromJson(const QString &json) {
    QJsonParseError perr{}; QJsonDocument jd = QJsonDocument::fromJson(json.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !jd.isObject()) return;
    const QString html = buildOrderDetailHtml(jd.object());
    auto *dlg = new QDialog(this); dlg->setWindowTitle(tr("订单详情")); dlg->resize(520, 480);
    auto *lay = new QVBoxLayout(dlg);
    auto *view = new QTextBrowser(dlg); view->setOpenExternalLinks(true); view->setHtml(html);
    lay->addWidget(view);
    dlg->setLayout(lay); dlg->exec();
}

void ChatWindow::appendOrderBubble(const QString &from, const QString &to, const QJsonObject &orderObj, const QString &ts, bool isSelf) {
    const qlonglong orderId = static_cast<qlonglong>(orderObj.value("orderId").toDouble());
    const QString summary = orderObj.value("summary").toString();
    const QString note = orderObj.value("note").toString();
    QString who = isSelf ? tr("我") : from;
    QString toText = to.isEmpty()? tr("(群)") : to;
    // 暗色主题适配
    const bool dark = qApp->palette().color(QPalette::Window).value() < 80 || qApp->styleSheet().contains("#1e1e1e");
    QString bubbleColor = isSelf
            ? (dark ? QStringLiteral("#244a75") : QStringLiteral("#E3F2FD"))
            : (dark ? QStringLiteral("#2b2b2b") : QStringLiteral("#FFFBE6")); // 自己发的偏蓝，别人发的偏黄（暗色下统一偏深色）

    auto *item = new QListWidgetItem();
    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container); v->setContentsMargins(0,0,0,0); v->setSpacing(0);
    auto *headLbl = new QLabel(QString("<div style='color:%1;font-size:12px;margin:4px 0;'>%2 → %3 · %4</div>")
                               .arg(dark? QStringLiteral("#aaaaaa"):QStringLiteral("#666"), who, toText, ts));
    headLbl->setTextFormat(Qt::RichText); v->addWidget(headLbl);

    // 卡片体
    auto *card = new QWidget();
    card->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:10px;")
                        .arg(bubbleColor, dark? QStringLiteral("#444"):QStringLiteral("#e5e5e5")));
    auto *h = new QHBoxLayout(card); h->setContentsMargins(12,10,12,10); h->setSpacing(12);
    // 左侧 ICON（票据/订单）
    auto *icon = new QLabel(card);
    icon->setFixedSize(36,36);
    icon->setStyleSheet("background:#1890ff;color:#fff;border-radius:6px;font-weight:700;text-align:center;");
    icon->setAlignment(Qt::AlignCenter);
    icon->setText("🧾");
    // 文本
    auto *textBox = new QWidget(card);
    auto *tv = new QVBoxLayout(textBox); tv->setContentsMargins(0,0,0,0); tv->setSpacing(4);
    auto *title = new QLabel(QString("订单 #%1").arg(orderId), card);
    title->setStyleSheet(QString("font-weight:600;%1").arg(dark? QStringLiteral("color:#e8e8e8;"):QString()));
    auto *summaryLbl = new QLabel(summary.isEmpty()? tr("订单摘要") : summary, card);
    summaryLbl->setWordWrap(true);
    if (dark) summaryLbl->setStyleSheet("color:#e8e8e8;");
    if (!note.isEmpty()) {
        auto *noteLbl = new QLabel(note, card);
        noteLbl->setStyleSheet(dark? "color:#e0b76a;" : "color:#d48806;");
        noteLbl->setWordWrap(true);
        tv->addWidget(noteLbl);
    }
    tv->addWidget(title);
    tv->addWidget(summaryLbl);
    h->addWidget(icon);
    h->addWidget(textBox, 1);

    // 双击打开订单详情
    card->setProperty("orderId", orderId);
    // 如果消息 content 同时带有 orderJson，可直接展示更详细信息
    if (orderObj.contains("orderJson")) card->setProperty("orderJson", orderObj.value("orderJson").toString());
    card->installEventFilter(this);

    auto *alignHost = new QWidget(); auto *alignLay = new QHBoxLayout(alignHost); alignLay->setContentsMargins(8,6,8,6); alignLay->setSpacing(0);
    if (isSelf) { alignLay->addStretch(); alignLay->addWidget(card,0,Qt::AlignRight|Qt::AlignTop);} else { alignLay->addWidget(card,0,Qt::AlignLeft|Qt::AlignTop); alignLay->addStretch(); }
    v->addWidget(alignHost);

    int vpw = ui->chatList->viewport()->width(); int maxWidth = qMax(320, vpw - 24);
    card->setMaximumWidth(qMin(maxWidth - 16, 640));
    container->setMaximumWidth(maxWidth);
    container->adjustSize();
    item->setSizeHint(container->sizeHint());
    ui->chatList->addItem(item);
    ui->chatList->setItemWidget(item, container);
    ui->chatList->scrollToBottom();
}

void ChatWindow::appendRefundBubble(const QString &from, const QString &to, const QJsonObject &refundObj, const QString &ts, bool isSelf) {
    const qlonglong orderId = static_cast<qlonglong>(refundObj.value("orderId").toDouble());
    const QString reason = refundObj.value("reason").toString();
    QString who = isSelf ? tr("我") : from;
    QString toText = to.isEmpty()? tr("(群)") : to;
    // 暗色主题适配
    const bool dark = qApp->palette().color(QPalette::Window).value() < 80 || qApp->styleSheet().contains("#1e1e1e");
    QString bubbleColor = isSelf
            ? (dark ? QStringLiteral("#2b2b2b") : QStringLiteral("#FFF1F0"))
            : (dark ? QStringLiteral("#2b2b2b") : QStringLiteral("#FFFBE6")); // 暗色下统一偏深色

    auto *item = new QListWidgetItem();
    auto *container = new QWidget();
    auto *v = new QVBoxLayout(container); v->setContentsMargins(0,0,0,0); v->setSpacing(0);
    auto *headLbl = new QLabel(QString("<div style='color:%1;font-size:12px;margin:4px 0;'>%2 → %3 · %4</div>")
                               .arg(dark? QStringLiteral("#aaaaaa"):QStringLiteral("#666"), who, toText, ts));
    headLbl->setTextFormat(Qt::RichText); v->addWidget(headLbl);

    auto *card = new QWidget();
    card->setStyleSheet(QString("background:%1;border:1px dashed %2;border-radius:10px;")
                        .arg(bubbleColor, dark? QStringLiteral("#444"):QStringLiteral("#f5a9a9")));
    auto *h = new QHBoxLayout(card); h->setContentsMargins(12,10,12,10); h->setSpacing(12);
    auto *icon = new QLabel(card); icon->setFixedSize(36,36); icon->setAlignment(Qt::AlignCenter);
    icon->setText("💬"); icon->setStyleSheet("background:#ff7875;color:#fff;border-radius:6px;font-weight:700;");
    auto *textBox = new QWidget(card); auto *tv = new QVBoxLayout(textBox); tv->setContentsMargins(0,0,0,0); tv->setSpacing(4);
    auto *title = new QLabel(tr("售后申请 · 订单 #%1").arg(orderId), card);
    title->setStyleSheet(QString("font-weight:600;%1").arg(dark? QStringLiteral("color:#e8e8e8;"):QString()));
    auto *summaryLbl = new QLabel(reason.isEmpty()? tr("用户提交了售后申请") : tr("原因：%1").arg(reason), card);
    summaryLbl->setWordWrap(true);
    if (dark) summaryLbl->setStyleSheet("color:#e8e8e8;");
    tv->addWidget(title); tv->addWidget(summaryLbl);
    h->addWidget(icon); h->addWidget(textBox, 1);

    card->setProperty("orderId", orderId);
    if (refundObj.contains("orderJson")) card->setProperty("orderJson", refundObj.value("orderJson").toString());
    card->installEventFilter(this);

    auto *alignHost = new QWidget(); auto *alignLay = new QHBoxLayout(alignHost); alignLay->setContentsMargins(8,6,8,6); alignLay->setSpacing(0);
    if (isSelf) { alignLay->addStretch(); alignLay->addWidget(card,0,Qt::AlignRight|Qt::AlignTop);} else { alignLay->addWidget(card,0,Qt::AlignLeft|Qt::AlignTop); alignLay->addStretch(); }
    v->addWidget(alignHost);

    int vpw = ui->chatList->viewport()->width(); int maxWidth = qMax(320, vpw - 24);
    card->setMaximumWidth(qMin(maxWidth - 16, 640));
    container->setMaximumWidth(maxWidth);
    container->adjustSize();
    item->setSizeHint(container->sizeHint());
    ui->chatList->addItem(item);
    ui->chatList->setItemWidget(item, container);
    ui->chatList->scrollToBottom();
}

void ChatWindow::showOrderDetailById(qlonglong orderId) {
    if (!socket || orderId <= 0) return;
    pendingDetailOrderId = orderId;
    QJsonObject req; req["type"] = "get_orders"; req["origin"] = "detail";
    QJsonDocument d(req); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
    appendSystem(tr("[系统] 正在获取订单 #%1 的详情…").arg(orderId));
}

bool ChatWindow::isWaitingOrderDetail() const {
    return pendingDetailOrderId > 0;
}