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
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    // 连接信号槽
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
    connect(&dlg, &RegisterDialog::submitRegister, this, [this](const QString& username, const QString& password){
        QJsonObject regRequest;
        regRequest["type"] = "register";
        regRequest["username"] = username;
        regRequest["password"] = password;
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