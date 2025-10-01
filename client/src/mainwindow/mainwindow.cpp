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
#include <QPropertyAnimation>
#include <QTabBar>
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QBuffer>
#include <QPixmap>
#include <QLabel>
#include <QTimer>
#include <QPointer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(nullptr)
    , cart(nullptr)
    , chat(nullptr)
{
    ui->setupUi(this);
    // 初始化网络图片加载器
    http = new QNetworkAccessManager(this);
    // 默认 HTTP 基址为空，等 socket 建立后从对端地址推断（例如 http://127.0.0.1:8081）
    httpBase.clear();
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

// 初始化窗口的自定义部分（与 Qt UI 生成的 ui->setupUi 区分开）
void MainWindow::setupUi()
{
    // 基本窗口属性
    setWindowTitle("购物系统");
    resize(1366, 900);

    // 子组件延迟创建
    cart = nullptr;
    chat = nullptr;

    // 左侧垂直 Tab 导航（首页/商城/购物车/历史订单/客服/个人中心）
    ensureSideTabBar();

    // 在问候语旁添加红色漂浮活动文案
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        // 查找问候标签所在的索引
        int greetIndex = -1;
        for (int i=0;i<root->count();++i) {
            if (root->itemAt(i)->widget() == ui->greetingLabel) { greetIndex = i; break; }
        }
        // 先从原布局中移除 greetingLabel 的项，避免重复持有
        if (greetIndex >= 0) {
            auto *item = root->takeAt(greetIndex);
            // item 中持有的是 greetingLabel，对象仍然存活，无需 delete
            delete item; item = nullptr;
        }
        // 构造一行：左侧问候，右侧活动文案
        auto *row = new QWidget(this);
        row->setObjectName("greetingRow");
        auto *hl = new QHBoxLayout(row); hl->setContentsMargins(0,0,0,0); hl->setSpacing(8);
        // 将原 greetingLabel 放入该行
        ui->greetingLabel->setParent(row);
        hl->addWidget(ui->greetingLabel, 0, Qt::AlignLeft);
        // 活动文案（红色加粗，不使用 Qt 不支持的 CSS 动画）
    auto *promo = new QLabel(row);
        promo->setObjectName("promoFloatingLabel");
    // 初始文本由定时器轮播填充
    promo->setText("");
        promo->setStyleSheet("color:#E53935;font-weight:700;");
        hl->addSpacing(8);
        hl->addWidget(promo, 0, Qt::AlignLeft);
        hl->addStretch(1);
        // 将新行插入原位置（若未知则放顶部）
        if (greetIndex >= 0) root->insertWidget(greetIndex, row);
        else root->insertWidget(0, row);

        // 初始化活动文案轮播
        promoMessages = QStringList{
            tr("商城内商品单次订单每满200减20"),
            tr("部分商品折扣中")
        };
        promoIndex = 0;
        if (!promoTimer) promoTimer = new QTimer(this);
        promoTimer->setInterval(2000);
        QObject::disconnect(promoTimer, nullptr, nullptr, nullptr);
        connect(promoTimer, &QTimer::timeout, this, [this]() {
            auto *label = findChild<QLabel*>("promoFloatingLabel");
            if (!label) return;
            if (promoMessages.isEmpty()) { label->clear(); return; }
            label->setText(promoMessages.at(promoIndex % promoMessages.size()));
            promoIndex = (promoIndex + 1) % qMax(1, promoMessages.size());
        });
        // 立即触发一次并启动
        if (auto label = findChild<QLabel*>("promoFloatingLabel")) {
            if (!promoMessages.isEmpty()) label->setText(promoMessages.first());
        }
        promoTimer->start();
    }
}

void MainWindow::setSocket(QTcpSocket *s)
{
    socket = s;
    qInfo() << "MainWindow setSocket, state=" << socket->state();
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, [this]{
        qWarning() << "Socket disconnected";
        statusBar()->showMessage(tr("网络连接已断开"), 4000);
    });
    // 通过 TCP 连接对端推断静态资源 HTTP 基址（同一台服务器通常同时提供 9090 套接字与 8081 HTTP）
    // 若对端是 127.0.0.1 或 ::1，则使用 localhost
    if (socket) {
        const QHostAddress addr = socket->peerAddress();
        QString host;
        if (addr.isNull()) {
            host = QStringLiteral("localhost");
        } else if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6) {
            host = QStringLiteral("localhost");
        } else {
            host = addr.toString();
        }
        httpBase = QStringLiteral("http://%1:8081").arg(host);
        qInfo() << "HTTP base resolved to" << httpBase;
    }
    // 刚设置好 socket 时，主动进入首页并加载内容
    showHomeView();
}

// 创建并插入左侧垂直 TabBar；如已存在则复用
QTabBar* MainWindow::ensureSideTabBar()
{
    if (auto exist = findChild<QTabBar*>("leftTabBar")) return exist;
    auto *tab = new QTabBar(this);
    tab->setObjectName("leftTabBar");
    tab->setExpanding(true);
    tab->setMovable(false);
    tab->setDrawBase(false);
    tab->setShape(QTabBar::RoundedWest);
    tab->setFocusPolicy(Qt::NoFocus);

    // 插入到左侧栏顶部
    if (auto left = ui->centralwidget->findChild<QVBoxLayout*>("leftSidebarLayout")) {
        left->insertWidget(0, tab);
    }

    // 添加 Tabs（带 key）
    struct TabDef { const char* key; const char* text; } defs[] = {
        {"home", "首页"}, {"mall", "发现好物"}, {"cart", "购物车"}, {"orders", "历史订单"}, {"chat", "客服/聊天"}, {"account", "个人中心"}
    };
    for (auto &d : defs) {
        int idx = tab->addTab(tr(d.text));
        tab->setTabData(idx, QString::fromLatin1(d.key));
    }

    // 简单样式
    tab->setStyleSheet(
        "QTabBar { margin: 6px 0; }"
        "QTabBar::tab { background:#f7f7f7; border:1px solid #ddd; border-left-width:3px; border-radius:6px;"
        " padding:8px 10px; margin:4px 6px; color:#333; min-width:24px; min-height:80px; text-align:center; }"
        "QTabBar::tab:selected { background:#ffffff; border-left-color:#1677ff; color:#1677ff; font-weight:600; }"
        "QTabBar::tab:hover { background:#fafafa; }"
    );

    connect(tab, &QTabBar::currentChanged, this, [this, tab](int idx){
        if (tabSwitching || idx < 0) return;
        const QString key = tab->tabData(idx).toString();
        tabSwitching = true;
        if (key == QLatin1String("home")) showHomeView();
        else if (key == QLatin1String("mall")) showMallView();
        else if (key == QLatin1String("cart")) showCartView();
        else if (key == QLatin1String("orders")) showOrdersView();
        else if (key == QLatin1String("chat")) showChatView();
        else if (key == QLatin1String("account")) showAccountView();
        tabSwitching = false;
    });
    tab->setCurrentIndex(0);
    return tab;
}

// 外部调用以同步 Tab 选中态，避免递归
void MainWindow::setTabActive(const QString &key)
{
    if (tabSwitching) return;
    auto *tab = ensureSideTabBar();
    if (!tab) return;
    for (int i = 0; i < tab->count(); ++i) {
        if (tab->tabData(i).toString() == key) {
            QSignalBlocker blocker(tab);
            tab->setCurrentIndex(i);
            break;
        }
    }
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

    // 购物车/客服按钮改走 Qt AutoConnect 的 on_* 槽，避免重复触发
    // 这里仅保留轻量日志，不绑定额外的 clicked 行为
    if (ui->cartButton) {
        connect(ui->cartButton, &QAbstractButton::pressed, this, [](){ qInfo() << "cartButton pressed"; });
        connect(ui->cartButton, &QAbstractButton::released, this, [](){ qInfo() << "cartButton released"; });
    }

    // 账号与订单按钮（内嵌页面）
    if (auto ab = findChild<QAbstractButton*>("accountButton")) {
        connect(ab, &QAbstractButton::clicked, this, [this]{ showAccountView(); });
    }
    if (auto ob = findChild<QAbstractButton*>("ordersButton")) {
        connect(ob, &QAbstractButton::clicked, this, [this]{ showOrdersView(); });
    }

    // 首页/进入商城 切换
    // 侧栏已改为 Tab 导航，隐藏旧按钮避免双导航
    if (auto home = findChild<QWidget*>("homeButton")) home->hide();
    if (auto mall = findChild<QWidget*>("enterMallButton")) mall->hide();
    if (auto ab = findChild<QWidget*>("accountButton")) ab->hide();
    if (auto ob = findChild<QWidget*>("ordersButton")) ob->hide();
    if (auto cb = findChild<QWidget*>("chatButton")) cb->hide();
    if (auto car = findChild<QWidget*>("cartButton")) car->hide();

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
    // 初次加载：作为首页，隐藏分页器（商城里再显示）并加载首页模块
    showHomeView();
    updateGreeting();
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
    // 购物车作为主窗内的“页面”展示
    if (!socket) {
        QMessageBox::warning(this, tr("提示"), tr("未连接服务器，无法打开购物车"));
        return;
    }
    showCartView();
}

void MainWindow::on_chatButton_clicked()
{
    showChatView();
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
            // 新增：销量字段（若后端提供）
            if (o.contains("sales")) item["sales"] = o.value("sales");
                // 新增：传递促销字段到渲染层
                if (o.contains("onSale")) item["onSale"] = o.value("onSale");
                if (o.contains("discountPrice")) item["discountPrice"] = o.value("discountPrice");
            // 新增：传递 isNew 字段以在商城卡片显示“新品”角标
            if (o.contains("isNew")) item["isNew"] = o.value("isNew");
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
        // 在 accountPage 中展示并可直接编辑保存
        QWidget *page = findChild<QWidget*>("accountPage");
        if (!page) {
            page = new QWidget(this);
            page->setObjectName("accountPage");
            auto *v = new QVBoxLayout(page);
            auto *topBar = new QHBoxLayout();
            auto *title = new QLabel(tr("个人中心"), page); title->setStyleSheet("font-weight:600;font-size:16px;");
            auto *back = new QPushButton(tr("返回"), page);
            auto *saveToggle = new QPushButton(tr("修改"), page); saveToggle->setObjectName("saveTopButton");
            // 统一按钮样式，避免看起来像透明
            saveToggle->setMinimumWidth(64);
            back->setMinimumWidth(64);
            topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(saveToggle); topBar->addWidget(back); v->addLayout(topBar);
            // 如果此前 showAccountView 放入了占位标签，这里优先移除占位
            if (auto vlyt = qobject_cast<QVBoxLayout*>(page->layout())) {
                for (int i = vlyt->count()-1; i>=0; --i) {
                    if (auto w = vlyt->itemAt(i)->widget()) {
                        if (qobject_cast<QLabel*>(w) && w->objectName().isEmpty()) { w->deleteLater(); vlyt->removeWidget(w); }
                    }
                }
            }
            auto *form = new QFormLayout();
            auto *idEdit = new QLineEdit(page); idEdit->setObjectName("acc_id"); idEdit->setReadOnly(true);
            auto *user = new QLineEdit(page); user->setObjectName("acc_user");
            auto *phone = new QLineEdit(page); phone->setObjectName("acc_phone");
            auto *email = new QLineEdit(page); email->setObjectName("acc_email");
            auto *pwd = new QLineEdit(page); pwd->setObjectName("acc_pwd"); pwd->setEchoMode(QLineEdit::Password);
            form->addRow(tr("账号ID"), idEdit);
            form->addRow(tr("用户名"), user);
            form->addRow(tr("手机号"), phone);
            form->addRow(tr("邮箱"), email);
            form->addRow(tr("新密码"), pwd);
            v->addLayout(form);
            // 默认禁用编辑，点击“修改”后启用并把按钮文案变为“保存”
            user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
            if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(page);
            // 行为
            connect(back, &QPushButton::clicked, this, [this, page]{ page->hide(); page->deleteLater(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });
            auto sendSave = [this, user, phone, email, pwd]{
                QJsonObject r; r["type"] = "update_account_info";
                const QString newName = user->text().trimmed();
                const QString newPhone = phone->text().trimmed();
                const QString newEmail = email->text().trimmed();
                const QString newPwd = pwd->text();
                if (!newName.isEmpty()) r["new_username"] = newName;
                if (!newPhone.isEmpty()) r["phone"] = newPhone;
                if (!newEmail.isEmpty()) r["email"] = newEmail;
                if (!newPwd.isEmpty()) r["password"] = newPwd;
                if (!currentUsername.isEmpty()) r["username"] = currentUsername;
                QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                statusBar()->showMessage(tr("正在保存修改…"), 2000);
            };
            // 单按钮切换：修改 <-> 保存
            connect(saveToggle, &QPushButton::clicked, this, [saveToggle, user, phone, email, pwd, sendSave]() mutable {
                if (saveToggle->text() == QObject::tr("修改")) {
                    user->setEnabled(true); phone->setEnabled(true); email->setEnabled(true); pwd->setEnabled(true);
                    user->setFocus();
                    saveToggle->setText(QObject::tr("保存"));
                } else {
                    sendSave();
                    user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
                    saveToggle->setText(QObject::tr("修改"));
                }
            });
        }
        // 如果页面已存在但尚未搭建表单（只有占位），则在此构建表单
        if (page && !page->findChild<QLineEdit*>("acc_user")) {
            if (auto v = qobject_cast<QVBoxLayout*>(page->layout())) {
                // 移除占位
                for (int i = v->count()-1; i>=0; --i) {
                    if (auto w = v->itemAt(i)->widget()) {
                        if (qobject_cast<QLabel*>(w) && w->objectName().isEmpty()) { w->deleteLater(); v->removeWidget(w); }
                    }
                }
                auto *form = new QFormLayout();
                auto *idEdit = new QLineEdit(page); idEdit->setObjectName("acc_id"); idEdit->setReadOnly(true);
                auto *user = new QLineEdit(page); user->setObjectName("acc_user");
                auto *phone = new QLineEdit(page); phone->setObjectName("acc_phone");
                auto *email = new QLineEdit(page); email->setObjectName("acc_email");
                auto *pwd = new QLineEdit(page); pwd->setObjectName("acc_pwd"); pwd->setEchoMode(QLineEdit::Password);
                form->addRow(tr("账号ID"), idEdit);
                form->addRow(tr("用户名"), user);
                form->addRow(tr("手机号"), phone);
                form->addRow(tr("邮箱"), email);
                form->addRow(tr("新密码"), pwd);
                v->addLayout(form);
                // 默认禁用编辑
                user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
                if (auto saveToggle = page->findChild<QPushButton*>("saveTopButton")) {
                    QObject::disconnect(saveToggle, nullptr, nullptr, nullptr);
                    connect(saveToggle, &QPushButton::clicked, this, [saveToggle, user, phone, email, pwd, this]() mutable {
                        auto sendSave = [this, user, phone, email, pwd]{
                            QJsonObject r; r["type"] = "update_account_info";
                            const QString newName = user->text().trimmed();
                            const QString newPhone = phone->text().trimmed();
                            const QString newEmail = email->text().trimmed();
                            const QString newPwd = pwd->text();
                            if (!newName.isEmpty()) r["new_username"] = newName;
                            if (!newPhone.isEmpty()) r["phone"] = newPhone;
                            if (!newEmail.isEmpty()) r["email"] = newEmail;
                            if (!newPwd.isEmpty()) r["password"] = newPwd;
                            if (!currentUsername.isEmpty()) r["username"] = currentUsername;
                            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                            statusBar()->showMessage(tr("正在保存修改…"), 2000);
                        };
                        if (saveToggle->text() == QObject::tr("修改")) {
                            user->setEnabled(true); phone->setEnabled(true); email->setEnabled(true); pwd->setEnabled(true);
                            user->setFocus();
                            saveToggle->setText(QObject::tr("保存"));
                        } else {
                            sendSave();
                            user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
                            saveToggle->setText(QObject::tr("修改"));
                        }
                    });
                }
            }
        }
        // 填充数据
        if (auto loading = page->findChild<QLabel*>("acc_loading", Qt::FindDirectChildrenOnly)) loading->hide();
        if (auto idEdit = page->findChild<QLineEdit*>("acc_id"))
            idEdit->setText(QString::number(response.value("clientId").toVariant().toLongLong()));
        if (auto user = page->findChild<QLineEdit*>("acc_user"))
            user->setText(response.value("username").toString());
        if (auto phone = page->findChild<QLineEdit*>("acc_phone"))
            phone->setText(response.value("phone").toString());
        if (auto email = page->findChild<QLineEdit*>("acc_email"))
            email->setText(response.value("email").toString());
        if (auto pwd = page->findChild<QLineEdit*>("acc_pwd"))
            pwd->clear();
        clearToFullPage(page);
    }
    else if (type == "update_account_response") {
        bool ok = response.value("success").toBool(); QString msg = response.value("message").toString();
        statusBar()->showMessage(ok ? (msg.isEmpty()? tr("保存成功"): msg) : (msg.isEmpty()? tr("保存失败"): msg), 3000);
        if (ok) {
            QMessageBox::information(this, tr("修改成功"), msg.isEmpty() ? tr("账户信息已更新") : msg);
        }
        if (ok) {
            // 若用户名被修改，更新本地状态与问候语
            if (auto page = findChild<QWidget*>("accountPage")) {
                if (auto user = page->findChild<QLineEdit*>("acc_user")) {
                    const QString newName = user->text().trimmed();
                    if (!newName.isEmpty() && newName != currentUsername) { currentUsername = newName; updateGreeting(); }
                }
            }
            // 成功后刷新一次账号信息，确保 UI 与服务端一致
            if (socket) {
                QJsonObject req; req["type"] = "get_account_info";
                QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); socket->write(payload);
            }
        }
    }
    else if (type == "orders_response") {
        // 如果这次是从聊天窗口发起（origin=chat），则交由 ChatWindow 自行处理，不构建订单页
        const QString origin = response.value("origin").toString();
        if (origin == QLatin1String("chat")) {
            if (chat) chat->handleMessage(response);
            return;
        }
        // 构建或复用内嵌订单页面（统一顶部返回栏 + 筛选 + 分页，样式统一）
    QWidget *container = findChild<QWidget*>("ordersPage");
    if (!container) {
        container = new QWidget(this);
        container->setObjectName("ordersPage");
        if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(container);
    }
    // 复用已有布局，避免重复叠加导致控件跑位（例如“下一页”出现在左上角）
    auto *v = qobject_cast<QVBoxLayout*>(container->layout());
    if (!v) v = new QVBoxLayout(container); else clearLayout(v);

        // 统一控件样式（与侧栏主色一致）
        container->setStyleSheet(
            "QPushButton { background:#ffffff; border:1px solid #ddd; border-radius:6px; padding:6px 10px; }"
            "QPushButton:hover { background:#fafafa; }"
            "QPushButton:pressed { background:#f0f0f0; }"
            /* 筛选按钮：白字蓝底 */
            "QPushButton#ordersFilterBtn { background:#1677ff; border:1px solid #1677ff; color:#ffffff; }"
            "QPushButton#ordersFilterBtn:hover { background:#3a8cff; border-color:#3a8cff; }"
            "QPushButton#ordersFilterBtn:pressed { background:#2a6bdb; }"
            /* 分页器按钮：白字蓝底，与筛选按钮一致 */
            "QPushButton#ordersPrev, QPushButton#ordersNext { background:#1677ff; border:1px solid #1677ff; color:#ffffff; }"
            "QPushButton#ordersPrev:hover, QPushButton#ordersNext:hover { background:#3a8cff; border-color:#3a8cff; }"
            "QPushButton#ordersPrev:pressed, QPushButton#ordersNext:pressed { background:#2a6bdb; }"
            "QLineEdit, QComboBox { border:1px solid #ddd; border-radius:6px; padding:4px 6px; }"
        );

        // 顶部栏
    auto *topBar = new QHBoxLayout();
        auto *title = new QLabel(tr("历史订单"), container); title->setStyleSheet("font-weight:600;font-size:16px;");
    auto *back = new QPushButton(tr("返回"), container); back->setMinimumWidth(64); back->setStyleSheet("QPushButton{color:#333;}");
        topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(back); v->addLayout(topBar);

        // 筛选栏
        auto *filterBar = new QHBoxLayout();
    auto *statusLbl = new QLabel(tr("状态"), container);
    auto *statusCmb = new QComboBox(container); statusCmb->setObjectName("ordersStatus");
    statusCmb->addItems({ tr("全部"), tr("已支付"), tr("待支付"), tr("已取消") });
        auto *kwLbl = new QLabel(tr("关键字"), container);
        auto *kwEdit = new QLineEdit(container); kwEdit->setObjectName("ordersKeyword"); kwEdit->setPlaceholderText(tr("订单号/用户名"));
    auto *filterBtn = new QPushButton(tr("筛选"), container); filterBtn->setObjectName("ordersFilterBtn");
        filterBar->addWidget(statusLbl); filterBar->addWidget(statusCmb);
        filterBar->addSpacing(8);
        filterBar->addWidget(kwLbl); filterBar->addWidget(kwEdit);
        filterBar->addSpacing(8);
        filterBar->addStretch(1);
        filterBar->addWidget(filterBtn);
        v->addLayout(filterBar);

        // 表格
    auto *table = new QTableWidget(container); table->setObjectName("ordersTable"); table->setColumnCount(5);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::StrongFocus);
        QStringList headers; headers << tr("订单号") << tr("用户名") << tr("金额") << tr("状态") << tr("时间");
        table->setHorizontalHeaderLabels(headers); table->horizontalHeader()->setStretchLastSection(true);
        v->addWidget(table);

        // 分页栏
        auto *pager = new QHBoxLayout();
        auto *pageInfo = new QLabel(container); pageInfo->setObjectName("ordersPageInfo");
    auto *prevBtn = new QPushButton(tr("上一页"), container); prevBtn->setObjectName("ordersPrev");
    auto *nextBtn = new QPushButton(tr("下一页"), container); nextBtn->setObjectName("ordersNext");
    // 分页条左对齐：信息 + 上一页/下一页，右侧留空
    pager->addWidget(pageInfo);
    pager->addSpacing(12);
    pager->addWidget(prevBtn);
    pager->addWidget(nextBtn);
    pager->addStretch(1);
        v->addLayout(pager);

        // 数据与刷新函数
    QJsonArray arr = response.value("orders").toArray();
    container->setProperty("ordersData", arr.toVariantList());
        container->setProperty("ordersPageNo", 1);
        container->setProperty("ordersPageSize", 10);

        auto refresh = [this, container]() {
            auto *table = container->findChild<QTableWidget*>("ordersTable");
            auto *statusCmb = container->findChild<QComboBox*>("ordersStatus");
            auto *kwEdit = container->findChild<QLineEdit*>("ordersKeyword");
            auto *pageInfo = container->findChild<QLabel*>("ordersPageInfo");
            auto *prevBtn = container->findChild<QPushButton*>("ordersPrev");
            auto *nextBtn = container->findChild<QPushButton*>("ordersNext");
            if (!table || !statusCmb || !kwEdit || !pageInfo || !prevBtn || !nextBtn) return;

            const QVariantList data = container->property("ordersData").toList();
            const int pageSize = container->property("ordersPageSize").toInt();
            int pageNo = container->property("ordersPageNo").toInt(); if (pageNo < 1) pageNo = 1;
            const QString statusSel = statusCmb->currentText();
            const QString kw = kwEdit->text().trimmed();

            // 过滤
            QVariantList filtered;
            filtered.reserve(data.size());
            for (const QVariant &v : data) {
                const QVariantMap m = v.toMap();
                QString status = m.value("status").toString();
                // 兼容后端或数据里出现的英文枚举，统一映射到中文以用于比较
                if (status.compare("PAID", Qt::CaseInsensitive) == 0) status = tr("已支付");
                else if (status.compare("CART", Qt::CaseInsensitive) == 0 || status.compare("PENDING", Qt::CaseInsensitive) == 0) status = tr("待支付");
                else if (status.compare("REFUNDED", Qt::CaseInsensitive) == 0 || status.compare("CANCELLED", Qt::CaseInsensitive) == 0) status = tr("已取消");
                const QString uname = m.value("username").toString();
                const QString idStr = QString::number(m.value("orderId").toLongLong());
                if (statusSel != tr("全部") && status != statusSel) continue;
                if (!kw.isEmpty() && !(idStr.contains(kw, Qt::CaseInsensitive) || uname.contains(kw, Qt::CaseInsensitive))) continue;
                filtered.push_back(v);
            }

            const int total = filtered.size();
            const int totalPages = qMax(1, (total + pageSize - 1) / pageSize);
            if (pageNo > totalPages) pageNo = totalPages;
            container->setProperty("ordersPageNo", pageNo);

            // 填充当前页
            const int start = (pageNo - 1) * pageSize;
            const int end = qMin(start + pageSize, total);
            table->setRowCount(qMax(0, end - start));
            int r = 0;
            for (int i = start; i < end; ++i) {
                const QVariantMap o = filtered.at(i).toMap();
                auto *idItem = new QTableWidgetItem(QString::number(o.value("orderId").toLongLong()));
                // 绑定整单数据供双击时查看详情
                idItem->setData(Qt::UserRole, o);
                table->setItem(r, 0, idItem);
                QString uname = o.value("username").toString(); if (uname.isEmpty()) uname = currentUsername;
                table->setItem(r, 1, new QTableWidgetItem(uname));
                // 计算订单维度的单品折扣与满减（每满200减20）并展示
                auto items = o.value("items").toList();
                double sumOriginal = 0.0;   // 原价合计
                double sumEffective = 0.0;  // 单品折扣后合计
                for (const QVariant &iv : items) {
                    const QVariantMap it = iv.toMap();
                    const int qty = it.value("quantity").toInt();
                    const double price = it.value("price").toDouble();
                    const double listPrice = it.contains("listPrice") ? it.value("listPrice").toDouble() : price;
                    const bool onSale = it.value("onSale").toBool();
                    const double dprice = it.contains("discountPrice") ? it.value("discountPrice").toDouble() : 0.0;
                    const bool hasDiscount = onSale && dprice>0.0 && dprice<listPrice;
                    const double unit = hasDiscount ? dprice : price;
                    sumOriginal += listPrice * qty;
                    sumEffective += unit * qty;
                }
                // 若后端列表未返回明细，则用 total_price 作为基准计算满减展示
                if (items.isEmpty()) {
                    sumOriginal = o.value("total_price").toDouble();
                    sumEffective = sumOriginal;
                }
                // 满减：每满200减20（基于单品折后合计）
                const qint64 cents = static_cast<qint64>(qRound64(sumEffective * 100.0));
                const qint64 threshold = 20000; // 200元
                const qint64 stepOff  = 2000;   // 20元
                const qint64 times    = (cents>0 ? (cents/threshold) : 0);
                const qint64 promoOff = times * stepOff;
                const qint64 finalPay = qMax<qint64>(0, cents - promoOff);
                const bool showStrike = finalPay < static_cast<qint64>(qRound64(sumOriginal * 100.0));
                // 列表仅显示“实付金额”（黑色），满减拆解放在详情弹窗
                table->setItem(r, 2, new QTableWidgetItem(QString::number(finalPay/100.0, 'f', 2)));
                table->setItem(r, 3, new QTableWidgetItem(o.value("status").toString()));
                table->setItem(r, 4, new QTableWidgetItem(o.value("order_time").toString()));
                ++r;
            }
            pageInfo->setText(tr("第 %1 / %2 页 · 共 %3 条").arg(pageNo).arg(totalPages).arg(total));
            prevBtn->setEnabled(pageNo > 1);
            nextBtn->setEnabled(pageNo < totalPages);
        };

        // 交互
        // 为避免重复连接导致的重复回调/野指针，先断开旧的（如果有），再建立一次新的连接
        QObject::disconnect(filterBtn, nullptr, nullptr, nullptr);
        QObject::disconnect(kwEdit, nullptr, nullptr, nullptr);
        QObject::disconnect(statusCmb, nullptr, nullptr, nullptr);
        QObject::disconnect(prevBtn, nullptr, nullptr, nullptr);
        QObject::disconnect(nextBtn, nullptr, nullptr, nullptr);
        connect(filterBtn, &QPushButton::clicked, this, [container, refresh]{ container->setProperty("ordersPageNo", 1); refresh(); });
        connect(kwEdit, &QLineEdit::returnPressed, this, [container, refresh]{ container->setProperty("ordersPageNo", 1); refresh(); });
        connect(statusCmb, qOverload<int>(&QComboBox::currentIndexChanged), this, [container, refresh](int){ container->setProperty("ordersPageNo", 1); refresh(); });
        connect(prevBtn, &QPushButton::clicked, this, [container, refresh]{ int p = container->property("ordersPageNo").toInt(); if (p>1) { container->setProperty("ordersPageNo", p-1); refresh(); } });
        connect(nextBtn, &QPushButton::clicked, this, [container, refresh]{ int p = container->property("ordersPageNo").toInt(); container->setProperty("ordersPageNo", p+1); refresh(); });

        // 双击表格行查看订单详情（小票样式）
        if (auto *tbl = container->findChild<QTableWidget*>("ordersTable")) {
            QObject::disconnect(tbl, nullptr, nullptr, nullptr);
            auto showOrderDetail = [this, tbl](int row){
                if (row < 0) return;
                auto *idItem = tbl->item(row, 0);
                if (!idItem) { QMessageBox::warning(this, tr("提示"), tr("未获取到订单数据")); return; }
                const QVariantMap order = idItem->data(Qt::UserRole).toMap();
                if (order.isEmpty()) { QMessageBox::warning(this, tr("提示"), tr("未获取到订单数据")); return; }
                const qlonglong orderId = order.value("orderId").toLongLong();
                const QString status = order.value("status").toString();
                QString timeStr = order.value("order_time").toString();
                // 友好化时间显示：将 ISO 8601 中的 'T' 替换为空格
                if (!timeStr.isEmpty()) timeStr.replace('T', ' ');
                const QVariantList items = order.value("items").toList();
                // 生成小票 HTML
                QString html;
                html += QString("<div style='font-weight:700;font-size:14px;margin-bottom:6px;'>订单 #%1</div>").arg(orderId);
                if (!timeStr.isEmpty()) html += QString("<div style='color:#666;'>时间：%1</div>").arg(timeStr);
                if (!status.isEmpty()) html += QString("<div style='color:#666;margin-bottom:6px;'>状态：%1</div>").arg(status);
                html += QString("<table style='width:100%;border-collapse:collapse;%1%2%3'>")
                            .arg("font-family:'Microsoft YaHei','Segoe UI','PingFang SC','Helvetica Neue',Arial,sans-serif;")
                            .arg("font-size:13px;")
                            .arg("");
                html += "<tr><th style='text-align:left;border-bottom:1px solid #eee;padding:4px 0;'>商品</th>"
                        "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>数量</th>"
                        "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>单价</th>"
                        "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>小计</th></tr>";
                double sumOriginal = 0.0;
                double sumEffective = 0.0;
                for (const QVariant &iv : items) {
                    const QVariantMap it = iv.toMap();
                    QString name = it.value("name").toString();
                    if (it.contains("size") && it.value("size").toInt()>0) {
                        name += QString("  (尺码:%1)").arg(it.value("size").toInt());
                    }
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
                    html += QString("<tr><td style='padding:4px 0;'>%1</td>"
                                    "<td style='text-align:right;padding:4px 0;'>%2</td>"
                    "<td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%3</td>"
                                    "<td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%4</td></tr>")
                                .arg(name.toHtmlEscaped())
                                .arg(qty)
                .arg(QString::number(unit, 'f', 2))
                .arg(QString::number(sub, 'f', 2));
                }
                // 若无明细，使用 total_price 作为基准（仍按活动规则展示合计信息）
                if (items.isEmpty()) {
                    sumOriginal = order.value("total_price").toDouble();
                    sumEffective = sumOriginal;
                }
        // 订单级满减（每满200减20），基于单品折后合计
        const qint64 cents = static_cast<qint64>(qRound64(sumEffective * 100.0));
        const qint64 threshold = 20000; // 200元
        const qint64 stepOff  = 2000;   // 20元
        const qint64 times    = (cents>0 ? (cents/threshold) : 0);
        const qint64 promoOff = times * stepOff;
        const qint64 finalPay = qMax<qint64>(0, cents - promoOff);
        // 合计区：原价（划线）/ 折后（可选）/ 满减 / 应付
        html += QString("<tr><td colspan='4' style='border-top:1px solid #eee;padding-top:6px;text-align:right;'>");
        if (sumOriginal > sumEffective + 1e-6) {
            html += QString("<div style='color:#999;text-decoration:line-through;'>原价合计：&yen;&nbsp;%1</div>")
                .arg(QString::number(sumOriginal, 'f', 2));
        }
        if (promoOff > 0) {
            html += QString("<div>商品折后：&yen;&nbsp;%1</div>")
                .arg(QString::number(sumEffective, 'f', 2));
            html += QString("<div style='color:#43A047;'>满减：-&yen;&nbsp;%1</div>")
                .arg(QString::number(promoOff/100.0, 'f', 2));
        }
        html += QString("<div style='font-weight:700;color:#E53935;'>应付：&yen;&nbsp;%1</div>")
            .arg(QString::number(finalPay/100.0, 'f', 2));
        html += "</td></tr>";
                html += "</table>";

                QMessageBox box(this);
                box.setWindowTitle(tr("订单详情"));
                box.setTextFormat(Qt::RichText);
                box.setText(html);
                box.addButton(tr("关闭"), QMessageBox::RejectRole);
                box.exec();
            };
            connect(tbl, &QTableWidget::itemDoubleClicked, this, [showOrderDetail](QTableWidgetItem *item){ showOrderDetail(item ? item->row() : -1); });
            connect(tbl, &QTableWidget::cellDoubleClicked, this, [showOrderDetail](int row, int /*column*/){ showOrderDetail(row); });
            connect(tbl, &QTableWidget::itemActivated, this, [showOrderDetail](QTableWidgetItem *item){ showOrderDetail(item ? item->row() : -1); });
            connect(tbl, &QTableWidget::cellActivated, this, [showOrderDetail](int row, int /*column*/){ showOrderDetail(row); });
            connect(static_cast<QTableView*>(tbl), &QTableView::doubleClicked, this, [showOrderDetail](const QModelIndex &idx){ showOrderDetail(idx.isValid() ? idx.row() : -1); });
        }

        // 返回
    QObject::disconnect(back, nullptr, nullptr, nullptr);
    connect(back, &QPushButton::clicked, this, [this, container]{ container->hide(); container->deleteLater(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });

        // 首次渲染
        refresh();
        clearToFullPage(container);
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

    // 将聊天相关消息转发给 ChatWindow（若已创建）
    if (chat) {
        const QString t = response.value("type").toString();
        if (t == QLatin1String("chat_init_response")
            || t == QLatin1String("chat_message")
            || t == QLatin1String("presence")
            || t == QLatin1String("chat_delete_response")) {
            chat->handleMessage(response);
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
        if (auto *childLayout = it->layout()) {
            // 递归清理并销毁子布局，防止遗留的控件/布局导致下次构建时错乱
            clearLayout(childLayout);
            delete childLayout;
        } else if (auto *w = it->widget()) {
            // 控件用 deleteLater，避免同步销毁引发信号回调访问已释放对象
            w->deleteLater();
        }
        // QSpacerItem 等通过删除 it 即可清理
        delete it;
    }
    // 失效布局缓存，确保后续计算重新进行
    layout->invalidate();
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
    Q_UNUSED(images);
    auto *layout = ensureVBoxLayout(ui->carouselArea);
    // 重建前先停止并断开旧的计时器连接，防止回调引用已销毁的控件导致崩溃
    if (carouselTimer) {
        carouselTimer->stop();
        QObject::disconnect(carouselTimer, nullptr, nullptr, nullptr);
    }
    clearLayout(layout);
    // 读取本地路径（用户提供目录 C:/Users/Edward/Desktop/test/images 下的 home_page 1-4.*）
    {
        const QString baseDir = QStringLiteral("C:/Users/Edward/Desktop/test/images");
        QDir dir(baseDir);
        QStringList fromFolder;
        if (dir.exists()) {
            const QStringList exts = {"png","jpg","jpeg","webp","bmp"};
            for (int i = 1; i <= 4; ++i) {
                bool found = false;
                for (const QString &ext : exts) {
                    const QString fp = QStringLiteral("%1/home_page %2.%3").arg(baseDir).arg(i).arg(ext);
                    if (QFileInfo::exists(fp)) { fromFolder << fp; found = true; break; }
                }
                if (!found) {
                    // 容错：允许没有扩展一致时跳过该序号
                }
            }
        }
        if (!fromFolder.isEmpty()) {
            carouselLocalPaths = fromFolder;
        }
        // 预加载原图（每次刷新目录结果后都重建缓存）
        carouselOriginals.clear();
        for (const QString &p : carouselLocalPaths) carouselOriginals.push_back(QPixmap(p));
    }
    // 三联图容器：prev | main | next
    QWidget *row = new QWidget(ui->carouselArea);
    auto *h = new QHBoxLayout(row); h->setContentsMargins(0,0,0,0); h->setSpacing(4);
    QLabel *prevLbl = new QLabel(row); prevLbl->setAlignment(Qt::AlignCenter); prevLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QLabel *mainLbl = new QLabel(row); mainLbl->setAlignment(Qt::AlignCenter); mainLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QLabel *nextLbl = new QLabel(row); nextLbl->setAlignment(Qt::AlignCenter); nextLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    prevLbl->setObjectName("carouselPrev"); mainLbl->setObjectName("carouselMain"); nextLbl->setObjectName("carouselNext");
    prevLbl->setStyleSheet("border-radius:6px; border:1px solid #ddd;");
    mainLbl->setStyleSheet("border-radius:8px; border:1px solid #ccc;");
    nextLbl->setStyleSheet("border-radius:6px; border:1px solid #ddd;");
    auto *prevEff = new QGraphicsOpacityEffect(prevLbl); prevEff->setOpacity(0.45); prevLbl->setGraphicsEffect(prevEff);
    auto *nextEff = new QGraphicsOpacityEffect(nextLbl); nextEff->setOpacity(0.45); nextLbl->setGraphicsEffect(nextEff);
    // 调整权重：中间主图更大，两侧预览更小
    h->addWidget(prevLbl, 10); h->addWidget(mainLbl, 80); h->addWidget(nextLbl, 10);
    layout->addWidget(row);
    // 保存主/侧标签并允许点击切换
    carouselImageLabel = mainLbl; carouselPrevLabel = prevLbl; carouselNextLabel = nextLbl;
    prevLbl->installEventFilter(this); nextLbl->installEventFilter(this);
    auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
    auto refreshAll = [this, prevLbl, nextLbl, idxWrap](){
        refreshCarouselPixmap();
        if (!carouselOriginals.isEmpty()) {
            const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
            const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
            int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
            int sideW = qMax(110, (contentW - 40) * 10 / 100);
            int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
            const int maxSideH = qMax(110, mainH * 75 / 100);
            prevLbl->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
            nextLbl->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
            prevLbl->setFixedWidth(sideW);
            nextLbl->setFixedWidth(sideW);
            prevLbl->setFixedHeight(prevLbl->pixmap(Qt::ReturnByValue).height());
            nextLbl->setFixedHeight(nextLbl->pixmap(Qt::ReturnByValue).height());
        }
        updateCarouselDots();
    };
    // 初始帧
    carouselIndex = 0;
    refreshAll();
    // 主图淡入动画
    auto playFade = [this](){
        if (!carouselImageLabel) return;
        auto *eff = new QGraphicsOpacityEffect(carouselImageLabel);
        carouselImageLabel->setGraphicsEffect(eff);
        auto *ani = new QPropertyAnimation(eff, "opacity", carouselImageLabel);
        ani->setDuration(400);
        ani->setStartValue(0.0);
        ani->setEndValue(1.0);
        ani->start(QAbstractAnimation::DeleteWhenStopped);
    };
    // 定时自动轮播
    // 统一（重新）配置计时器，并把回调上下文绑定到本次渲染生成的 row，
    // 这样 row 被销毁时该连接会自动断开，避免悬空指针
    if (!carouselTimer) {
        carouselTimer = new QTimer(this);
    }
    carouselTimer->setInterval(2500);
    connect(carouselTimer, &QTimer::timeout, row, [this, refreshAll, playFade]{
        if (carouselOriginals.isEmpty()) return;
        carouselIndex = (carouselIndex + 1) % carouselOriginals.size();
        refreshAll();
        playFade();
    });
    carouselTimer->start();
    // 底部小圆点
    {
    auto *dotRow = new QHBoxLayout(); dotRow->setContentsMargins(0,4,0,0);
        dotRow->setSpacing(6); dotRow->addStretch(1);
        carouselDots.clear();
        for (int i=0;i<carouselOriginals.size();++i) {
            auto *d = new QLabel(ui->carouselArea);
            d->setFixedSize(10,10);
            d->setStyleSheet("border-radius:5px;background:#ddd;");
            d->setProperty("dotIndex", i);
            d->installEventFilter(this);
            carouselDots.push_back(d);
            dotRow->addWidget(d);
        }
        dotRow->addStretch(1);
        auto *host = new QWidget(ui->carouselArea); host->setLayout(dotRow);
        layout->addWidget(host);
        updateCarouselDots();
    }
}

void MainWindow::renderRecommendations(const QJsonArray &products)
{
    // 保存最近数据并标记当前区域
    lastRecommendations = products;
    showingList = false;
    auto *container = ui->recommendationsArea;
    auto *layout = ensureVBoxLayout(container);
    clearLayout(layout);
    // 标题行
    {
        auto *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0,0,0,0);
    auto *title = new QLabel(tr("首页精选"), container);
        title->setStyleSheet("font-weight:600;margin:6px 0;");
        titleRow->addWidget(title);
        titleRow->addStretch(1);
        auto *moreBtn = new QPushButton(tr("更多 >"), container);
        moreBtn->setObjectName("homeMoreDiscountBtn");
        moreBtn->setStyleSheet(
            "QPushButton{color:#ffffff;background:#1677ff;border:1px solid #1677ff;border-radius:6px;padding:2px 10px;}"
            "QPushButton:hover{background:#3a8cff;border-color:#3a8cff;}"
        );
        titleRow->addWidget(moreBtn);
        auto *host = new QWidget(container); host->setLayout(titleRow); layout->addWidget(host);
        connect(moreBtn, &QPushButton::clicked, this, [this]{ showMallView(); });
    }
    if (products.isEmpty()) { renderRecommendationsPlaceholder(); return; }
    // 选择规则：1 个新品 + 3 个打折；不足则用其它补齐，合计最多 4 个
    QList<QJsonObject> items; items.reserve(products.size());
    for (const auto &v : products) items.push_back(v.toObject());
    auto isDiscounted = [](const QJsonObject &o){
        const double price = o.value("price").toDouble();
        const double discount = o.value("discountPrice").toDouble(0.0);
        const bool onSale = o.value("onSale").toBool();
        return onSale && discount>0.0 && discount<price;
    };
    QJsonArray picked;
    int newIdx = -1;
    for (int i=0;i<items.size();++i) if (items[i].value("isNew").toBool()) { picked.append(items[i]); newIdx = i; break; }
    for (int i=0;i<items.size() && picked.size()<4; ++i) {
        if (i==newIdx) continue;
        if (isDiscounted(items[i])) picked.append(items[i]);
    }
    for (int i=0;i<items.size() && picked.size()<4; ++i) {
        if (i==newIdx) continue;
        const int pid = items[i].value("product_id").toInt();
        bool exists=false; for (const auto &pv : picked) { if (pv.toObject().value("product_id").toInt()==pid) { exists=true; break; } }
        if (!exists) picked.append(items[i]);
    }
    // 渲染 2x2 网格
    auto *grid = new QGridLayout();
    grid->setContentsMargins(4,4,4,4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    const int count = qMin(4, picked.size());
    for (int i=0;i<count; ++i) {
        const QJsonObject o = picked.at(i).toObject();
        const int pid = o.value("product_id").toInt();
        const QString name = o.value("name").toString(QLatin1String("商品"));
        const double price = o.value("price").toDouble();
        const int stock = o.value("stock").toInt(-1);
        auto *card = new QFrame(container);
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet("QFrame{border:1px solid #ddd;border-radius:8px;background:#fff;} QLabel{color:#333;font-size:13px;}");
        auto *vbox = new QVBoxLayout(card); vbox->setContentsMargins(8,8,8,8); vbox->setSpacing(4);
        // 角标
        bool soldOut = (stock==0);
        bool isNew = o.value("isNew").toBool();
        if (soldOut || isNew) {
            auto *row = new QHBoxLayout(); row->setContentsMargins(0,0,0,0); row->addStretch(1);
            auto *badge = new QLabel(soldOut ? tr("售罄") : tr("新品"), card);
            badge->setStyleSheet(soldOut ? "QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}"
                                           : "QLabel{background:#34A853;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            row->addWidget(badge, 0, Qt::AlignRight); vbox->addLayout(row);
        }
        // 图片
    auto *img = new QLabel(card); img->setAlignment(Qt::AlignCenter); img->setMinimumHeight(100);
    img->setStyleSheet("QLabel{background:#fafafa;border:1px solid #eee;border-radius:6px;}"); vbox->addWidget(img);
    // 兼容后端字段名 image_url / imageUrl
    QString imgUrl = o.value("image_url").toString();
    if (imgUrl.isEmpty()) imgUrl = o.value("imageUrl").toString();
    if (!imgUrl.isEmpty()) setImageFromUrl(imgUrl, img, QSize(160, 100));
        // 名称
        auto *nameLbl = new QLabel(name, card); nameLbl->setStyleSheet("font-weight:600;"); nameLbl->setWordWrap(true);
        vbox->addWidget(nameLbl);
        // 价格
        QLabel *priceLbl=nullptr, *discountLbl=nullptr;
        const bool onSale = o.value("onSale").toBool(); const double discount = o.value("discountPrice").toDouble(0.0);
        const bool hasDiscount = onSale && discount>0.0 && discount<price;
        if (hasDiscount) {
            priceLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
            priceLbl->setStyleSheet("color:#999;text-decoration:line-through;");
            discountLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(discount, 'f', 2)), card);
            discountLbl->setStyleSheet("color:#E53935;font-weight:700;");
        } else {
            priceLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
        }
        vbox->addWidget(priceLbl); if (discountLbl) vbox->addWidget(discountLbl);
        // 按钮
        auto *btnRow = new QHBoxLayout(); auto *detailBtn = new QPushButton(tr("详情"), card); auto *addBtn = new QPushButton(tr("加入购物车"), card);
        detailBtn->setMinimumSize(64,28); addBtn->setMinimumSize(86,28); if (stock==0) addBtn->setEnabled(false);
        btnRow->addWidget(detailBtn); btnRow->addWidget(addBtn); btnRow->addStretch(1); vbox->addLayout(btnRow);
        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
    connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock, -1); });
        // 放入 2 列网格
        grid->addWidget(card, i/2, i%2);
    }
    auto *gridHost = new QWidget(container); gridHost->setLayout(grid); layout->addWidget(gridHost);
    lastColumns = 2;
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
    // 排序工具条（商城页）
    {
        auto *bar = new QHBoxLayout();
        bar->setContentsMargins(0,0,0,0);
        auto *lbl = new QLabel(tr("排序:"), container);
        auto *cmb = new QComboBox(container); cmb->setObjectName("mallSortCombo");
        // 四个选项：销量优先、价格升序、价格降序、打折优先
        cmb->addItem(tr("销量优先"), QStringLiteral("sales_desc"));
        cmb->addItem(tr("价格升序"), QStringLiteral("price_asc"));
        cmb->addItem(tr("价格降序"), QStringLiteral("price_desc"));
        cmb->addItem(tr("打折优先"), QStringLiteral("discount_first"));
        // 从窗口属性恢复选择，默认打折优先
        QString mode = this->property("mallSortMode").toString();
        if (mode.isEmpty()) mode = QStringLiteral("discount_first");
        for (int i=0;i<cmb->count();++i) { if (cmb->itemData(i).toString()==mode) { cmb->setCurrentIndex(i); break; } }
        bar->addWidget(lbl); bar->addWidget(cmb); bar->addStretch(1);
        auto *barHost = new QWidget(container); barHost->setLayout(bar); layout->addWidget(barHost);
        // 选择变化时，记录到属性并按最新规则重新渲染当前 lastResults，同时请求第一页（服务端排序覆盖全量）
        connect(cmb, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, cmb]{
            this->setProperty("mallSortMode", cmb->currentData().toString());
            // 立即请求第一页，带上排序参数；本地立即重排作为视觉反馈
            requestProductsPage(1);
            if (!lastResults.isEmpty()) renderSearchResults(lastResults);
        });
    }
    auto *title = new QLabel(tr("商品列表"), container);
    title->setWordWrap(true);
    title->setStyleSheet("font-weight:600;margin:6px 0;");
    layout->addWidget(title);
    if (results.isEmpty()) {
        layout->addWidget(new QLabel(tr("未找到相关商品"), container));
        return;
    }
    // 根据选择应用排序（本地排序，若后端已排序则保持一致；此处只对当前页数据排序）
    QString mode = this->property("mallSortMode").toString();
    if (mode.isEmpty()) mode = QStringLiteral("discount_first");
    QList<QJsonObject> list; list.reserve(results.size());
    for (const auto &v : results) list.push_back(v.toObject());
    auto effectivePrice = [](const QJsonObject &o){
        const double price = o.value("price").toDouble();
        const double discount = o.value("discountPrice").toDouble(0.0);
        const bool onSale = o.value("onSale").toBool();
        if (onSale && discount>0.0 && discount<price) return discount; else return price;
    };
    auto hasDiscount = [](const QJsonObject &o){
        const double price = o.value("price").toDouble();
        const double discount = o.value("discountPrice").toDouble(0.0);
        const bool onSale = o.value("onSale").toBool();
        return onSale && discount>0.0 && discount<price;
    };
    std::sort(list.begin(), list.end(), [&](const QJsonObject &a, const QJsonObject &b){
        if (mode == QLatin1String("sales_desc")) {
            const int sa = a.value("sales").toInt();
            const int sb = b.value("sales").toInt();
            if (sa != sb) return sa > sb; // 降序
            return effectivePrice(a) < effectivePrice(b); // 次级：低价优先
        } else if (mode == QLatin1String("price_asc")) {
            const double pa = effectivePrice(a), pb = effectivePrice(b);
            if (pa != pb) return pa < pb;
            return a.value("sales").toInt() > b.value("sales").toInt();
        } else if (mode == QLatin1String("price_desc")) {
            const double pa = effectivePrice(a), pb = effectivePrice(b);
            if (pa != pb) return pa > pb;
            return a.value("sales").toInt() > b.value("sales").toInt();
        } else /* discount_first */ {
            const bool da = hasDiscount(a), db = hasDiscount(b);
            if (da != db) return da && !db; // 有折扣在前
            // 次级：按有效价升序
            const double pa = effectivePrice(a), pb = effectivePrice(b);
            if (pa != pb) return pa < pb;
            return a.value("sales").toInt() > b.value("sales").toInt();
        }
    });
    QJsonArray sorted; for (const auto &o : list) sorted.append(o);
    auto *grid = new QGridLayout();
    grid->setContentsMargins(4,4,4,4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    int available = container->width();
    int colCount = computeColumns(available);
    int idx = 0;
    for (auto v : sorted) {
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
        // 右上角角标：优先“售罄”，否则“新品”
        bool showSoldOut2 = (stock == 0);
        bool showNew2 = (!showSoldOut2 && o.value("isNew").toBool());
        if (showSoldOut2 || showNew2) {
            auto *badgeRow = new QHBoxLayout();
            badgeRow->setContentsMargins(0,0,0,0);
            badgeRow->addStretch(1);
            auto *badge = new QLabel(showSoldOut2 ? tr("售罄") : tr("新品"), card);
            if (showSoldOut2) badge->setStyleSheet("QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            else badge->setStyleSheet("QLabel{background:#34A853;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            badgeRow->addWidget(badge, 0, Qt::AlignRight);
            vbox->addLayout(badgeRow);
        }
        // 图片区域（固定高，等比缩放）
        {
            auto *img = new QLabel(card);
            img->setObjectName("img");
            img->setAlignment(Qt::AlignCenter);
            img->setMinimumHeight(100);
            img->setStyleSheet("QLabel{background:#fafafa;border:1px solid #eee;border-radius:6px;}");
            vbox->addWidget(img);
            // 若有 image_url 字段，发起加载
            const QString imgUrl = o.value("image_url").toString();
            if (!imgUrl.isEmpty()) {
                setImageFromUrl(imgUrl, img, QSize(160, 100));
            }
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
        // 销量标签（若返回了 sales 字段）
        if (o.contains("sales")) {
            const int sales = o.value("sales").toInt();
            auto *salesLbl = new QLabel(tr("销量 %1").arg(sales), card);
            salesLbl->setStyleSheet("color:#666;font-size:12px;");
            vbox->addWidget(salesLbl);
        }
        vbox->addStretch(1); // 将按钮推到底部
        vbox->addLayout(btnRow); // 把按钮行加入布局

        int r = idx / colCount;
        int c = idx % colCount;
        grid->addWidget(card, r, c);
        ++idx;

        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
    connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock, -1); });
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
    if (currentView == ViewMode::Home && homeHeaderImage && homeHeaderImage->isVisible()) {
        applyHomeHeaderImage();
    }
    // 首页时联动刷新轮播尺寸与左右预览，避免主图显示不全
    if (currentView == ViewMode::Home && carouselImageLabel) {
        refreshCarouselPixmap();
        if (carouselPrevLabel && carouselNextLabel && !carouselOriginals.isEmpty()) {
            int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
            int sideW = qMax(110, (contentW - 40) * 10 / 100);
            int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
            const int maxSideH = qMax(110, mainH * 75 / 100);
            auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
            const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
            const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
            carouselPrevLabel->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
            carouselNextLabel->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
            carouselPrevLabel->setFixedWidth(sideW);
            carouselNextLabel->setFixedWidth(sideW);
            carouselPrevLabel->setFixedHeight(carouselPrevLabel->pixmap(Qt::ReturnByValue).height());
            carouselNextLabel->setFixedHeight(carouselNextLabel->pixmap(Qt::ReturnByValue).height());
        }
    }
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
    int sales = product.value("sales").toInt(-1);
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
    // 可选描述；将属性改为竖排逐行展示
    const QString desc = product.value("description").toString();
    QString text;
    text += QString("<div><b>名称：</b>%1</div>").arg(name.toHtmlEscaped());
    if (!desc.isEmpty()) {
        text += QString("<div><b>描述：</b>%1</div>").arg(desc.toHtmlEscaped());
    }
    text += QString("<div><b>价格：</b>%1</div>").arg(priceLine);
    if (stock >= 0) text += QString("<div><b>库存：</b>%1</div>").arg(stock);
    if (sales >= 0) text += QString("<div><b>销量：</b>%1</div>").arg(sales);
    // 库存为0显示醒目的售罄徽标
    bool soldOut = (stock == 0);
    if (soldOut) {
        // 使用富文本渲染一个红底白字圆角徽标
        text += QString("<div style='margin-top:6px;'>%1</div>")
                    .arg("<span style='background:#E53935;color:#fff;border-radius:12px;padding:4px 10px;font-weight:600;'>售罄</span>");
    }
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    QPushButton *add = box.addButton(tr("加入购物车"), QMessageBox::AcceptRole);
    if (soldOut) add->setEnabled(false);
    box.addButton(tr("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (!soldOut && box.clickedButton() == add) {
        // 优先使用后端返回的每尺码库存（sizes: [{size,stock}, ...]）；否则回退到 37-45 全量
        QList<QPair<int,int>> sizeStock; // {size, stock}
        if (product.contains("sizes") && product.value("sizes").isArray()) {
            for (const auto &v : product.value("sizes").toArray()) {
                const auto o = v.toObject();
                sizeStock.append({ o.value("size").toInt(), o.value("stock").toInt() });
            }
        }
        if (sizeStock.isEmpty()) {
            for (int s=37; s<=45; ++s) sizeStock.append({s, -1}); // -1 表示未知或无限
        }
        QStringList options; options.reserve(sizeStock.size());
        QList<int> enableMask; enableMask.reserve(sizeStock.size());
        for (const auto &ss : sizeStock) {
            const int s = ss.first; const int st = ss.second;
            const bool ok = (st != 0); // 0 无货；-1 或 >0 视为可选
            options << QString::number(s) + (st>=0? QString("  (库存:%1)").arg(st) : QString());
            enableMask << (ok?1:0);
        }
        // 使用自定义对话框/临时 QInputDialog：禁用项通过提示拦截
        bool okSize = false; int defIndex = qBound(0, 42-37, options.size()-1);
        QString chosen = QInputDialog::getItem(this, tr("选择尺码"), tr("尺码"), options, defIndex, false, &okSize);
        if (!okSize) return;
        int idx = options.indexOf(chosen);
        if (idx < 0) return;
        if (enableMask.value(idx,1)==0) { QMessageBox::information(this, tr("提示"), tr("该尺码暂无库存，请选择其他尺码")); return; }
        int size = sizeStock.value(idx).first;
        addToCart(pid, stock, size);
    }
}

void MainWindow::addToCart(int productId, int stock, int size)
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
    // 若未指定有效尺码，则先让用户选择（37-45，默认 42）
    if (size <= 0) {
        // 若没传入尺码，尝试从最近一次商品详情缓存或默认 37-45；这里无法知晓具体商品的每尺码库存，退回简单列表
        QStringList sizes; for (int s=37; s<=45; ++s) sizes << QString::number(s);
        bool okSize = false;
        QString chosen = QInputDialog::getItem(this, tr("选择尺码"), tr("尺码"), sizes, /*current*/ 42-37, false, &okSize);
        if (!okSize) return;
        bool okConv = false; int sz = chosen.toInt(&okConv); size = okConv ? sz : 42;
    }
    int quantity = QInputDialog::getInt(this, tr("加入购物车"), tr("数量"), 1, 1, maxQty, 1, &ok);
    if (!ok) return;
    QJsonObject req;
    req["type"] = "add_to_cart";
    req["product_id"] = productId;
    req["quantity"] = quantity;
    if (size > 0) req["size"] = size;
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
    // 携带排序参数（如果未设置则使用默认：打折优先）
    QString mode = this->property("mallSortMode").toString();
    if (mode.isEmpty()) mode = QStringLiteral("discount_first");
    req["sort"] = mode;
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

// ---- 视图模式切换 ----
void MainWindow::showHomeView()
{
    currentView = ViewMode::Home;
    lastNonCartView = ViewMode::Home;
    setTabActive("home");
    setSearchBarVisible(true);
    // 切回首页时，确保全屏页（个人中心/订单/聊天）被销毁，避免叠加和回调访问已失效对象
    if (auto p = findChild<QWidget*>("accountPage")) { p->hide(); p->deleteLater(); }
    if (auto p = findChild<QWidget*>("ordersPage")) { p->hide(); p->deleteLater(); }
    if (auto p = findChild<QWidget*>("chatPage")) { p->hide(); p->deleteLater(); }
    if (chat) { chat->deleteLater(); chat = nullptr; }
    // 回到首页前确保轮播计时器处于可控状态（若已存在，稍后 renderCarousel 会重建连接并启动）
    if (carouselTimer) { carouselTimer->stop(); }
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(true);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(true);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(true);
    if (promoTimer && !promoTimer->isActive()) promoTimer->start();
    updateGreeting();
    // 顶部头图已移除
    if (ui->carouselArea) ui->carouselArea->setVisible(true);
    // 隐藏促销区（按用户要求去掉“开学季/会员日”两行）
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(true);
    // 隐藏全局分页器（仅商城显示）
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 隐藏购物车页（如存在）
    if (cart) cart->hide();
    // 刷新首页数据
    // 确保本地轮播就绪
    if (!carouselImageLabel) {
        // 先尝试构建一次本地轮播，避免服务器暂未返回数据时显示文字占位
        // 具体内容在 renderCarousel 中也会再次确保
        // 初始化路径列表以便后续 renderCarousel 使用
        carouselLocalPaths = QStringList{
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/4fff3de19840e88d717fb372351454fe.png"),
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/888cc0d3b9d75bc12017b7f0c3393a38.png"),
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/1569583902c0ce638ebd23c952202400.png"),
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/dd96060742b6578bd89299dbc31c46c4.png")
        };
    }
    loadCarousel();
    // 不再加载促销数据
    loadRecommendations();
}

void MainWindow::showMallView()
{
    currentView = ViewMode::Mall;
    lastNonCartView = ViewMode::Mall;
    setTabActive("mall");
    setSearchBarVisible(true);
    // 进入商城页时也直接销毁全屏页，避免叠加
    if (auto p = findChild<QWidget*>("accountPage")) { p->hide(); p->deleteLater(); }
    if (auto p = findChild<QWidget*>("ordersPage")) { p->hide(); p->deleteLater(); }
    if (auto p = findChild<QWidget*>("chatPage")) { p->hide(); p->deleteLater(); }
    if (chat) { chat->deleteLater(); chat = nullptr; }
    // 离开首页，停止轮播
    if (carouselTimer) { carouselTimer->stop(); }
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(false);
    if (promoTimer) promoTimer->stop();
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(true);
    // 仅商城显示全局分页器
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(true);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(true);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(true);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(true);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(true);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(true);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(true);
    // 隐藏购物车页
    if (cart) cart->hide();
    // 请求列表第一页
    requestProductsPage(1);
}

void MainWindow::showCartView()
{
    setTabActive("cart");
    setSearchBarVisible(false);
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(false);
    if (promoTimer) promoTimer->stop();
    if (homeHeaderImage) homeHeaderImage->setVisible(false);
    // 进入购物车时同样直接销毁全屏页
    if (auto p = findChild<QWidget*>("accountPage")) { p->hide(); p->deleteLater(); }
    if (auto p = findChild<QWidget*>("ordersPage")) { p->hide(); p->deleteLater(); }
    if (auto p = findChild<QWidget*>("chatPage")) { p->hide(); p->deleteLater(); }
    if (chat) { chat->deleteLater(); chat = nullptr; }
    // 离开首页，停止轮播
    if (carouselTimer) { carouselTimer->stop(); }
    // 创建或重用购物车，并嵌入主布局区域
    if (!cart) {
        cart = new ShoppingCart(socket, this);
        if (!currentUsername.isEmpty()) cart->setUsername(currentUsername);
        // 嵌入到 rootLayout 的末尾（在底部分页之前或之后均可，这里放在分页下方同级，显示时隐藏其他区域）
        if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
            root->addWidget(cart);
        } else {
            // 回退：直接设置父子关系
            cart->setParent(this);
        }
        if (auto backBtn = cart->findChild<QPushButton*>("backButton")) {
            connect(backBtn, &QPushButton::clicked, this, &MainWindow::exitCartView);
        }
    }
    currentView = ViewMode::Cart;
    // 隐藏首页/商城区域和全局分页器
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(false);
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 显示购物车页面并尽量铺满主界面区域
    cart->setVisible(true);
    cart->raise();
}

void MainWindow::exitCartView()
{
    if (cart) cart->hide();
    // 返回之前的非购物车视图
    if (lastNonCartView == ViewMode::Mall) {
        showMallView();
    } else {
        showHomeView();
    }
}

void MainWindow::clearToFullPage(QWidget *page)
{
    if (!page) return;
    // 隐藏首页/商城区域、全局分页器、问候标题与活动文案
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(false);
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(false);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(false);
    if (promoTimer) promoTimer->stop();
    // 非首页/发现好物页时，隐藏搜索栏
    setSearchBarVisible(false);
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    if (cart) cart->hide();
    // 显示全屏页时停止轮播
    if (carouselTimer) { carouselTimer->stop(); }
    // 直接销毁其他全屏页，防止遗留信号/回调导致闪退
    if (auto oldOrders = findChild<QWidget*>("ordersPage")) { if (oldOrders != page) { oldOrders->hide(); oldOrders->deleteLater(); } }
    if (auto oldAccount = findChild<QWidget*>("accountPage")) { if (oldAccount != page) { oldAccount->hide(); oldAccount->deleteLater(); } }
    if (auto oldChat = findChild<QWidget*>("chatPage")) { if (oldChat != page) { oldChat->hide(); oldChat->deleteLater(); } }
    // 注意：聊天被内嵌在 chatPage 内部，这里不要额外隐藏 chat，否则会出现“只有页面没有内容”
    // 添加并显示当前页
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        if (page->parent() != this && page->parent() != ui->centralwidget) page->setParent(this);
        // 避免重复添加：如果已经在 root 中，则不再 addWidget
        bool alreadyIn = false;
        for (int i=0;i<root->count();++i) {
            if (root->itemAt(i)->widget() == page) { alreadyIn = true; break; }
        }
        if (!alreadyIn) root->addWidget(page);
    }
    page->show();
    page->raise();
}

void MainWindow::updateGreeting()
{
    if (auto lbl = findChild<QLabel*>("greetingLabel")) {
        if (!currentUsername.isEmpty()) lbl->setText(tr("你好！%1").arg(currentUsername));
        else lbl->setText(tr("你好！"));
    }
}

void MainWindow::showAccountView()
{
    setTabActive("account");
    setSearchBarVisible(false);
    // 若不存在，先创建一个占位的 accountPage，并使用 clearToFullPage 显示
    QWidget *page = findChild<QWidget*>("accountPage");
    if (!page) {
        page = new QWidget(this);
        page->setObjectName("accountPage");
        auto *v = new QVBoxLayout(page);
        auto *topBar = new QHBoxLayout();
        auto *title = new QLabel(tr("个人中心"), page); title->setStyleSheet("font-weight:600;font-size:16px;");
        auto *back = new QPushButton(tr("返回"), page);
        auto *saveTop = new QPushButton(tr("修改"), page); saveTop->setObjectName("saveTopButton");
        topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(saveTop); topBar->addWidget(back); v->addLayout(topBar);
        // 仅在尚未创建表单时显示占位
        auto *info = new QLabel(tr("正在加载账号信息…"), page);
        info->setObjectName("acc_loading");
        v->addWidget(info);
        if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(page);
            connect(back, &QPushButton::clicked, this, [this, page]{ page->hide(); page->deleteLater(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });
    }
    // 已有页面：若已存在表单，隐藏占位；否则确保占位显示
    if (page->findChild<QLineEdit*>("acc_user")) {
        if (auto info = page->findChild<QLabel*>("acc_loading", Qt::FindDirectChildrenOnly)) info->hide();
    } else {
        if (auto info = page->findChild<QLabel*>("acc_loading", Qt::FindDirectChildrenOnly)) { info->setText(tr("正在加载账号信息…")); info->show(); }
    }
    // 进入账号页时强制把顶部按钮文案设为“修改”
    if (auto saveToggle = page->findChild<QPushButton*>("saveTopButton")) saveToggle->setText(tr("修改"));
    clearToFullPage(page);
    // 请求账号信息，填充内容（重用已有请求）
    if (socket) {
        QJsonObject req; req["type"] = "get_account_info"; if (!currentUsername.isEmpty()) req["username"] = currentUsername; QJsonDocument d(req); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
    }
}

void MainWindow::showOrdersView()
{
    setTabActive("orders");
    setSearchBarVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    // 隐藏全局分页器，避免与订单分页重复
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 为避免遗留的旧 ordersPage 上的信号/控件导致再次进入时闪退，进入前先彻底销毁旧页面
    if (auto old = findChild<QWidget*>("ordersPage")) { old->hide(); old->deleteLater(); }
    // 发送请求；在 orders_response 分支中构建/展示内嵌页面
    if (!socket) return;
    QJsonObject req; req["type"] = "get_orders"; QJsonDocument d(req); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
    statusBar()->showMessage(tr("正在获取历史订单…"), 2000);
}

void MainWindow::showChatView()
{
    setTabActive("chat");
    setSearchBarVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    // 若已存在 chatPage，直接复用；ChatWindow 采用单例持有，避免 page 删除后异步回调访问已销毁控件
    if (!chat) chat = new ChatWindow(this);
    if (auto exist = findChild<QWidget*>("chatPage")) {
        // 已有页面则仅重新挂载 ChatWindow 并刷新
        chat->setParent(exist);
        chat->setWindowFlags(Qt::Widget);
        chat->setMinimumSize(0,300);
        if (socket) chat->setSocket(socket);
        if (!currentUsername.isEmpty()) chat->setUsername(currentUsername);
        chat->initChat();
        clearToFullPage(exist);
        return;
    }
    // 注入网络依赖与当前用户名
    if (socket) chat->setSocket(socket);
    if (!currentUsername.isEmpty()) chat->setUsername(currentUsername);
    QWidget *page = new QWidget(this);
    page->setObjectName("chatPage");
    auto *v = new QVBoxLayout(page);
    auto *topBar = new QHBoxLayout();
    auto *title = new QLabel(tr("客服聊天"), page); title->setStyleSheet("font-weight:600;font-size:16px;");
    auto *back = new QPushButton(tr("返回"), page);
    topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(back); v->addLayout(topBar);
    chat->setParent(page);
    chat->setWindowFlags(Qt::Widget);
    chat->setMinimumSize(0,300);
    v->addWidget(chat);
    connect(back, &QPushButton::clicked, this, [this, page]{
        // 隐藏并释放聊天页；ChatWindow 脱离父子关系以保持存活
        page->hide();
        if (chat) { chat->deleteLater(); chat = nullptr; }
        page->deleteLater();
        if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView();
    });
    clearToFullPage(page);
    // 进入聊天页时，拉取历史并刷新在线用户
    chat->initChat();
}

// 顶部搜索栏显隐：仅在首页/发现好物显示
void MainWindow::setSearchBarVisible(bool visible)
{
    if (auto w = findChild<QWidget*>("searchInput")) w->setVisible(visible);
    if (auto w = findChild<QWidget*>("searchButton")) w->setVisible(visible);
}
// 加载网络图片（带简单内存缓存），按目标尺寸等比缩放后设置到 QLabel
void MainWindow::setImageFromUrl(const QString &url, QLabel *label, const QSize &targetSize)
{
    if (!label) return;
    // 归一化：将相对路径或以 /images 开头的路径补齐为完整 URL
    const QString fullUrl = resolveHttpUrl(url);
    // 命中缓存
    if (imageCache.contains(fullUrl)) {
        label->setPixmap(scaledAspect(imageCache.value(fullUrl), targetSize));
        return;
    }
    QUrl qurl(fullUrl);
    if (!qurl.isValid() || qurl.scheme().isEmpty()) return;
    QNetworkRequest req(qurl);
    auto *reply = http->get(req);
    // 以 label 作为上下文对象，若 label 已销毁则此回调不会被触发，避免悬空指针
    connect(reply, &QNetworkReply::finished, label, [this, reply, fullUrl, targetSize, label]{
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Image load failed" << fullUrl << ":" << reply->errorString();
            if (label) label->setText(tr("图片加载失败"));
            return;
        }
        const QByteArray bytes = reply->readAll();
        QPixmap pm; if (!pm.loadFromData(bytes)) return;
        imageCache.insert(fullUrl, pm);
        if (label) label->setPixmap(scaledAspect(pm, targetSize));
    });
}

QPixmap MainWindow::scaledAspect(const QPixmap &src, const QSize &target)
{
    if (src.isNull() || !target.isValid()) return src;
    return src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// 等比扩展并居中裁剪（类似 CSS cover）
QPixmap MainWindow::scaledCover(const QPixmap &src, const QSize &target)
{
    if (src.isNull() || !target.isValid()) return src;
    // 按目标高度等比缩放，保证纵向完整显示；若宽度超出则仅左右裁剪
    const int tgtH = target.height();
    if (tgtH <= 0) return src;
    const qreal scale = static_cast<qreal>(tgtH) / qMax(1, src.height());
    const int newW = qMax(1, static_cast<int>(qRound(src.width() * scale)));
    const int newH = qMax(1, tgtH);
    QPixmap scaled = src.scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (newW <= target.width()) {
        // 宽度不足目标宽，直接返回（由 QLabel 居中对齐留白）
        return scaled;
    }
    // 宽度大于目标宽，水平居中裁剪，纵向不裁剪
    const int x = (newW - target.width()) / 2;
    return scaled.copy(x, 0, target.width(), newH);
}

QString MainWindow::resolveHttpUrl(const QString &url) const
{
    QString u = url.trimmed();
    if (u.isEmpty()) return u;
    // 已经是 http/https，直接返回
    if (u.startsWith("http://", Qt::CaseInsensitive) || u.startsWith("https://", Qt::CaseInsensitive)) return u;
    // 以 // 开头的协议相对 URL，补 http:
    if (u.startsWith("//")) return QStringLiteral("http:%1").arg(u);
    // 以 /images 或 images 开头的相对路径，拼上 httpBase
    if (u.startsWith("/")) {
        return httpBase.isEmpty() ? (QStringLiteral("http://localhost:8081") + u) : (httpBase + u);
    }
    if (u.startsWith("images/", Qt::CaseInsensitive)) {
        return httpBase.isEmpty() ? (QStringLiteral("http://localhost:8081/") + u) : (httpBase + "/" + u);
    }
    // 其他相对路径，按 images/ 子目录处理
    return httpBase.isEmpty() ? (QStringLiteral("http://localhost:8081/images/") + u)
                              : (httpBase + "/images/" + u);
}

// 根据当前窗口宽度，等比缩放本地图片并显示在首页问候语下方
void MainWindow::applyHomeHeaderImage()
{
    if (!homeHeaderImage) return;
    if (homeHeaderImagePath.isEmpty()) { homeHeaderImage->clear(); return; }
    // 如果原始图未加载或上一次失败，尝试从本地路径读取
    if (homeHeaderOriginal.isNull()) {
        QPixmap pm(QString::fromUtf8(homeHeaderImagePath.toUtf8()));
        if (!pm.isNull()) {
            homeHeaderOriginal = pm;
        } else {
            homeHeaderImage->setText(tr("无法加载图片"));
            return;
        }
    }
    // 计算目标尺寸：尽量铺满右侧内容区宽度，限制最大高度以避免过高
    int contentW = width();
    if (auto central = ui->centralwidget) contentW = central->width();
    // 右侧内容区大致是整体宽度减去左侧栏宽度（约 160-220px）与边距
    int targetW = qMax(200, contentW - 220);
    const int maxH = 300; // 提高首页头图最大高度，减少留白
    QPixmap scaled = scaledAspect(homeHeaderOriginal, QSize(targetW, maxH));
    homeHeaderImage->setPixmap(scaled);
    homeHeaderImage->setFixedHeight(scaled.height());
}

// 将当前索引的原图按 carouselArea 宽度等比缩放并显示
void MainWindow::refreshCarouselPixmap()
{
    if (!carouselImageLabel) return;
    if (carouselOriginals.isEmpty()) { carouselImageLabel->clear(); return; }
    const QPixmap &orig = carouselOriginals.at(qBound(0, carouselIndex, carouselOriginals.size()-1));
    if (orig.isNull()) { carouselImageLabel->setText(tr("图片不可用")); return; }
    int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
    int contentH = ui->carouselArea ? ui->carouselArea->height() : height();
    // 改为“铺满并居中裁剪”，消除左右留白；目标宽度与布局权重（约80%）一致
    int targetW = qMax(320, (contentW - 40) * 80 / 100);
    int maxH = qMax(220, (contentH > 0 ? (contentH * 72 / 100) : 360));
    QPixmap covered = scaledCover(orig, QSize(targetW, maxH));
    carouselImageLabel->setPixmap(covered);
    carouselImageLabel->setFixedWidth(targetW);
    carouselImageLabel->setFixedHeight(covered.height());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 左右侧预览点击
    if ((obj == carouselPrevLabel || obj == carouselNextLabel) && event->type() == QEvent::MouseButtonRelease) {
        if (carouselOriginals.isEmpty()) return false;
        if (obj == carouselPrevLabel) {
            carouselIndex = (carouselIndex - 1 + carouselOriginals.size()) % carouselOriginals.size();
        } else {
            carouselIndex = (carouselIndex + 1) % carouselOriginals.size();
        }
        // 刷新
        refreshCarouselPixmap();
        if (carouselPrevLabel && carouselNextLabel && !carouselOriginals.isEmpty()) {
            int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
            int sideW = qMax(110, (contentW - 40) * 10 / 100);
            int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
            const int maxSideH = qMax(110, mainH * 75 / 100);
            auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
            const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
            const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
            carouselPrevLabel->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
            carouselNextLabel->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
            carouselPrevLabel->setFixedWidth(sideW);
            carouselNextLabel->setFixedWidth(sideW);
            carouselPrevLabel->setFixedHeight(carouselPrevLabel->pixmap(Qt::ReturnByValue).height());
            carouselNextLabel->setFixedHeight(carouselNextLabel->pixmap(Qt::ReturnByValue).height());
        }
        updateCarouselDots();
        if (carouselTimer) { carouselTimer->stop(); carouselTimer->start(); }
        return true;
    }
    // 底部小圆点点击
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto *lbl = qobject_cast<QLabel*>(obj)) {
            bool ok = false; int idx = lbl->property("dotIndex").toInt(&ok);
            if (ok && idx >= 0 && idx < carouselOriginals.size()) {
                carouselIndex = idx;
                refreshCarouselPixmap();
                if (carouselPrevLabel && carouselNextLabel && !carouselOriginals.isEmpty()) {
                    int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
                    int sideW = qMax(110, (contentW - 40) * 10 / 100);
                    int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
                    const int maxSideH = qMax(110, mainH * 75 / 100);
                    auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
                    const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
                    const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
                    carouselPrevLabel->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
                    carouselNextLabel->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
                    carouselPrevLabel->setFixedWidth(sideW);
                    carouselNextLabel->setFixedWidth(sideW);
                    carouselPrevLabel->setFixedHeight(carouselPrevLabel->pixmap(Qt::ReturnByValue).height());
                    carouselNextLabel->setFixedHeight(carouselNextLabel->pixmap(Qt::ReturnByValue).height());
                }
                updateCarouselDots();
                if (carouselTimer) { carouselTimer->stop(); carouselTimer->start(); }
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::updateCarouselDots()
{
    for (int i=0; i<carouselDots.size(); ++i) {
        QLabel *d = carouselDots[i];
        if (!d) continue;
        if (i == carouselIndex) d->setStyleSheet("border-radius:5px;background:#1677ff;");
        else d->setStyleSheet("border-radius:5px;background:#ddd;");
    }
}