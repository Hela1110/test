#include "login/loginwindow.h"
#include "ui_loginwindow.h"
#include "mainwindow/mainwindow.h"
#include "register/registerdialog.h"
#include <QTimer>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>

LoginWindow::LoginWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LoginWindow),
    socket(nullptr)
{
    ui->setupUi(this);
    setupUi();
    initializeSocket();
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
    setMinimumSize(600, 460);
    resize(640, 480);

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
            QLabel *title = central->findChild<QLabel*>(QStringLiteral("welcomeTitleFixed"));
            if (!title) {
                title = new QLabel(QString::fromUtf8("欢迎使用微商系统"), central);
                title->setObjectName(QStringLiteral("welcomeTitleFixed"));
                QFont f; f.setPointSize(18); f.setBold(true);
                title->setFont(f);
                title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                title->setStyleSheet(QString::fromUtf8("color:#1677ff;margin:8px 0 6px 0;"));
                title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                title->setMaximumHeight(40);
                // 插入到布局最顶部
                v->insertWidget(0, title);
            } else {
                title->setText(QString::fromUtf8("欢迎使用微商系统"));
                title->setMaximumHeight(40);
                title->show();
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
}

void LoginWindow::connectToServer()
{
    // 使用 127.0.0.1 避免 hosts 解析异常；必要时可改为服务端实际 IP
    qInfo() << "Connecting to 127.0.0.1:8080";
    socket->connectToHost("127.0.0.1", 8080);
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
    connect(&dlg, &RegisterDialog::submitRegister, this, [this](const QString& username, const QString& password, const QString& phone){
        QJsonObject regRequest;
        regRequest["type"] = "register";
        regRequest["username"] = username;
        regRequest["password"] = password;
        if (!phone.isEmpty()) regRequest["phone"] = phone;
        sendJson(regRequest);
    });
    dlg.exec();
}

void LoginWindow::onConnected()
{
    qDebug() << "Connected to server";
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
}

void LoginWindow::onReadyRead()
{
    QByteArray data = socket->readAll();
    qInfo() << "onReadyRead bytes=" << data.size();
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) continue;
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        QJsonObject response = doc.object();
        const QString type = response.value("type").toString();
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
                QMessageBox::warning(this, "登录失败", response.value("message").toString());
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
                this->resize(this->width(), qMax(h, 560));
            }
        }
    }
}