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
#include <QTabBar>
#include <QSignalBlocker>
#include <QComboBox>

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
        {"home", "首页"}, {"mall", "发现好物"}, {"cart", "购物车"}, {"orders", "历史订单"}, {"chat", "客服"}, {"account", "个人中心"}
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
            auto *pwd = new QLineEdit(page); pwd->setObjectName("acc_pwd"); pwd->setEchoMode(QLineEdit::Password);
            form->addRow(tr("账号ID"), idEdit);
            form->addRow(tr("用户名"), user);
            form->addRow(tr("手机号"), phone);
            form->addRow(tr("新密码"), pwd);
            v->addLayout(form);
            // 默认禁用编辑，点击“修改”后启用并把按钮文案变为“保存”
            user->setEnabled(false); phone->setEnabled(false); pwd->setEnabled(false);
            if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(page);
            // 行为
            connect(back, &QPushButton::clicked, this, [this, page]{ page->hide(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });
            auto sendSave = [this, user, phone, pwd]{
                QJsonObject r; r["type"] = "update_account_info";
                const QString newName = user->text().trimmed();
                const QString newPhone = phone->text().trimmed();
                const QString newPwd = pwd->text();
                if (!newName.isEmpty()) r["new_username"] = newName;
                if (!newPhone.isEmpty()) r["phone"] = newPhone;
                if (!newPwd.isEmpty()) r["password"] = newPwd;
                if (!currentUsername.isEmpty()) r["username"] = currentUsername;
                QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                statusBar()->showMessage(tr("正在保存修改…"), 2000);
            };
            // 单按钮切换：修改 <-> 保存
            connect(saveToggle, &QPushButton::clicked, this, [saveToggle, user, phone, pwd, sendSave]() mutable {
                if (saveToggle->text() == QObject::tr("修改")) {
                    user->setEnabled(true); phone->setEnabled(true); pwd->setEnabled(true);
                    user->setFocus();
                    saveToggle->setText(QObject::tr("保存"));
                } else {
                    sendSave();
                    user->setEnabled(false); phone->setEnabled(false); pwd->setEnabled(false);
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
                auto *pwd = new QLineEdit(page); pwd->setObjectName("acc_pwd"); pwd->setEchoMode(QLineEdit::Password);
                form->addRow(tr("账号ID"), idEdit);
                form->addRow(tr("用户名"), user);
                form->addRow(tr("手机号"), phone);
                form->addRow(tr("新密码"), pwd);
                v->addLayout(form);
                // 默认禁用编辑
                user->setEnabled(false); phone->setEnabled(false); pwd->setEnabled(false);
                if (auto saveToggle = page->findChild<QPushButton*>("saveTopButton")) {
                    QObject::disconnect(saveToggle, nullptr, nullptr, nullptr);
                    connect(saveToggle, &QPushButton::clicked, this, [saveToggle, user, phone, pwd, this]() mutable {
                        auto sendSave = [this, user, phone, pwd]{
                            QJsonObject r; r["type"] = "update_account_info";
                            const QString newName = user->text().trimmed();
                            const QString newPhone = phone->text().trimmed();
                            const QString newPwd = pwd->text();
                            if (!newName.isEmpty()) r["new_username"] = newName;
                            if (!newPhone.isEmpty()) r["phone"] = newPhone;
                            if (!newPwd.isEmpty()) r["password"] = newPwd;
                            if (!currentUsername.isEmpty()) r["username"] = currentUsername;
                            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); socket->write(p);
                            statusBar()->showMessage(tr("正在保存修改…"), 2000);
                        };
                        if (saveToggle->text() == QObject::tr("修改")) {
                            user->setEnabled(true); phone->setEnabled(true); pwd->setEnabled(true);
                            user->setFocus();
                            saveToggle->setText(QObject::tr("保存"));
                        } else {
                            sendSave();
                            user->setEnabled(false); phone->setEnabled(false); pwd->setEnabled(false);
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
        if (auto pwd = page->findChild<QLineEdit*>("acc_pwd"))
            pwd->clear();
        clearToFullPage(page);
    }
    else if (type == "update_account_response") {
        bool ok = response.value("success").toBool(); QString msg = response.value("message").toString();
        statusBar()->showMessage(ok ? (msg.isEmpty()? tr("保存成功"): msg) : (msg.isEmpty()? tr("保存失败"): msg), 3000);
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
        // 构建或复用内嵌订单页面（统一顶部返回栏 + 筛选 + 分页，样式统一）
    QWidget *container = findChild<QWidget*>("ordersPage");
    if (!container) container = new QWidget(this);
    container->setObjectName("ordersPage");
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
                const QString status = m.value("status").toString();
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
                table->setItem(r, 2, new QTableWidgetItem(QString::number(o.value("total_price").toDouble(), 'f', 2)));
                table->setItem(r, 3, new QTableWidgetItem(o.value("status").toString()));
                table->setItem(r, 4, new QTableWidgetItem(o.value("order_time").toString()));
                ++r;
            }
            pageInfo->setText(tr("第 %1 / %2 页 · 共 %3 条").arg(pageNo).arg(totalPages).arg(total));
            prevBtn->setEnabled(pageNo > 1);
            nextBtn->setEnabled(pageNo < totalPages);
        };

        // 交互
        connect(filterBtn, &QPushButton::clicked, this, [container, refresh]{ container->setProperty("ordersPageNo", 1); refresh(); });
        connect(kwEdit, &QLineEdit::returnPressed, this, [container, refresh]{ container->setProperty("ordersPageNo", 1); refresh(); });
    connect(statusCmb, qOverload<int>(&QComboBox::currentIndexChanged), this, [container, refresh](int){ container->setProperty("ordersPageNo", 1); refresh(); });
        connect(prevBtn, &QPushButton::clicked, this, [container, refresh]{ int p = container->property("ordersPageNo").toInt(); if (p>1) { container->setProperty("ordersPageNo", p-1); refresh(); } });
        connect(nextBtn, &QPushButton::clicked, this, [container, refresh]{ int p = container->property("ordersPageNo").toInt(); container->setProperty("ordersPageNo", p+1); refresh(); });

        // 双击表格行查看订单详情（小票样式）
        if (auto *tbl = container->findChild<QTableWidget*>("ordersTable")) {
            connect(tbl, &QTableWidget::itemDoubleClicked, this, [this, tbl](QTableWidgetItem *item){
                int row = item->row();
                auto *idItem = tbl->item(row, 0);
                if (!idItem) return;
                const QVariantMap order = idItem->data(Qt::UserRole).toMap();
                if (order.isEmpty()) return;
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
                double sum = 0.0;
                for (const QVariant &iv : items) {
                    const QVariantMap it = iv.toMap();
                    const QString name = it.value("name").toString();
                    const int qty = it.value("quantity").toInt();
                    const double price = it.value("price").toDouble();
                    const double sub = price * qty;
                    sum += sub;
                    html += QString("<tr><td style='padding:4px 0;'>%1</td>"
                                    "<td style='text-align:right;padding:4px 0;'>%2</td>"
                                    "<td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%3</td>"
                                    "<td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%4</td></tr>")
                                .arg(name.toHtmlEscaped())
                                .arg(qty)
                                .arg(QString::number(price, 'f', 2))
                                .arg(QString::number(sub, 'f', 2));
                }
                html += QString("<tr><td colspan='4' style='border-top:1px solid #eee;padding-top:6px;text-align:right;font-weight:700;'>合计：&yen;&nbsp;%1</td></tr>")
                        .arg(QString::number(sum, 'f', 2));
                html += "</table>";

                QMessageBox box(this);
                box.setWindowTitle(tr("订单详情"));
                box.setTextFormat(Qt::RichText);
                box.setText(html);
                box.addButton(tr("关闭"), QMessageBox::RejectRole);
                box.exec();
            });
        }

        // 返回
    connect(back, &QPushButton::clicked, this, [this, container]{ container->hide(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });

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
    // 首页标题行：精选折扣商品  +  更多 >
    {
        auto *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0,0,0,0);
        auto *title = new QLabel(tr("首页精选折扣商品"), container);
        title->setWordWrap(true);
        title->setStyleSheet("font-weight:600;margin:6px 0;");
        titleRow->addWidget(title);
        titleRow->addStretch(1);
        auto *moreBtn = new QPushButton(tr("更多 >"), container);
        moreBtn->setObjectName("homeMoreDiscountBtn");
        moreBtn->setFlat(true);
        // 改为白字蓝底，保持 hover 有轻微高亮
        moreBtn->setStyleSheet(
            "QPushButton{color:#ffffff;background:#1677ff;border:1px solid #1677ff;border-radius:6px;padding:2px 10px;}"
            "QPushButton:hover{background:#3a8cff;border-color:#3a8cff;}"
        );
    titleRow->addWidget(moreBtn);
    // 将标题行放入一个容器后添加到父布局
    auto *titleHost = new QWidget(container);
    titleHost->setLayout(titleRow);
    layout->addWidget(titleHost);
        // 点击“更多”进入商城视图
        connect(moreBtn, &QPushButton::clicked, this, [this]{ showMallView(); });
    }
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

        // 右上角角标：优先显示“售罄”（库存=0），否则若 isNew=true 显示“新品”（绿色）
        bool showSoldOut = (stock == 0);
        bool showNew = (!showSoldOut && o.value("isNew").toBool());
        if (showSoldOut || showNew) {
            auto *badgeRow = new QHBoxLayout();
            badgeRow->setContentsMargins(0,0,0,0);
            badgeRow->addStretch(1);
            auto *badge = new QLabel(showSoldOut ? tr("售罄") : tr("新品"), card);
            if (showSoldOut) badge->setStyleSheet("QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            else badge->setStyleSheet("QLabel{background:#34A853;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
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
        // 销量标签（若返回了 sales 字段）
        if (o.contains("sales")) {
            const int sales = o.value("sales").toInt();
            auto *salesLbl = new QLabel(tr("销量 %1").arg(sales), card);
            salesLbl->setStyleSheet("color:#666;font-size:12px;");
            vbox->addWidget(salesLbl);
        }
        vbox->addStretch(1); // 将按钮推到底部
        vbox->addLayout(btnRow); // 把按钮行加入布局，防止浮动遮挡

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
    // 可选描述；将属性改为竖排逐行展示
    const QString desc = product.value("description").toString();
    QString text;
    text += QString("<div><b>名称：</b>%1</div>").arg(name.toHtmlEscaped());
    if (!desc.isEmpty()) {
        text += QString("<div><b>描述：</b>%1</div>").arg(desc.toHtmlEscaped());
    }
    text += QString("<div><b>价格：</b>%1</div>").arg(priceLine);
    if (stock >= 0) text += QString("<div><b>库存：</b>%1</div>").arg(stock);
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
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(true);
    updateGreeting();
    if (ui->carouselArea) ui->carouselArea->setVisible(true);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(true);
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
    loadCarousel();
    loadPromotions();
    loadRecommendations();
}

void MainWindow::showMallView()
{
    currentView = ViewMode::Mall;
    lastNonCartView = ViewMode::Mall;
    setTabActive("mall");
    setSearchBarVisible(true);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
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
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
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
    // 隐藏首页/商城区域、全局分页器、问候标题
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(false);
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
    // 隐藏其他全屏页（不 deleteLater，避免频繁构建造成卡顿）
    if (auto oldOrders = findChild<QWidget*>("ordersPage")) { if (oldOrders != page) oldOrders->hide(); }
    if (auto oldAccount = findChild<QWidget*>("accountPage")) { if (oldAccount != page) oldAccount->hide(); }
    if (auto oldChat = findChild<QWidget*>("chatPage")) { if (oldChat != page) oldChat->hide(); }
    // 注意：聊天被内嵌在 chatPage 内部，这里不要额外隐藏 chat，否则会出现“只有页面没有内容”
    // 添加并显示当前页
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        if (page->parent() != this) page->setParent(this);
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
        connect(back, &QPushButton::clicked, this, [this, page]{ page->hide(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });
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
    // 若已存在 chatPage，直接复用
    if (auto exist = findChild<QWidget*>("chatPage")) {
        clearToFullPage(exist);
        return;
    }
    if (!chat) chat = new ChatWindow(this);
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
    connect(back, &QPushButton::clicked, this, [this, page]{ page->hide(); page->deleteLater(); if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView(); });
    clearToFullPage(page);
}

// 顶部搜索栏显隐：仅在首页/发现好物显示
void MainWindow::setSearchBarVisible(bool visible)
{
    if (auto w = findChild<QWidget*>("searchInput")) w->setVisible(visible);
    if (auto w = findChild<QWidget*>("searchButton")) w->setVisible(visible);
}