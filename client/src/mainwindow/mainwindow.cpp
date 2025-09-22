#include "mainwindow/mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(nullptr)
    , cart(nullptr)
    , chat(nullptr)
{
    ui->setupUi(this);
    setupUi();
    setupConnections();
    loadCarousel();
    loadRecommendations();
    loadPromotions();
}

MainWindow::~MainWindow()
{
    delete ui;
    if (cart) {
        delete cart;
    }
    if (chat) {
        delete chat;
    }
}

void MainWindow::setSocket(QTcpSocket *s)
{
    socket = s;
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
}

void MainWindow::setupUi()
{
    // 设置窗口基本属性
    setWindowTitle("购物系统");
    resize(1024, 768);
    
    // 子窗口延迟创建，避免在 socket 未设置时构造依赖 socket 的窗口
    cart = nullptr;
    chat = nullptr;
}

void MainWindow::setupConnections()
{
    // 连接搜索按钮信号
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::on_searchButton_clicked);
    
    // 连接购物车按钮信号
    connect(ui->cartButton, &QPushButton::clicked, this, &MainWindow::on_cartButton_clicked);
    
    // 连接客服按钮信号
    connect(ui->chatButton, &QPushButton::clicked, this, &MainWindow::on_chatButton_clicked);
}

void MainWindow::loadCarousel()
{
    // TODO: 从服务器加载轮播图数据
    if (socket) {
        QJsonObject request;
        request["type"] = "get_carousel";
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
    }
}

void MainWindow::loadRecommendations()
{
    // TODO: 从服务器加载推荐商品
    if (socket) {
        QJsonObject request;
        request["type"] = "get_recommendations";
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
    }
}

void MainWindow::loadPromotions()
{
    // TODO: 从服务器加载促销信息
    if (socket) {
        QJsonObject request;
        request["type"] = "get_promotions";
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
    }
}

void MainWindow::on_searchButton_clicked()
{
    QString keyword = ui->searchInput->text().trimmed();
    if (keyword.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入搜索关键词");
        return;
    }

    // 发送搜索请求到服务器
    if (socket) {
        QJsonObject request;
        request["type"] = "search";
        request["keyword"] = keyword;
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
    }
}

void MainWindow::on_cartButton_clicked()
{
    if (!cart) {
        if (!socket) {
            QMessageBox::warning(this, "提示", "未连接服务器，无法打开购物车");
            return;
        }
        cart = new ShoppingCart(socket, this);
    }
    cart->show();
    cart->raise();
    cart->activateWindow();
}

void MainWindow::on_chatButton_clicked()
{
    if (!chat) {
        chat = new ChatWindow(this);
    }
    chat->show();
    chat->raise();
    chat->activateWindow();
}

void MainWindow::onProductClicked(int productId)
{
    // 发送获取商品详情请求
    if (socket) {
        QJsonObject request;
        request["type"] = "get_product_detail";
        request["product_id"] = productId;
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
    }
}

void MainWindow::onReadyRead()
{
    if (!socket) return;
    
    QByteArray data = socket->readAll();
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &line : lines) {
    if (line.trimmed().isEmpty()) continue;
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError) continue;
    QJsonObject response = doc.object();
    QString type = response["type"].toString();
    if (type == "carousel_data") {
        // 处理轮播图数据
        QJsonArray images = response["images"].toArray();
        // TODO: 更新轮播图UI
    }
    else if (type == "recommendations") {
        // 处理推荐商品数据
        QJsonArray products = response["products"].toArray();
        // TODO: 更新推荐商品UI
    }
    else if (type == "promotions") {
        // 处理促销信息
        QJsonArray promotions = response["promotions"].toArray();
        // TODO: 更新促销信息UI
    }
    else if (type == "search_results") {
        // 处理搜索结果
        QJsonArray results = response["results"].toArray();
        // TODO: 显示搜索结果
    }
    else if (type == "product_detail") {
        // 处理商品详情
        QJsonObject product = response["product"].toObject();
        // TODO: 显示商品详情
    }
    }
}