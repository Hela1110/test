#include "loginwindow.h"
#include "ui_loginwindow.h"
#include "mainwindow/mainwindow.h"
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
}

void LoginWindow::connectToServer()
{
    socket->connectToHost("localhost", 8080);
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
    
    QJsonDocument doc(loginRequest);
    socket->write(doc.toJson());
}

void LoginWindow::onConnected()
{
    qDebug() << "Connected to server";
}

void LoginWindow::onDisconnected()
{
    qDebug() << "Disconnected from server";
}

void LoginWindow::onReadyRead()
{
    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject response = doc.object();
    
    if (response["type"].toString() == "login_response") {
        if (response["success"].toBool()) {
            // 登录成功，创建主窗口
            MainWindow *mainWindow = new MainWindow();
            mainWindow->setSocket(socket);
            mainWindow->show();
            socket = nullptr; // 转移socket所有权
            this->close();
        } else {
            QMessageBox::warning(this, "登录失败", response["message"].toString());
        }
    }
}