#include "mainwindow/mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QDebug>
#include <QAbstractButton>
#include <QInputDialog>
#include <QSpinBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(nullptr)
    , cart(nullptr)
    , chat(nullptr)
{
    ui->setupUi(this);
    qInfo() << "MainWindow constructed";
    monotonic.start();
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
    // 首页以“商品列表第一页”为主，不再先渲染推荐，避免与第一页视觉冲突
    // loadRecommendations();
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
    connect(ui->searchButton, &QAbstractButton::clicked, this, &MainWindow::on_searchButton_clicked);
    // 支持在输入框回车直接搜索
    connect(ui->searchInput, &QLineEdit::returnPressed, this, &MainWindow::on_searchButton_clicked);
    
    qInfo() << "setupConnections: searchButton=" << static_cast<void*>(ui->searchButton)
             << "cartButton=" << static_cast<void*>(ui->cartButton)
             << "chatButton=" << static_cast<void*>(ui->chatButton);
    // 再用 findChild 兜底查找（防止 ui 成员名不一致）
    if (auto fb = findChild<QAbstractButton*>("cartButton")) {
        if (fb != ui->cartButton) {
            qInfo() << "setupConnections: findChild(cartButton) found different instance" << static_cast<void*>(fb);
        } else {
            qInfo() << "setupConnections: findChild(cartButton) matches ui->cartButton";
        }
    } else {
        qInfo() << "setupConnections: findChild(cartButton) not found";
    }

    // 连接购物车按钮信号
    if (ui->cartButton) {
        connect(ui->cartButton, &QAbstractButton::clicked, this, &MainWindow::on_cartButton_clicked);
        // 冗余日志连接，确保点击有痕迹
        connect(ui->cartButton, &QAbstractButton::clicked, this, [](){ qInfo() << "cartButton lambda clicked"; });
        connect(ui->cartButton, &QAbstractButton::pressed, this, [](){ qInfo() << "cartButton pressed"; });
        connect(ui->cartButton, &QAbstractButton::released, this, [](){ qInfo() << "cartButton released"; });
    }
    if (auto cb = findChild<QAbstractButton*>("cartButton")) {
        // 再挂一遍（不同实例或相同实例均可），仅用于日志与兜底
        connect(cb, &QAbstractButton::clicked, this, [](){ qInfo() << "cartButton(findChild) clicked"; });
    }
    
    // 连接客服按钮信号
    connect(ui->chatButton, &QAbstractButton::clicked, this, &MainWindow::on_chatButton_clicked);

    // 账号信息
    if (auto ab = findChild<QAbstractButton*>("accountButton")) {
        connect(ab, &QAbstractButton::clicked, this, [this]{
            // 获取账号信息
            if (!socket) return;
            QJsonObject req; req["type"] = "get_account_info";
            QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); socket->write(payload);
            // 简易对话框在收到 account_info 后展示，由 onReadyRead 处理
            statusBar()->showMessage(tr("正在获取账号信息…"), 2000);
        });
    }
    if (auto ob = findChild<QAbstractButton*>("ordersButton")) {
        connect(ob, &QAbstractButton::clicked, this, [this]{
            if (!socket) return;
            QJsonObject req; req["type"] = "get_orders"; QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); socket->write(payload);
            statusBar()->showMessage(tr("正在获取历史订单…"), 2000);
        });
    }

    // 分页按钮（顶部）—按需连接；仅保留底部分页条时隐藏
    // 顶部分页按钮已移除

    // 底部分页条：跳转按钮
    if (auto btn = findChild<QPushButton*>("gotoPageButton")) {
        connect(btn, &QPushButton::clicked, this, [this]{
            if (auto spin = findChild<QSpinBox*>("gotoPageSpin")) {
                int page = spin->value();
                int totalPages = (pageSize>0) ? ((totalProducts + pageSize - 1) / pageSize) : 1;
                if (page < 1) page = 1;
                if (page > totalPages) page = totalPages;
                requestProductsPage(page);
            }
        });
    }
    // 也支持直接改动 SpinBox 立即跳页（做一个小的防抖）
    if (auto spin = findChild<QSpinBox*>("gotoPageSpin")) {
        static qint64 lastJumpMs = 0;
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val){
            Q_UNUSED(val);
            static QElapsedTimer t; if (!t.isValid()) t.start();
            qint64 now = t.elapsed();
            static qint64 last = 0;
            if (now - last < 200) return;
            last = now;
            requestProductsPage(qMax(1, val));
        });
    }

    // 可选：如果 UI 暂无分页按钮，这里用键盘快捷键或在状态栏增加上一页/下一页动作
    // 初次加载：请求第 1 页商品列表
    requestProductsPage(1);
    // 可选：预取历史订单，后续可在 UI 中展示
    if (socket) {
        QJsonObject req; req["type"] = "get_orders"; QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); socket->write(payload);
    }
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
    // 300ms 防抖，避免双击/回车重复
    if (monotonic.elapsed() - lastSearchMs < 300) return;
    lastSearchMs = monotonic.elapsed();
    // 允许空关键词，表示列出全部；服务器会返回所有匹配项
    if (socket) {
        QJsonObject request;
        // 与文档对齐：使用 search_products；服务器仍兼容旧的 search
        request["type"] = "search_products";
        request["keyword"] = keyword;
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
        statusBar()->showMessage(keyword.isEmpty() ? tr("正在获取全部商品…") : tr("正在搜索：%1").arg(keyword), 2000);
    }
}

void MainWindow::on_cartButton_clicked()
{
    qInfo() << "cartButton clicked";
    if (!cart) {
        if (!socket) {
            QMessageBox::warning(this, "提示", "未连接服务器，无法打开购物车");
            qInfo() << "cartButton aborted: socket not connected";
            return;
        }
        qInfo() << "Creating ShoppingCart instance";
        cart = new ShoppingCart(socket, this);
        // 将当前用户名传递给购物车，用于后续请求自动携带
        if (!currentUsername.isEmpty()) {
            cart->setUsername(currentUsername);
        }
        qInfo() << "ShoppingCart instance created";
        // 返回按钮：隐藏购物车，回到主界面
        if (auto backBtn = cart->findChild<QPushButton*>("backButton")) {
            connect(backBtn, &QPushButton::clicked, cart, &QWidget::hide);
        }
    }
    qInfo() << "Showing ShoppingCart";
    cart->show();
    cart->raise();
    cart->activateWindow();
    qInfo() << "ShoppingCart shown";
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
    else if (type == "products_response") {
        // 将文档格式的 products 列表归一化为现有 renderSearchResults 接受的结构
        QJsonArray products = response.value("products").toArray();
        totalProducts = response.value("total").toInt();
        QJsonArray results;
        for (const auto &v : products) {
            const auto o = v.toObject();
            QJsonObject item;
            item["product_id"] = o.value("id").toInt();
            item["name"] = o.value("name").toString();
            item["price"] = o.value("price").toDouble();
            // 可选字段保留
            if (o.contains("description")) item["description"] = o.value("description");
            if (o.contains("imageUrl")) item["image_url"] = o.value("imageUrl");
            if (o.contains("stock")) item["stock"] = o.value("stock");
            results.append(item);
        }
        renderSearchResults(results);
        // 在状态栏显示分页信息
        int totalPages = (pageSize>0) ? ((totalProducts + pageSize - 1) / pageSize) : 1;
        statusBar()->showMessage(tr("商品分页：第 %1/%2 页（共 %3 条）").arg(currentPage).arg(totalPages).arg(totalProducts), 3000);
        // 更新底部分页条
        if (auto lbl = findChild<QLabel*>("pageInfoLabel")) {
            lbl->setText(tr("第 %1/%2 页").arg(currentPage).arg(totalPages));
        }
        if (auto lbl2 = findChild<QLabel*>("totalInfoLabel")) {
            lbl2->setText(tr("共 %1 条").arg(totalProducts));
        }
        if (auto spin = findChild<QSpinBox*>("gotoPageSpin")) {
            spin->setMaximum(totalPages > 0 ? totalPages : 1);
            spin->setValue(currentPage);
        }
    // top pager buttons removed; only bottom pager is active
    }
    else if (type == "account_info") {
        // 简易账号对话框
        QDialog dlg(this); dlg.setWindowTitle(tr("账号信息")); QVBoxLayout v(&dlg);
        QFormLayout form; QLineEdit user; QLineEdit phone; QLineEdit pwd; QLineEdit newUser;
        user.setText(response.value("username").toString()); user.setReadOnly(true);
        phone.setText(response.value("phone").toString());
        pwd.setEchoMode(QLineEdit::Password);
        form.addRow(tr("当前用户名"), &user); form.addRow(tr("新用户名"), &newUser); form.addRow(tr("手机号"), &phone); form.addRow(tr("新密码"), &pwd);
        v.addLayout(&form);
        QHBoxLayout btns; QPushButton ok(tr("保存")); QPushButton cancel(tr("取消")); btns.addStretch(1); btns.addWidget(&ok); btns.addWidget(&cancel); v.addLayout(&btns);
        connect(&cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(&ok, &QPushButton::clicked, &dlg, [&]{
            QJsonObject r; r["type"] = "update_account_info"; if (!phone.text().trimmed().isEmpty()) r["phone"] = phone.text().trimmed(); if (!pwd.text().isEmpty()) r["password"] = pwd.text(); if (!newUser.text().trimmed().isEmpty()) r["new_username"] = newUser.text().trimmed();
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            dlg.accept();
        });
        dlg.exec();
    }
    else if (type == "update_account_response") {
        bool ok = response.value("success").toBool(); QString msg = response.value("message").toString();
        statusBar()->showMessage(ok ? (msg.isEmpty()? tr("保存成功"): msg) : (msg.isEmpty()? tr("保存失败"): msg), 3000);
    }
    else if (type == "orders_response") {
        QJsonArray arr = response.value("orders").toArray();
        // 展示一个简单的订单列表对话框
        QDialog dlg(this); dlg.setWindowTitle(tr("历史订单")); QVBoxLayout v(&dlg);
        QTableWidget table; table.setColumnCount(5); QStringList headers; headers << tr("订单号") << tr("用户名") << tr("金额") << tr("状态") << tr("时间"); table.setHorizontalHeaderLabels(headers); table.horizontalHeader()->setStretchLastSection(true);
        table.setRowCount(arr.size());
        int r = 0; for (const auto &val : arr) {
            QJsonObject o = val.toObject();
            table.setItem(r, 0, new QTableWidgetItem(QString::number(o.value("orderId").toVariant().toLongLong())));
            table.setItem(r, 1, new QTableWidgetItem(o.value("username").toString()));
            table.setItem(r, 2, new QTableWidgetItem(QString::number(o.value("total_price").toDouble(), 'f', 2)));
            table.setItem(r, 3, new QTableWidgetItem(o.value("status").toString()));
            table.setItem(r, 4, new QTableWidgetItem(o.value("order_time").toString()));
            ++r;
        }
        v.addWidget(&table);
        QHBoxLayout btns; QPushButton close(tr("关闭")); btns.addStretch(1); btns.addWidget(&close); v.addLayout(&btns); QObject::connect(&close, &QPushButton::clicked, &dlg, &QDialog::accept);
        dlg.resize(720, 420);
        dlg.exec();
    }
    else if (type == "add_to_cart_response") {
        bool ok = response.value("success").toBool();
        if (ok) {
            statusBar()->showMessage(tr("已加入购物车"), 3000);
        } else {
            const int code = response.value("code").toInt();
            const QString msg = response.value("message").toString();
            QString friendly = tr("加入购物车失败");
            if (code == 2002) friendly = tr("库存不足");
            statusBar()->showMessage(friendly + (msg.isEmpty()? QString(): (" - " + msg)), 3000);
        }
        if (ok && cart) {
            cart->refreshCart();
        }
    }
    else if (type == "error") {
        const int code = response.value("code").toInt();
        const QString msg = response.value("message").toString();
        QString friendly;
        switch (code) {
            case 1001: friendly = tr("认证失败"); break;
            case 1002: friendly = tr("参数无效"); break;
            case 2001: friendly = tr("商品不存在"); break;
            case 2002: friendly = tr("库存不足"); break;
            case 3001: friendly = tr("购物车为空"); break;
            case 3002: friendly = tr("订单创建失败"); break;
            default: friendly = tr("发生错误");
        }
        statusBar()->showMessage(friendly + (msg.isEmpty() ? QString() : (" - " + msg)), 4000);
    }
    // 将购物车相关的响应转发到购物车窗口（若已创建）
    if (cart) {
        const QString t = response.value("type").toString();
        if (t == QLatin1String("cart_items")
            || t == QLatin1String("cart_response")
            || t == QLatin1String("checkout_response")
            || t == QLatin1String("order_response")
            || t == QLatin1String("clear_cart_response")) {
            cart->handleMessage(response);
        }
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
    auto *title = new QLabel(tr("商品列表"), ui->recommendationsArea);
    title->setWordWrap(true);
    title->setStyleSheet("font-weight:600;margin:6px 0;");
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
        h->setContentsMargins(6,4,6,4);
        h->setSpacing(8);
        row->setMinimumHeight(42);
    int stock = o.value("stock").toInt(-1);
    QString priceStock = QString("%1    ￥%2").arg(name).arg(QString::number(price, 'f', 2));
    if (stock >= 0) priceStock += QString("    库存：%1").arg(stock);
    auto *nameLbl = new QLabel(priceStock, row);
        nameLbl->setMinimumHeight(32);
        auto *detailBtn = new QPushButton(tr("详情"), row);
        auto *addBtn = new QPushButton(tr("加入购物车"), row);
        detailBtn->setMinimumSize(72, 32);
        addBtn->setMinimumSize(100, 32);
        h->addWidget(nameLbl, 1);
        h->addWidget(detailBtn);
        h->addWidget(addBtn);
        layout->addWidget(row);
        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
        connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock); });
    }
}

void MainWindow::showProductDetail(const QJsonObject &product)
{
    int pid = product.value("product_id").toInt();
    QString name = product.value("name").toString();
    double price = product.value("price").toDouble();
    int stock = product.value("stock").toInt(-1);
    QMessageBox box(this);
    box.setWindowTitle(tr("商品详情"));
    QString text = QString("%1\n价格：￥%2").arg(name).arg(QString::number(price, 'f', 2));
    if (stock >= 0) text += QString("\n库存：%1").arg(stock);
    box.setText(text);
    QPushButton *add = box.addButton(tr("加入购物车"), QMessageBox::AcceptRole);
    box.addButton(tr("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == add) {
        addToCart(pid, stock);
    }
}

void MainWindow::addToCart(int productId, int stock)
{
    // 200ms 防抖，避免按钮连点导致多次请求
    if (monotonic.elapsed() - lastAddMs < 200) return;
    lastAddMs = monotonic.elapsed();
    if (!socket) return;
    // 让用户选择数量（默认 1）
    bool ok = false;
    int maxQty = (stock >= 0 ? qMax(1, stock) : 999);
    if (stock == 0) {
        statusBar()->showMessage(tr("该商品暂无库存，无法加入购物车"), 3000);
        return;
    }
    int quantity = QInputDialog::getInt(this, tr("加入购物车"), tr("数量"), 1, 1, maxQty, 1, &ok);
    if (!ok) return;
    QJsonObject req;
    req["type"] = "add_to_cart";
    req["product_id"] = productId;
    req["quantity"] = quantity;
    if (!currentUsername.isEmpty()) req["username"] = currentUsername;
    QJsonDocument doc(req);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
}

void MainWindow::requestProductsPage(int page)
{
    if (!socket) return;
    if (page < 1) page = 1;
    currentPage = page;
    QJsonObject req;
    req["type"] = "get_products";
    req["page"] = currentPage;
    req["size"] = pageSize;
    QJsonDocument doc(req);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
}

void MainWindow::on_prevPage_clicked()
{
    if (currentPage > 1) {
        requestProductsPage(currentPage - 1);
    }
}

void MainWindow::on_nextPage_clicked()
{
    int totalPages = (pageSize>0) ? ((totalProducts + pageSize - 1) / pageSize) : 1;
    if (currentPage < totalPages) {
        requestProductsPage(currentPage + 1);
    }
}