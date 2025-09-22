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
    qInfo() << "MainWindow constructed";
    setupUi();
    setupConnections();
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
    qInfo() << "MainWindow setSocket, state=" << socket->state();
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, [this]{
        qWarning() << "Socket disconnected";
        statusBar()->showMessage(tr("已断开与服务器的连接"), 5000);
    });
    connect(socket, qOverload<QAbstractSocket::SocketError>(&QTcpSocket::errorOccurred), this, [this](QAbstractSocket::SocketError){
        qCritical() << "Socket error in MainWindow:" << socket->errorString();
        statusBar()->showMessage(tr("网络错误：%1").arg(socket->errorString()), 5000);
    });
    // 登录后、socket 就绪再发首屏数据请求
    loadCarousel();
    loadRecommendations();
    loadPromotions();
    // 简单状态提示，确认已连通
    statusBar()->showMessage(tr("已连接服务器，正在加载数据…"), 3000);

    // 渲染占位，等待真实数据覆盖
    renderCarouselPlaceholder();
    renderRecommendationsPlaceholder();
    renderPromotionsPlaceholder();
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
        QJsonArray images = response["images"].toArray();
        renderCarousel(images);
    }
    else if (type == "recommendations") {
        QJsonArray products = response["products"].toArray();
        renderRecommendations(products);
    }
    else if (type == "promotions") {
        QJsonArray promotions = response["promotions"].toArray();
        renderPromotions(promotions);
    }
    else if (type == "search_results") {
        QJsonArray results = response["results"].toArray();
        renderSearchResults(results);
    }
    else if (type == "product_detail") {
        QJsonObject product = response["product"].toObject();
        showProductDetail(product);
    }
    else if (type == "add_to_cart_response") {
        bool ok = response.value("success").toBool();
        statusBar()->showMessage(ok ? tr("已加入购物车") : tr("加入购物车失败"), 3000);
    }
    }
}

// ---- Helpers ----
QLayout* MainWindow::ensureVBoxLayout(QWidget *area)
{
    if (!area) return nullptr;
    if (!area->layout()) {
        auto *v = new QVBoxLayout(area);
        v->setContentsMargins(8,8,8,8);
        v->setSpacing(6);
        area->setLayout(v);
        return v;
    }
    return area->layout();
}

void MainWindow::clearLayout(QLayout *layout)
{
    if (!layout) return;
    while (QLayoutItem *it = layout->takeAt(0)) {
        if (auto *w = it->widget()) w->deleteLater();
        delete it;
    }
}

void MainWindow::renderCarouselPlaceholder()
{
    auto *layout = ensureVBoxLayout(ui->carouselArea);
    clearLayout(layout);
    auto *lbl = new QLabel(tr("这里显示轮播图（占位）"), ui->carouselArea);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setMinimumHeight(180);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
}

void MainWindow::renderRecommendationsPlaceholder()
{
    auto *layout = ensureVBoxLayout(ui->recommendationsArea);
    clearLayout(layout);
    auto *lbl = new QLabel(tr("为你推荐（占位）"), ui->recommendationsArea);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    for (int i=0;i<4;++i) {
        auto *item = new QPushButton(tr("推荐商品 %1").arg(i+1), ui->recommendationsArea);
        item->setMinimumHeight(32);
        layout->addWidget(item);
    }
}

void MainWindow::renderPromotionsPlaceholder()
{
    auto *layout = ensureVBoxLayout(ui->promotionsArea);
    clearLayout(layout);
    auto *lbl = new QLabel(tr("优惠活动（占位）"), ui->promotionsArea);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    for (int i=0;i<2;++i) {
        auto *promo = new QLabel(tr("限时优惠 %1：全场 9.%1 折").arg(i+1), ui->promotionsArea);
        promo->setWordWrap(true);
        layout->addWidget(promo);
    }
}

void MainWindow::renderCarousel(const QJsonArray &images)
{
    auto *layout = ensureVBoxLayout(ui->carouselArea);
    clearLayout(layout);
    if (images.isEmpty()) {
        renderCarouselPlaceholder();
        return;
    }
    // 简单以列表形式展示图片 URL/标题
    for (auto v : images) {
        const auto o = v.toObject();
        auto *lbl = new QLabel(o.value("title").toString(QLatin1String("图片")) + ": " + o.value("url").toString(), ui->carouselArea);
        lbl->setWordWrap(true);
        layout->addWidget(lbl);
    }
}

void MainWindow::renderRecommendations(const QJsonArray &products)
{
    auto *layout = ensureVBoxLayout(ui->recommendationsArea);
    clearLayout(layout);
    if (products.isEmpty()) {
        renderRecommendationsPlaceholder();
        return;
    }
    for (auto v : products) {
        const auto o = v.toObject();
        int pid = o.value("product_id").toInt();
        QString name = o.value("name").toString(QLatin1String("商品"));
        double price = o.value("price").toDouble();
        auto *btn = new QPushButton(QString("%1  ￥%2").arg(name).arg(QString::number(price, 'f', 2)), ui->recommendationsArea);
        btn->setMinimumHeight(32);
        layout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
    }
}

void MainWindow::renderPromotions(const QJsonArray &promotions)
{
    auto *layout = ensureVBoxLayout(ui->promotionsArea);
    clearLayout(layout);
    if (promotions.isEmpty()) {
        renderPromotionsPlaceholder();
        return;
    }
    for (auto v : promotions) {
        const auto o = v.toObject();
        auto *lbl = new QLabel(o.value("title").toString(QLatin1String("活动")) + ": " + o.value("desc").toString(), ui->promotionsArea);
        lbl->setWordWrap(true);
        layout->addWidget(lbl);
    }
}

void MainWindow::renderSearchResults(const QJsonArray &results)
{
    // 复用推荐区域显示搜索结果
    auto *layout = ensureVBoxLayout(ui->recommendationsArea);
    clearLayout(layout);
    auto *title = new QLabel(tr("搜索结果"), ui->recommendationsArea);
    title->setWordWrap(true);
    layout->addWidget(title);
    if (results.isEmpty()) {
        layout->addWidget(new QLabel(tr("未找到相关商品"), ui->recommendationsArea));
        return;
    }
    for (auto v : results) {
        const auto o = v.toObject();
        int pid = o.value("product_id").toInt();
        QString name = o.value("name").toString(QLatin1String("商品"));
        double price = o.value("price").toDouble();
        auto *row = new QWidget(ui->recommendationsArea);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        auto *nameLbl = new QLabel(QString("%1  ￥%2").arg(name).arg(QString::number(price, 'f', 2)), row);
        auto *detailBtn = new QPushButton(tr("详情"), row);
        auto *addBtn = new QPushButton(tr("加入购物车"), row);
        h->addWidget(nameLbl, 1);
        h->addWidget(detailBtn);
        h->addWidget(addBtn);
        layout->addWidget(row);
        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
        connect(addBtn, &QPushButton::clicked, this, [this, pid]{ addToCart(pid); });
    }
}

void MainWindow::showProductDetail(const QJsonObject &product)
{
    int pid = product.value("product_id").toInt();
    QString name = product.value("name").toString();
    double price = product.value("price").toDouble();
    QMessageBox box(this);
    box.setWindowTitle(tr("商品详情"));
    box.setText(QString("%1\n价格：￥%2").arg(name).arg(QString::number(price, 'f', 2)));
    QPushButton *add = box.addButton(tr("加入购物车"), QMessageBox::AcceptRole);
    box.addButton(tr("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == add) {
        addToCart(pid);
    }
}

void MainWindow::addToCart(int productId)
{
    if (!socket) return;
    QJsonObject req;
    req["type"] = "add_to_cart";
    req["product_id"] = productId;
    QJsonDocument doc(req);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
}