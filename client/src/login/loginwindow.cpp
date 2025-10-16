#include "login/loginwindow.h"
#include "ui_loginwindow.h"
#include "mainwindow/mainwindow.h"
#include "register/registerdialog.h"
#include <QTimer>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStatusBar>
#include <QByteArray>

LoginWindow::LoginWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LoginWindow),
    socket(nullptr)
{
    ui->setupUi(this);
    setupUi();
    initializeSocket();
    // 自动重连（指数退避，持续重连）
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]{ connectToServer(); });

    // 心跳：每 20s 发送一次 ping，防止空闲断开
    m_heartbeatTimer.setSingleShot(false);
    m_heartbeatTimer.setInterval(20000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &LoginWindow::sendHeartbeat);
    connectToServer();
}

LoginWindow::~LoginWindow()
{
    delete ui;
    if (socket) {
        socket->deleteLater();
    }
}

void LoginWindow::setupUi()
{
    // 使用标准窗口样式，避免透明背景导致样式表背景不可见
    // setWindowFlags(Qt::FramelessWindowHint);
    // setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(600, 560);
    resize(640, 620);

    // 在运行时动态插入一个标题，避免 UI 版本差异导致字段缺失
    if (auto central = this->centralWidget()) {
        // 隐藏可能存在的重复标题（UI 自动生成的 welcomeTitle），以释放垂直空间
        if (auto dup = central->findChild<QLabel*>(QStringLiteral("welcomeTitle"))) {
            dup->hide();
        }
        // 尝试获取中心布局
        if (auto v = qobject_cast<QVBoxLayout*>(central->layout())) {
            // 压缩整体间距，减少被挤压的风险
            v->setContentsMargins(16, 12, 16, 12);
            v->setSpacing(8);
            // 构建带图标 + 标题 + 副标题的头部容器
            QWidget *header = central->findChild<QWidget*>(QStringLiteral("welcomeHeader"));
            if (!header) {
                header = new QWidget(central);
                header->setObjectName(QStringLiteral("welcomeHeader"));
                QVBoxLayout *hv = new QVBoxLayout(header);
                hv->setContentsMargins(0, 0, 0, 8);
                hv->setSpacing(4);

                // 第一行：图标 + 标题
                QHBoxLayout *row = new QHBoxLayout();
                row->setContentsMargins(0, 0, 0, 0);
                row->setSpacing(8);
                QLabel *icon = new QLabel(header);
                icon->setObjectName(QStringLiteral("welcomeIcon"));
                icon->setFixedSize(32, 32);
                QIcon icn(QStringLiteral(":/icons/mall.svg"));
                icon->setPixmap(icn.pixmap(32, 32));
                icon->setScaledContents(true);

                QLabel *title = new QLabel(QString::fromUtf8("欢迎使用微商系统"), header);
                title->setObjectName(QStringLiteral("welcomeTitleFixed"));
                QFont f; f.setPointSize(28); f.setBold(true);
                title->setFont(f);
                title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                title->setStyleSheet(QString::fromUtf8("color:#ff7a00;font-size:28px;"));
                title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                title->setMaximumHeight(64);

                row->addStretch(1);
                row->addWidget(icon);
                row->addWidget(title);
                row->addStretch(1);
                hv->addLayout(row);

                // 第二行：副标题
                QLabel *subtitle = new QLabel(QString::fromUtf8("轻量电商演示系统"), header);
                subtitle->setObjectName(QStringLiteral("welcomeSubtitle"));
                QFont sf = subtitle->font(); sf.setPointSize(14); subtitle->setFont(sf);
                subtitle->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                subtitle->setStyleSheet(QString::fromUtf8("color:#666;margin:0 0 6px 0;"));
                hv->addWidget(subtitle);

                // 插入到布局最顶部
                v->insertWidget(0, header);
            } else {
                // 更新已存在的头部样式
                if (auto icon = header->findChild<QLabel*>(QStringLiteral("welcomeIcon"))) {
                    QIcon icn(QStringLiteral(":/icons/mall.svg"));
                    icon->setPixmap(icn.pixmap(32, 32));
                    icon->setFixedSize(32, 32);
                    icon->setScaledContents(true);
                }
                if (auto title = header->findChild<QLabel*>(QStringLiteral("welcomeTitleFixed"))) {
                    title->setText(QString::fromUtf8("欢迎使用微商系统"));
                    QFont f = title->font(); f.setPointSize(28); f.setBold(true); title->setFont(f);
                    title->setStyleSheet(QString::fromUtf8("color:#ff7a00;font-size:28px;"));
                    title->setMaximumHeight(64);
                    title->show();
                }
                if (auto subtitle = header->findChild<QLabel*>(QStringLiteral("welcomeSubtitle"))) {
                    subtitle->setText(QString::fromUtf8("轻量电商演示系统"));
                    QFont sf = subtitle->font(); sf.setPointSize(14); subtitle->setFont(sf);
                    subtitle->setStyleSheet(QString::fromUtf8("color:#666;margin:0 0 6px 0;"));
                    subtitle->show();
                }
                header->show();
            }
            // 如果 UI 中的 welcomeTitle 仍然可见，同步样式/隐藏，避免重复
            if (auto old = central->findChild<QLabel*>(QStringLiteral("welcomeTitle"))) {
                old->setStyleSheet(QString::fromUtf8("color:#ff7a00;font-size:28px;"));
                QFont fo = old->font(); fo.setPointSize(28); fo.setBold(true); old->setFont(fo);
                // 尽量隐藏旧的以防重叠
                old->hide();
            }
            // 尝试压缩 UI 中的两个 spacer
            if (ui->verticalSpacer) {
                ui->verticalLayout->removeItem(ui->verticalSpacer); // 先移除再以更小尺寸重新添加
                delete ui->verticalSpacer;
                ui->verticalSpacer = new QSpacerItem(0, 4, QSizePolicy::Minimum, QSizePolicy::Fixed);
                ui->verticalLayout->insertItem(1, ui->verticalSpacer);
            }
            // 直接移除尾部 spacer，避免把底部按钮挤出可视区域
            if (ui->verticalSpacer_2) {
                ui->verticalLayout->removeItem(ui->verticalSpacer_2);
                delete ui->verticalSpacer_2;
                ui->verticalSpacer_2 = nullptr;
            }
             // 确保背景色
             central->setStyleSheet(QString::fromUtf8("background-color:#F5F7FA;"));
 
             // 调整底部按钮行的对齐与间距
             if (auto h = central->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout"))) {
                // 隐藏原有按钮并收缩布局自身高度
                if (ui->loginButton) ui->loginButton->setVisible(false);
                if (ui->registerButton) ui->registerButton->setVisible(false);
                h->setContentsMargins(0, 0, 0, 0);
                h->setSpacing(0);
                h->setAlignment(Qt::AlignHCenter);
             }
 
             // 固定底部按钮栏，确保可见可点
             QWidget *bottom = central->findChild<QWidget*>(QStringLiteral("bottomBarFixed"));
             if (!bottom) {
                 bottom = new QWidget(central);
                 bottom->setObjectName(QStringLiteral("bottomBarFixed"));
                 QHBoxLayout *hb = new QHBoxLayout(bottom);
                 hb->setContentsMargins(0, 10, 0, 0);
                 hb->setSpacing(12);
                 hb->setAlignment(Qt::AlignHCenter);

                QPushButton *btnLogin = new QPushButton(QString::fromUtf8("登录"), bottom);
                btnLogin->setObjectName(QStringLiteral("loginButtonFixed"));
                btnLogin->setMinimumSize(100, 38);
                btnLogin->setStyleSheet(
                    "QPushButton{background:#1677ff;color:white;border:none;border-radius:6px;padding:6px 14px;}"
                    "QPushButton:hover{background:#3c8cff;}"
                    "QPushButton:pressed{background:#0e5ad1;}"
                );
                QObject::connect(btnLogin, &QPushButton::clicked, this, &LoginWindow::on_loginButton_clicked);

                QPushButton *btnReg = new QPushButton(QString::fromUtf8("注册"), bottom);
                btnReg->setObjectName(QStringLiteral("registerButtonFixed"));
                btnReg->setMinimumSize(100, 38);
                btnReg->setStyleSheet(
                    "QPushButton{background:transparent;color:#1677ff;border:1px solid #1677ff;border-radius:6px;padding:5px 13px;}"
                    "QPushButton:hover{background:rgba(22,119,255,0.06);}"
                    "QPushButton:pressed{background:rgba(22,119,255,0.12);}"
                );
                QObject::connect(btnReg, &QPushButton::clicked, this, &LoginWindow::on_registerButton_clicked);

                hb->addWidget(btnLogin);
                hb->addWidget(btnReg);

                // 放在布局末尾（尾部 spacer 已移除）
                v->addWidget(bottom, 0, Qt::AlignHCenter);
            }
        }
    }

    // 确保输入框最小高度
    if (ui->usernameInput) ui->usernameInput->setMinimumHeight(32);
    if (ui->passwordInput) ui->passwordInput->setMinimumHeight(32);

    // 刷新布局
    if (auto lay = centralWidget() ? centralWidget()->layout() : nullptr) {
        lay->invalidate();
        lay->update();
    }
    this->updateGeometry();

    // 原始按钮事件连接保留（即使被隐藏也无妨）
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginWindow::on_loginButton_clicked);
    connect(ui->registerButton, &QPushButton::clicked, this, &LoginWindow::on_registerButton_clicked);
}

void LoginWindow::initializeSocket()
{
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &LoginWindow::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &LoginWindow::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &LoginWindow::onReadyRead);
    connect(socket, qOverload<QAbstractSocket::SocketError>(&QTcpSocket::errorOccurred), this, &LoginWindow::onSocketError);

    // 尝试开启 TCP keep-alive（跨平台尽力而为，部分平台可能无效）
#ifdef Q_OS_WIN
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
#else
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
#endif
}

void LoginWindow::connectToServer()
{
    // 支持通过环境变量指定主机与端口，便于 StartAll 传参
    QString host = qEnvironmentVariable("APP_HOST");
    if (host.trimmed().isEmpty()) host = QStringLiteral("127.0.0.1");
    bool ok = false; int port = qEnvironmentVariableIntValue("APP_SOCKET_PORT", &ok);
    if (!ok || port <= 0) port = 8080;
    qInfo() << "Connecting to" << host << ":" << port;
    statusBar()->showMessage(QString::fromUtf8("正在连接 %1:%2 ...").arg(host).arg(port));
    socket->connectToHost(host, static_cast<quint16>(port));
}

void LoginWindow::on_loginButton_clicked()
{
    QString username = ui->usernameInput->text();
    QString password = ui->passwordInput->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "登录失败", "用户名和密码不能为空");
        return;
    }
    
    // 构造登录请求
    QJsonObject loginRequest;
    loginRequest["type"] = "login";
    loginRequest["username"] = username;
    loginRequest["password"] = password;
    
    sendJson(loginRequest);
}

void LoginWindow::on_registerButton_clicked()
{
    RegisterDialog dlg(this);
    // 当用户在对话框提交注册时，发送注册请求
    connect(&dlg, &RegisterDialog::submitRegister, this, [this](const QString& username, const QString& password, const QString& phone, const QString& email){
        QJsonObject regRequest;
        regRequest["type"] = "register";
        regRequest["username"] = username;
        regRequest["password"] = password;
        if (!phone.isEmpty()) regRequest["phone"] = phone;
        if (!email.isEmpty()) regRequest["email"] = email;
        sendJson(regRequest);
    });
    dlg.exec();
}

void LoginWindow::onConnected()
{
    qDebug() << "Connected to server";
    m_reconnectAttempts = 0;
    statusBar()->showMessage(QString::fromUtf8("已连接服务器"), 1500);
    // 启动心跳
    if (!m_heartbeatTimer.isActive()) m_heartbeatTimer.start();
    // 连接成功后，发送队列中积压的消息
    if (!m_pendingWrites.isEmpty()) {
        for (const auto &payload : std::as_const(m_pendingWrites)) {
            socket->write(payload);
        }
        m_pendingWrites.clear();
    }
}

void LoginWindow::onDisconnected()
{
    qDebug() << "Disconnected from server";
    statusBar()->showMessage(QString::fromUtf8("与服务器断开，准备重连..."), 2000);
    // 停止心跳
    if (m_heartbeatTimer.isActive()) m_heartbeatTimer.stop();
    scheduleReconnect();
}

void LoginWindow::onReadyRead()
{
    static QByteArray recvBuf; // 仅登录窗口阶段使用的局部静态缓冲
    recvBuf += socket->readAll();
    int idx;
    while ((idx = recvBuf.indexOf('\n')) != -1) {
        QByteArray one = recvBuf.left(idx);
        recvBuf.remove(0, idx + 1);
        const QByteArray trimmed = one.trimmed();
        if (trimmed.isEmpty()) continue;
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(trimmed, &err);
        if (err.error != QJsonParseError::NoError) continue;
        QJsonObject response = doc.object();
        const QString type = response.value("type").toString();
        if (type == "pong") {
            // 心跳响应
            continue;
        }
        if (type == "login_response") {
            if (response.value("success").toBool()) {
                qInfo() << "Login success, creating MainWindow and handing off socket";
                MainWindow *mainWindow = new MainWindow(nullptr);
                // 先断开 LoginWindow 上的 socket 信号连接，避免重复处理
                if (socket) {
                    socket->disconnect(this);
                    // 将 socket 的父对象转移到主窗口，避免关闭 LoginWindow 时销毁 socket
                    socket->setParent(mainWindow);
                }
                // 将当前用户名传递给主窗口
                const QString username = ui->usernameInput->text();
                mainWindow->setCurrentUsername(username);
                mainWindow->setSocket(socket);
                mainWindow->show();
                // 避免析构时 deleteLater，明确放弃所有权
                socket = nullptr;
                // 使用 hide 而不是 close；且暂不 deleteLater，避免任何意外导致应用退出
                this->hide();
            } else {
                // 统一提示文案：用户不存在或密码错误（不暴露更具体信息给终端用户）
                const QString serverMsg = response.value("message").toString();
                qWarning() << "Login failed:" << serverMsg;
                QMessageBox::warning(this, QString::fromUtf8("登录失败"), QString::fromUtf8("用户不存在或密码错误"));
            }
        } else if (type == "register_response") {
            bool ok = response.value("success").toBool();
            QString msg = response.value("message").toString();
            for (QObject* child : this->children()) {
                if (auto dlg = qobject_cast<RegisterDialog*>(child)) {
                    dlg->onRegisterResult(ok, msg);
                    ok = true; // 复用变量标记已处理
                    break;
                }
            }
            if (!ok) {
                // 如果未找到对话框，则弹窗提示
                if (response.value("success").toBool()) QMessageBox::information(this, "注册成功", msg.isEmpty() ? "注册成功" : msg);
                else QMessageBox::warning(this, "注册失败", msg.isEmpty() ? "请重试" : msg);
            }
        }
    }
}

void LoginWindow::sendJson(const QJsonObject& obj)
{
    QJsonDocument doc(obj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n'); // 使用换行作为消息分隔符
    qInfo() << "sendJson" << payload;
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->write(payload);
    } else {
        // 未连接则先连接，并将消息缓存，待 connected 后发送
        if (socket->state() == QAbstractSocket::UnconnectedState) {
            connectToServer();
        }
        m_pendingWrites.push_back(payload);
    }
}

void LoginWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    qCritical() << "Socket error:" << socket->errorString();
    statusBar()->showMessage(QString::fromUtf8("连接出错：%1").arg(socket->errorString()), 3000);
    scheduleReconnect();
}

void LoginWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 如果底部固定按钮栏存在但不在可视范围内，则适当增高窗口
    if (auto central = this->centralWidget()) {
        QWidget *bottom = central->findChild<QWidget*>(QStringLiteral("bottomBarFixed"));
        if (bottom) {
            QRect r = bottom->geometry();
            QRect visible = central->rect();
            if (!visible.contains(r, /*proper*/ true)) {
                // 增加高度，确保露出
                int h = this->height();
                this->resize(this->width(), qMax(h, 640));
            }
        }
    }
}

void LoginWindow::scheduleReconnect()
{
    if (!socket) return;
    if (socket->state() == QAbstractSocket::ConnectedState) return;
    // 持续重连（指数退避，最大 30s）
    int base = 500; // 起始 500ms
    int d = qMin(base << qMin(m_reconnectAttempts, 6), 30000); // 0.5s,1s,2s,4s,8s,16s,32s->封顶30s
    m_reconnectAttempts = qMin(m_reconnectAttempts + 1, 30);
    qInfo() << "Schedule reconnect attempt" << m_reconnectAttempts << "after" << d << "ms";
    m_reconnectTimer.start(d);
}

void LoginWindow::sendHeartbeat()
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    QJsonObject ping;
    ping["type"] = QStringLiteral("ping");
    sendJson(ping);
}