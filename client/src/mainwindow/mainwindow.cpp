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
#include <QGridLayout>
#include <QFrame>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QGraphicsOpacityEffect>

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
    // 首页展示推荐商品（按需返回几个即可）
    loadRecommendations();
    loadPromotions();
    // 简单状态提示，确认已连通
    statusBar()->showMessage(tr("已连接服务器，正在加载数据…"), 3000);

    // 渲染占位，等待真实数据覆盖
    renderCarouselPlaceholder();
    // 首页不再显示“推荐占位”，等待真实推荐返回
    // renderRecommendationsPlaceholder();
    renderPromotionsPlaceholder();

}

void MainWindow::setupUi()
{
    // 设置窗口基本属性
    setWindowTitle("购物系统");
    resize(1366, 900);
    
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
            if (!socket) return;
            QJsonObject req; req["type"] = "get_account_info";
            QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); socket->write(payload);
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
    // 调试日志：记录首页相关的返回
    if (type == QLatin1String("carousel_data")) {
        qInfo() << "recv carousel_data images=" << response.value("images").toArray().size();
    } else if (type == QLatin1String("recommendations")) {
        qInfo() << "recv recommendations products=" << response.value("products").toArray().size();
    } else if (type == QLatin1String("promotions")) {
        qInfo() << "recv promotions count=" << response.value("promotions").toArray().size();
    } else if (type == QLatin1String("products_response")) {
        qInfo() << "recv products_response total=" << response.value("total").toInt()
                << " page=" << currentPage;
    }
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
                // 新增：传递促销字段到渲染层
                if (o.contains("onSale")) item["onSale"] = o.value("onSale");
                if (o.contains("discountPrice")) item["discountPrice"] = o.value("discountPrice");
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
        // 账户信息：默认只读 + 修改按钮；点击修改后进入可编辑态（用户名、手机号），ID 不可改
        QDialog dlg(this); dlg.setWindowTitle(tr("账号信息")); QVBoxLayout v(&dlg);
    QFormLayout form; QLineEdit idEdit; QLineEdit user; QLineEdit phone; QLineEdit pwd;
        idEdit.setText(QString::number(response.value("clientId").toVariant().toLongLong())); idEdit.setReadOnly(true);
        user.setText(response.value("username").toString());
        phone.setText(response.value("phone").toString());
    pwd.setEchoMode(QLineEdit::Password);
        form.addRow(tr("账号ID"), &idEdit);
        form.addRow(tr("用户名"), &user);
        form.addRow(tr("手机号"), &phone);
    form.addRow(tr("新密码"), &pwd);
        v.addLayout(&form);

        // 初始只读
        auto setReadOnly = [&](bool ro){ user.setReadOnly(ro); phone.setReadOnly(ro); };
        setReadOnly(true);

        QHBoxLayout btns; QPushButton edit(tr("修改")); QPushButton save(tr("保存")); QPushButton cancel(tr("关闭"));
        save.setEnabled(false);
        btns.addStretch(1); btns.addWidget(&edit); btns.addWidget(&save); btns.addWidget(&cancel); v.addLayout(&btns);
        QObject::connect(&cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        QObject::connect(&edit, &QPushButton::clicked, &dlg, [&]{
            setReadOnly(false);
            save.setEnabled(true);
            edit.setEnabled(false);
            // 强调当前处于编辑状态
            save.setStyleSheet("background:#1677ff;color:white;");
            statusBar()->showMessage(tr("已进入可编辑模式，请修改后点击保存"), 2500);
        });
        QObject::connect(&save, &QPushButton::clicked, &dlg, [&]{
            QJsonObject r; r["type"] = "update_account_info";
            const QString newName = user.text().trimmed();
            const QString newPhone = phone.text().trimmed();
            const QString newPwd = pwd.text();
            if (!newName.isEmpty()) r["new_username"] = newName;
            if (!newPhone.isEmpty()) r["phone"] = newPhone;
            if (!newPwd.isEmpty()) r["password"] = newPwd;
            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
            // 回到只读态，避免误触；并提示正在保存
            setReadOnly(true);
            save.setEnabled(false);
            edit.setEnabled(true);
            save.setStyleSheet("");
            statusBar()->showMessage(tr("正在保存修改…"), 2000);
            dlg.accept();
        });
        dlg.exec();
    }
    else if (type == "update_account_response") {
        bool ok = response.value("success").toBool(); QString msg = response.value("message").toString();
        statusBar()->showMessage(ok ? (msg.isEmpty()? tr("保存成功"): msg) : (msg.isEmpty()? tr("保存失败"): msg), 3000);
        if (ok) {
            // 成功后刷新一次账号信息，确保 UI 与服务端一致
            if (socket) {
                QJsonObject req; req["type"] = "get_account_info";
                QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); socket->write(payload);
            }
        }
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
            {
                QString uname = o.value("username").toString();
                if (uname.isEmpty()) uname = currentUsername; // 回退为当前登录用户
                table.setItem(r, 1, new QTableWidgetItem(uname));
            }
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
            || t == QLatin1String("clear_cart_response")
            || t == QLatin1String("error")) {
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
    // 保存最近数据并标记当前区域
    lastRecommendations = products;
    showingList = false;
    // 使用网格矩形卡片展示
    auto *container = ui->recommendationsArea;
    auto *layout = ensureVBoxLayout(container);
    clearLayout(layout);
    if (products.isEmpty()) {
        renderRecommendationsPlaceholder();
        return;
    }
    // 创建网格（更紧凑）
    auto *grid = new QGridLayout();
    grid->setContentsMargins(4,4,4,4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    int available = container->width();
    int colCount = computeColumns(available);
    int idx = 0;
    for (auto v : products) {
        const auto o = v.toObject();
        int pid = o.value("product_id").toInt();
        QString name = o.value("name").toString(QLatin1String("商品"));
        double price = o.value("price").toDouble();
        int stock = o.value("stock").toInt(-1);

        // 卡片
        auto *card = new QFrame(container);
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet("QFrame{border:1px solid #ddd;border-radius:8px;background:#fff;} QLabel{color:#333;font-size:13px;}");
        card->setMinimumSize(180, 120);
        auto *vbox = new QVBoxLayout(card);
        vbox->setContentsMargins(8,8,8,8);
        vbox->setSpacing(4);

        // 右上角“售罄”徽标（仅当库存为0显示）
        if (stock == 0) {
            auto *badgeRow = new QHBoxLayout();
            badgeRow->setContentsMargins(0,0,0,0);
            badgeRow->addStretch(1);
            auto *badge = new QLabel(tr("售罄"), card);
            badge->setStyleSheet("QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            badgeRow->addWidget(badge, 0, Qt::AlignRight);
            vbox->addLayout(badgeRow);
        }

    auto *nameLbl = new QLabel(name, card);
    nameLbl->setStyleSheet("font-weight:600;");
    nameLbl->setWordWrap(true);
    // 避免编码差异引起的货币符号乱码，使用 Unicode 码点或替代字符
    QLabel *priceLbl = nullptr;
    QLabel *discountLbl = nullptr;
    bool onSale = o.value("onSale").toBool();
    double discount = o.value("discountPrice").toDouble(0.0);
    bool hasDiscount = onSale && discount > 0.0 && discount < price;
    if (hasDiscount) {
        priceLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
        priceLbl->setStyleSheet("color:#999;text-decoration:line-through;");
        discountLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(discount, 'f', 2)), card);
        discountLbl->setStyleSheet("color:#E53935;font-weight:700;");
    } else {
        priceLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
    }
    // 隐藏商品卡片上的库存显示，改为详情查看
        auto *btnRow = new QHBoxLayout();
    auto *detailBtn = new QPushButton(tr("详情"), card);
    auto *addBtn = new QPushButton(tr("加入购物车"), card);
    detailBtn->setMinimumSize(64, 28);
    addBtn->setMinimumSize(86, 28);
        if (stock == 0) {
            // 降低名称/价格不透明度
            auto *nmEff = new QGraphicsOpacityEffect(card); nmEff->setOpacity(0.6); nameLbl->setGraphicsEffect(nmEff);
            auto *prEff = new QGraphicsOpacityEffect(card); prEff->setOpacity(0.6); priceLbl->setGraphicsEffect(prEff);
            addBtn->setEnabled(false);
        }
        btnRow->addWidget(detailBtn);
        btnRow->addWidget(addBtn);
        btnRow->addStretch(1);

        vbox->addWidget(nameLbl);
    vbox->addWidget(priceLbl);
        if (discountLbl) vbox->addWidget(discountLbl);

        int r = idx / colCount;
        int c = idx % colCount;
        grid->addWidget(card, r, c);
        ++idx;

        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
        connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock); });
    }
    // QLayout 没有 addLayout，这里用一个容器承载网格再添加
    auto *gridHost = new QWidget(container);
    gridHost->setLayout(grid);
    layout->addWidget(gridHost);
    lastColumns = colCount;
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
    // 保存最近数据并标记当前区域
    lastResults = results;
    showingList = true;
    // 使用网格矩形卡片展示，复用推荐区域容器
    auto *container = ui->recommendationsArea;
    auto *layout = ensureVBoxLayout(container);
    clearLayout(layout);
    auto *title = new QLabel(tr("商品列表"), container);
    title->setWordWrap(true);
    title->setStyleSheet("font-weight:600;margin:6px 0;");
    layout->addWidget(title);
    if (results.isEmpty()) {
        layout->addWidget(new QLabel(tr("未找到相关商品"), container));
        return;
    }
    auto *grid = new QGridLayout();
    grid->setContentsMargins(4,4,4,4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    int available = container->width();
    int colCount = computeColumns(available);
    int idx = 0;
    for (auto v : results) {
        const auto o = v.toObject();
        int pid = o.value("product_id").toInt();
        QString name = o.value("name").toString(QLatin1String("商品"));
        double price = o.value("price").toDouble();
        int stock = o.value("stock").toInt(-1);

        auto *card = new QFrame(container);
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet("QFrame{border:1px solid #ddd;border-radius:8px;background:#fff;} QLabel{color:#333;font-size:13px;}");
        card->setMinimumSize(190, 130);
            auto *vbox = new QVBoxLayout(card);
        vbox->setContentsMargins(8,8,8,8);
        vbox->setSpacing(4);
        // 右上角“售罄”徽标（仅当库存为0显示）
        if (stock == 0) {
            auto *badgeRow = new QHBoxLayout();
            badgeRow->setContentsMargins(0,0,0,0);
            badgeRow->addStretch(1);
            auto *badge = new QLabel(tr("售罄"), card);
            badge->setStyleSheet("QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            badgeRow->addWidget(badge, 0, Qt::AlignRight);
            vbox->addLayout(badgeRow);
        }
        auto *nameLbl = new QLabel(name, card);
        nameLbl->setStyleSheet("font-weight:600;");
        nameLbl->setWordWrap(true);
        nameLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        nameLbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        {
            QFontMetrics fm(nameLbl->font());
            nameLbl->setFixedHeight(fm.lineSpacing()*2 + 4);
            nameLbl->setToolTip(name);
        }
    QLabel *priceLbl2 = nullptr;
    QLabel *discountLbl2 = nullptr;
    bool onSale2 = o.value("onSale").toBool();
    double discount2 = o.value("discountPrice").toDouble(0.0);
    bool hasDiscount2 = onSale2 && discount2 > 0.0 && discount2 < price;
    if (hasDiscount2) {
        priceLbl2 = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
        priceLbl2->setStyleSheet("color:#999;text-decoration:line-through;");
        discountLbl2 = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(discount2, 'f', 2)), card);
        discountLbl2->setStyleSheet("color:#E53935;font-weight:700;");
    } else {
        priceLbl2 = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
    }
        auto *btnRow = new QHBoxLayout();
    auto *detailBtn = new QPushButton(tr("详情"), card);
    auto *addBtn = new QPushButton(tr("加入购物车"), card);
    detailBtn->setMinimumSize(64, 28);
    addBtn->setMinimumSize(86, 28);
        if (stock == 0) {
            auto *nmEff = new QGraphicsOpacityEffect(card); nmEff->setOpacity(0.6); nameLbl->setGraphicsEffect(nmEff);
            auto *prEff = new QGraphicsOpacityEffect(card); prEff->setOpacity(0.6); priceLbl2->setGraphicsEffect(prEff);
            addBtn->setEnabled(false);
        }
        btnRow->addWidget(detailBtn);
        btnRow->addWidget(addBtn);
        btnRow->addStretch(1);
        vbox->addWidget(nameLbl);
    vbox->addWidget(priceLbl2);
        if (discountLbl2) vbox->addWidget(discountLbl2);

        int r = idx / colCount;
        int c = idx % colCount;
        grid->addWidget(card, r, c);
        ++idx;

        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
        connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock); });
    }
    auto *gridHost2 = new QWidget(container);
    gridHost2->setLayout(grid);
    layout->addWidget(gridHost2);
    lastColumns = colCount;
}

int MainWindow::computeColumns(int availableWidth) const
{
    if (availableWidth >= 1600) return 5;
    if (availableWidth >= 1366) return 4;
    return 3;
}

void MainWindow::reflowGrids()
{
    QWidget *container = ui->recommendationsArea; // 推荐区与商品列表复用此容器
    if (!container) return;
    int available = container->width();
    int col = computeColumns(available);
    if (col == lastColumns) return; // 未跨阈值，不重排
    // 根据当前展示内容重绘
    if (showingList) {
        if (!lastResults.isEmpty()) {
            renderSearchResults(lastResults);
        }
    } else {
        if (!lastRecommendations.isEmpty()) {
            renderRecommendations(lastRecommendations);
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    reflowGrids();
    // 让购物车窗口随主窗缩放并居中停靠
    if (cart && cart->isVisible()) {
        const int w = qMax(720, this->width() * 85 / 100);
        const int h = qMax(520, this->height() * 85 / 100);
        cart->resize(w, h);
        const QPoint center = this->geometry().center();
        QRect r = cart->frameGeometry();
        r.moveCenter(center);
        cart->move(r.topLeft());
    }
}

void MainWindow::showProductDetail(const QJsonObject &product)
{
    int pid = product.value("product_id").toInt();
    QString name = product.value("name").toString();
    double price = product.value("price").toDouble();
    int stock = product.value("stock").toInt(-1);
    QMessageBox box(this);
    // in showProductDetail, read discount and render both prices if applicable
    // before composing text
    bool onSaleD = product.value("onSale").toBool();
    double discountD = product.value("discountPrice").toDouble(0.0);
    bool hasDiscountD = onSaleD && discountD > 0.0 && discountD < price;
    QString priceLine;
    if (hasDiscountD) {
        priceLine = QString("<span style='color:#999;text-decoration:line-through;'>%1</span> <span style='color:#E53935;font-weight:700;'>%2</span>")
                    .arg(QStringLiteral("\x00A5%1").arg(QString::number(price, 'f', 2)))
                    .arg(QStringLiteral("\x00A5%1").arg(QString::number(discountD, 'f', 2)));
    } else {
        priceLine = QStringLiteral("\x00A5%1").arg(QString::number(price, 'f', 2));
    }
    QString text = QString("%1\n%2 %3")
                    .arg(name)
                    .arg(QStringLiteral("价格:"))
                    .arg(priceLine);
    if (stock >= 0) text += QString("\n库存：%1").arg(stock);
    // 库存为0显示醒目的售罄徽标
    bool soldOut = (stock == 0);
    if (soldOut) {
        // 使用富文本渲染一个红底白字圆角徽标
        text += QString("\n\n%1").arg("<span style='background:#E53935;color:#fff;border-radius:12px;padding:4px 10px;font-weight:600;'>售罄</span>");
    }
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    QPushButton *add = box.addButton(tr("加入购物车"), QMessageBox::AcceptRole);
    if (soldOut) add->setEnabled(false);
    box.addButton(tr("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (!soldOut && box.clickedButton() == add) {
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