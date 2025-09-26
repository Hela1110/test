#include "shopping/shoppingcart.h"
#include "ui_shoppingcart.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHash>
#include <QPointer>
#include <QDebug>
#include <QTimer>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QSpinBox>
#include <QMouseEvent>
#include <QCheckBox>
#include <QHeaderView>
#include <QSignalBlocker>

// 全局（文件级）弹窗占用标记，防止同一时刻出现多个消息框
static QPointer<QMessageBox> s_activePopup;
// 全局静默期时间戳（ms），用于抑制短时间内连续不同 tag 的二次弹窗
static qint64 s_globalLastPopupMs = -1;

ShoppingCart::ShoppingCart(QTcpSocket *socket, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ShoppingCart),
    socket(socket)
{
    ui->setupUi(this);
    // 初始化时记录关键控件是否存在，辅助判断 auto-connect 是否可用
    auto checkoutBtn = findChild<QPushButton*>("checkoutButton");
    auto deleteBtn = findChild<QPushButton*>("batchDeleteButton");
    auto totalLbl = findChild<QLabel*>("totalPriceLabel");
    auto table = findChild<QTableWidget*>("cartTable");
    qInfo() << "ShoppingCart ctor: checkoutButton?" << (checkoutBtn!=nullptr)
            << "deleteButton?" << (deleteBtn!=nullptr)
            << "totalLabel?" << (totalLbl!=nullptr)
            << "cartTable?" << (table!=nullptr);
    // 启动单调计时器用于轻量节流
    monotonic.start();
    setupUi();
    loadCartItems();
}

ShoppingCart::~ShoppingCart()
{
    delete ui;
}

void ShoppingCart::setupUi()
{
    // 设置表格属性：关闭原生多选，首列自定义复选圆点
    if (ui->cartTable) {
        ui->cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->cartTable->setSelectionMode(QAbstractItemView::NoSelection); // 使用自定义勾选交互
        ui->cartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->cartTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui->cartTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->cartTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        ui->cartTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        ui->cartTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    }
    
    // 槽函数已按 on_* 命名，Qt 会通过 QMetaObject::connectSlotsByName 自动连接
    // 无需手动 connect，以免重复触发导致弹窗两次
    if (objectName().isEmpty()) setObjectName("ShoppingCart");
    // 通过 findChild 再次确认关键控件（统一不再重复绑定 lambda，避免 UniqueConnection 警告）
    if (auto btn = findChild<QPushButton*>("checkoutButton")) {
        QObject::disconnect(btn, nullptr, this, nullptr);
        connect(btn, &QPushButton::clicked, this, &ShoppingCart::on_checkoutButton_clicked);
    }
    // delete 按钮可能命名为 deleteButton 或 batchDeleteButton，做兼容
    if (auto delBtn = findChild<QPushButton*>("deleteButton")) {
        QObject::disconnect(delBtn, nullptr, this, nullptr);
        connect(delBtn, &QPushButton::clicked, this, &ShoppingCart::on_deleteButton_clicked);
    } else if (auto delBtn2 = findChild<QPushButton*>("batchDeleteButton")) {
        QObject::disconnect(delBtn2, nullptr, this, nullptr);
        connect(delBtn2, &QPushButton::clicked, this, &ShoppingCart::on_deleteButton_clicked);
    }
    if (auto clrBtn = findChild<QPushButton*>("clearButton")) {
        QObject::disconnect(clrBtn, nullptr, this, nullptr);
        connect(clrBtn, &QPushButton::clicked, this, [this]{
            if (QMessageBox::question(this, tr("清空购物车"), tr("确定要清空购物车吗？")) != QMessageBox::Yes) return;
            // 1. 立即本地清空（乐观 UI）
            {
                QSignalBlocker b(ui->cartTable);
                cartItems.clear();
                if (ui->cartTable) { ui->cartTable->setRowCount(0); ui->cartTable->clearContents(); }
            }
            if (auto all = findChild<QCheckBox*>("selectAllCheck")) { QSignalBlocker bb(*all); all->setCheckState(Qt::Unchecked); }
            updateSelectedCount();
            updateTotalPrice();
            qInfo() << "[clear_cart] optimistic cleared locally username=" << username;
            // 2. 发送服务器请求
            QJsonObject req; req["type"] = "clear_cart"; if (!username.isEmpty()) req["username"] = username; sendRequest(req);
            // 3. 1 秒后强制刷新一次防止服务器侧失败不同步（失败会重新拉回数据）
            QTimer::singleShot(1000, this, [this]{ loadCartItems(); });
        });
        clrBtn->setVisible(true);
        clrBtn->setEnabled(true);
    }
    ensureSelectAllHook();
}

void ShoppingCart::loadCartItems()
{
    QJsonObject request;
    request["type"] = "get_cart";
    if (!username.isEmpty()) request["username"] = username;
    sendRequest(request);
}

void ShoppingCart::handleMessage(const QJsonObject &response)
{
    const QString type = response.value("type").toString();
    if (type == QLatin1String("cart_items") || type == QLatin1String("cart_response")) {
        // 生成签名：items 数量 + 每个 product_id:qty 排序拼接，避免重复构建 UI
        QStringList sigParts;
        QJsonArray itemsRaw = response.value("items").toArray();
        for (const auto &v : itemsRaw) {
            QJsonObject o = v.toObject();
            int pid = o.contains("product_id") ? o.value("product_id").toInt() : o.value("productId").toInt();
            int qty = o.value("quantity").toInt();
            sigParts << QString::number(pid) + ":" + QString::number(qty);
        }
        std::sort(sigParts.begin(), sigParts.end());
        QString signature = QString::number(sigParts.size()) + "|" + sigParts.join(",");
        qint64 nowMs = monotonic.isValid()? monotonic.elapsed():0;
        if (signature == lastCartSignature && (nowMs - lastCartBuildMs) < 800) {
            qInfo() << "[cart_load] skip duplicate build sig=" << signature;
            return; // 忽略重复刷新
        }
        lastCartSignature = signature; lastCartBuildMs = nowMs;
        QSignalBlocker blocker(ui->cartTable); // 填充表格时屏蔽触发
        ui->cartTable->clearContents();
    ui->cartTable->setRowCount(0);
        cartItems.clear();

        QJsonArray items = response.value("items").toArray();
        for (const QJsonValue &value : items) {
            QJsonObject item = value.toObject();
            // 若是 cart_response（字段 productId），转为现有字段
            if (item.contains("productId") && !item.contains("product_id")) {
                item.insert("product_id", item.take("productId").toInt());
            }
            cartItems.append(item);

            int row = ui->cartTable->rowCount();
            ui->cartTable->insertRow(row);

            const double price = item.value("price").toDouble();
            const int quantity = item.value("quantity").toInt();
            const int stock = item.value("stock").toInt(-1);
            const int pid = item.value("product_id").toInt();

            // 选择列：圆形样式的复选框（注意父子关系：chk 挂到 wrap，便于 wrap->findChild 查找）
            QWidget *wrap = new QWidget(ui->cartTable);
            auto *chk = new QCheckBox(wrap);
            chk->setTristate(false);
            chk->setChecked(false);
            // 圆形效果（样式表）
            chk->setStyleSheet("QCheckBox::indicator{width:18px;height:18px;border-radius:9px;border:1px solid #aaa;}"
                               "QCheckBox::indicator:checked{background:#1677ff;border-color:#1677ff;}"
                               "QCheckBox::indicator:unchecked{background:transparent;}");
            auto *hl = new QHBoxLayout(wrap); hl->setContentsMargins(6,0,6,0); hl->addWidget(chk); hl->addStretch();
            ui->cartTable->setCellWidget(row, 0, wrap);

            ui->cartTable->setItem(row, 1, new QTableWidgetItem(item.value("name").toString()));
            // 单价列：若存在促销（onSale 且 discountPrice < listPrice），显示 原价(划线灰)+红色折扣价
            {
                const double listPrice = item.contains("listPrice") ? item.value("listPrice").toDouble() : price; // 回退
                const bool onSale = item.value("onSale").toBool();
                const double dprice = item.contains("discountPrice") ? item.value("discountPrice").toDouble() : 0.0;
                const bool hasDiscount = onSale && dprice > 0.0 && dprice < listPrice;
                QWidget *cell = new QWidget(ui->cartTable);
                auto *v = new QVBoxLayout(cell); v->setContentsMargins(2,2,2,2); v->setSpacing(0);
                if (hasDiscount) {
                    auto *orig = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(listPrice, 'f', 2), cell);
                    orig->setStyleSheet("color:#999;text-decoration:line-through;font-size:12px;");
                    auto *disc = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(dprice, 'f', 2), cell);
                    disc->setStyleSheet("color:#E53935;font-weight:700;");
                    v->addWidget(orig);
                    v->addWidget(disc);
                } else {
                    auto *only = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(price, 'f', 2), cell);
                    v->addWidget(only);
                }
                ui->cartTable->setCellWidget(row, 2, cell);
            }
            // 数量使用 SpinBox，带上下箭头
            auto *spin = new QSpinBox(ui->cartTable);
            int maxQty = (stock >= 0 ? qMax(0, stock) : 9999);
            spin->setRange(0, maxQty);
            spin->setValue(quantity);
            ui->cartTable->setCellWidget(row, 3, spin);
            // 小计
            // 小计列：若有折扣，按折扣价计算，并同样展示双价（按件价×数量）
            {
                const double listPrice = item.contains("listPrice") ? item.value("listPrice").toDouble() : price;
                const bool onSale = item.value("onSale").toBool();
                const double dprice = item.contains("discountPrice") ? item.value("discountPrice").toDouble() : 0.0;
                const bool hasDiscount = onSale && dprice > 0.0 && dprice < listPrice;
                const double unit = hasDiscount ? dprice : price;
                QWidget *cell = new QWidget(ui->cartTable);
                auto *v = new QVBoxLayout(cell); v->setContentsMargins(2,2,2,2); v->setSpacing(0);
                if (hasDiscount) {
                    auto *orig = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(listPrice * quantity, 'f', 2), cell);
                    orig->setStyleSheet("color:#999;text-decoration:line-through;font-size:12px;");
                    auto *disc = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(unit * quantity, 'f', 2), cell);
                    disc->setStyleSheet("color:#E53935;font-weight:700;");
                    v->addWidget(orig);
                    v->addWidget(disc);
                } else {
                    auto *only = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(unit * quantity, 'f', 2), cell);
                    v->addWidget(only);
                }
                ui->cartTable->setCellWidget(row, 4, cell);
            }

            // 连接数量变更：立即发送 set_cart_quantity，并本地更新小计/总计与 cartItems
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, pid, price, row](int val){
                // 发送到服务器
                QJsonObject req;
                req["type"] = "set_cart_quantity";
                req["product_id"] = pid;
                req["quantity"] = val;
                sendRequest(req);
                // 本地更新：小计 & cartItems 中对应项数量
                if (ui->cartTable && row >= 0 && row < ui->cartTable->rowCount()) {
                    // 重新渲染该行的小计单元，以便折扣价变化生效
                    // 读取该行对应的数据项（含 listPrice/onSale/discountPrice）
                    double listPrice = price;
                    bool onSale = false; double dprice = 0.0;
                    if (row >= 0 && row < cartItems.size()) {
                        const auto it = cartItems[row];
                        listPrice = it.contains("listPrice") ? it.value("listPrice").toDouble() : (it.value("price").toDouble());
                        onSale = it.value("onSale").toBool();
                        dprice = it.contains("discountPrice") ? it.value("discountPrice").toDouble() : 0.0;
                    }
                    const bool hasDiscount = onSale && dprice > 0.0 && dprice < listPrice;
                    const double unit = hasDiscount ? dprice : price;
                    QWidget *cell = new QWidget(ui->cartTable);
                    auto *v = new QVBoxLayout(cell); v->setContentsMargins(2,2,2,2); v->setSpacing(0);
                    if (hasDiscount) {
                        auto *orig = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(listPrice * val, 'f', 2), cell);
                        orig->setStyleSheet("color:#999;text-decoration:line-through;font-size:12px;");
                        auto *disc = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(unit * val, 'f', 2), cell);
                        disc->setStyleSheet("color:#E53935;font-weight:700;");
                        v->addWidget(orig);
                        v->addWidget(disc);
                    } else {
                        auto *only = new QLabel(QString::fromUtf8("\xC2\xA5 ") + QString::number(unit * val, 'f', 2), cell);
                        v->addWidget(only);
                    }
                    ui->cartTable->setCellWidget(row, 4, cell);
                }
                for (auto &ci : cartItems) {
                    if (ci.value("product_id").toInt() == pid) { ci["quantity"] = val; break; }
                }
                updateTotalPrice();
            });

            // 选择变化联动总价与“已选择X项”
            connect(chk, &QCheckBox::toggled, this, [this](bool){ updateTotalPrice(); updateSelectedCount(); });

            // 单击行（除数量列）即切换选中
            ui->cartTable->setRowHeight(row, 44);
            for (int c = 0; c < ui->cartTable->columnCount(); ++c) {
                if (c == 3) continue; // 数量列不拦截
                auto *it = ui->cartTable->item(row, c);
                if (!it) { it = new QTableWidgetItem(); ui->cartTable->setItem(row, c, it); }
            }
        }
        blocker.unblock();
        // 默认全选，便于直接看到总计与已选择数量
        if (ui->cartTable && ui->cartTable->rowCount() > 0) {
            if (auto all = findChild<QCheckBox*>("selectAllCheck")) {
                QSignalBlocker b(*all);
                all->setChecked(true); // 默认全选，但本次不触发回调，下面手动刷新
            }
            // 手动执行一次全选逻辑
            const int rows = ui->cartTable->rowCount();
            for (int r = 0; r < rows; ++r) {
                QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                if (!wrap) continue;
                auto chk = wrap->findChild<QCheckBox*>();
                if (chk) { chk->setChecked(true); }
            }
            qInfo() << "[cart_load] populated rows=" << ui->cartTable->rowCount() << "all selected initially";
        } else {
            qInfo() << "[cart_load] empty cart";
        }
        updateSelectedCount();
        updateTotalPrice();
        // 安装事件过滤器以支持点击行切换选中
        ui->cartTable->viewport()->installEventFilter(this);
        ensureSelectAllHook(); // 确保钩子存在（避免第一次加载时尚未创建）
    } else if (type == QLatin1String("checkout_response")) {
        // 仅在一次有效结算流程中处理响应，避免重复弹窗
        if (!checkoutInFlight) {
            return;
        }

        checkoutInFlight = false; // 无论成功失败，结束本次流程
        if (response.value("success").toBool()) {
            // 结算成功提示应仅统计本次勾选项金额
            double total = 0;
            if (ui->cartTable) {
                const int rows = ui->cartTable->rowCount();
                for (int r = 0; r < rows; ++r) {
                    QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                    if (!wrap) continue;
                    auto chk = wrap->findChild<QCheckBox*>();
                    if (chk && chk->isChecked()) {
                        if (r >=0 && r < cartItems.size()) {
                            total += cartItems[r].value("price").toDouble() * cartItems[r].value("quantity").toInt();
                        }
                    }
                }
            }
            showOnce("checkout_ok", QMessageBox::Information, "结算成功", QString("本次金额：￥%1").arg(QString::number(total, 'f', 2)));
            emit checkoutCompleted();
            // 结算成功后从购物车中移除已结算的商品
            // 逐个发送 remove_from_cart 请求
            if (ui->cartTable) {
                const int rows = ui->cartTable->rowCount();
                for (int r = 0; r < rows; ++r) {
                    QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                    if (!wrap) continue;
                    auto chk = wrap->findChild<QCheckBox*>();
                    if (chk && chk->isChecked()) {
                        if (r >=0 && r < cartItems.size()) {
                            QJsonObject request; request["type"] = "remove_from_cart"; request["product_id"] = cartItems[r]["product_id"]; if (!username.isEmpty()) request["username"] = username; sendRequest(request);
                            qInfo() << "[checkout_response] remove_from_cart pid=" << cartItems[r]["product_id"].toInt() << "username=" << username;
                        }
                    }
                }
            }
            QTimer::singleShot(300, this, [this]{ loadCartItems(); });
        } else {
            const QString msg = response.value("message").toString();
            // 若服务器消息包含“购物车为空”，与本地空车提示统一到同一 tag，避免双弹
            const QString tag = (msg.contains(QLatin1String("购物车为空"))
                                  ? QLatin1String("empty_cart")
                                  : QLatin1String("checkout_fail"));
            // 将去重窗口调大到 5s，避免网络返回稍晚导致的二次弹窗
            showOnce(tag, QMessageBox::Warning, "结算失败", msg, 5000);
        }
    } else if (type == QLatin1String("order_response")) {
        // create_order 的返回（新协议）。这里也要执行“移除已结算项”的逻辑
        checkoutInFlight = false;
        const bool ok = response.value("success").toBool();
        if (ok) {
            const auto orderId = response.value("orderId").toVariant().toLongLong();
            // 只统计本次勾选项金额
            double total = 0;
            if (ui->cartTable) {
                const int rows = ui->cartTable->rowCount();
                for (int r = 0; r < rows; ++r) {
                    QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                    if (!wrap) continue;
                    auto chk = wrap->findChild<QCheckBox*>();
                    if (chk && chk->isChecked()) {
                        if (r >= 0 && r < cartItems.size()) {
                            total += cartItems[r].value("price").toDouble() * cartItems[r].value("quantity").toInt();
                        }
                    }
                }
            }
            showOnce("order_ok", QMessageBox::Information, "下单成功", QString("订单已创建：#%1\n本次金额：￥%2").arg(orderId).arg(QString::number(total, 'f', 2)));
            emit checkoutCompleted();
            // 同步移除本次勾选的条目
            if (ui->cartTable) {
                const int rows = ui->cartTable->rowCount();
                for (int r = 0; r < rows; ++r) {
                    QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                    if (!wrap) continue;
                    auto chk = wrap->findChild<QCheckBox*>();
                    if (chk && chk->isChecked()) {
                        if (r >= 0 && r < cartItems.size()) {
                            QJsonObject req; req["type"] = "remove_from_cart"; req["product_id"] = cartItems[r]["product_id"]; if (!username.isEmpty()) req["username"] = username; sendRequest(req);
                        }
                    }
                }
            }
            QTimer::singleShot(300, this, [this]{ loadCartItems(); });
        } else {
            const int code = response.value("code").toInt();
            const QString msg = response.value("message").toString();
            showOnce(QString("order_fail_%1").arg(code), QMessageBox::Warning, "下单失败", msg, 5000);
            // 若失败且可能是服务器不支持 create_order，则尝试回退到旧协议
            fallbackToLegacyCheckoutIfPossible(msg);
        }
    } else if (type == QLatin1String("error")) {
        // 统一的错误通道（MainWindow 已转发），若在结算中收到错误，也结束流程并考虑回退
        const QString msg = response.value("message").toString();
        if (checkoutInFlight) {
            checkoutInFlight = false;
            showOnce("checkout_error", QMessageBox::Warning, "结算失败", msg, 4000);
            fallbackToLegacyCheckoutIfPossible(msg);
        }
    } else if (type == QLatin1String("clear_cart_response")) {
        if (response.value("success").toBool()) {
            // 立即清空本地 UI，提升操作反馈
            QSignalBlocker blocker(ui->cartTable);
            cartItems.clear();
            if (ui->cartTable) { ui->cartTable->setRowCount(0); ui->cartTable->clearContents(); }
            if (auto all = findChild<QCheckBox*>("selectAllCheck")) {
                QSignalBlocker b(*all);
                all->setCheckState(Qt::Unchecked);
            }
            qInfo() << "[clear_cart_response] success cart cleared username=" << username;
            updateSelectedCount();
            updateTotalPrice();
        }
    } else if (type == QLatin1String("error")) {
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
        showOnce(QString("err_%1").arg(code), QMessageBox::Warning, tr("错误"), friendly + (msg.isEmpty()? QString(): (" - " + msg)), 4000);
    }
}

void ShoppingCart::updateTotalPrice()
{
    double totalOriginal = 0.0;   // 原价合计（黑色）
    double totalEffective = 0.0;  // 折后合计（红色；若无折扣则与原价相同）
    bool anyDiscount = false;
    // 仅统计被勾选的行；若没有任何行被勾选，则为 0
    if (ui->cartTable) {
        const int rows = ui->cartTable->rowCount();
        for (int r = 0; r < rows; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk && chk->isChecked()) {
                if (r >=0 && r < cartItems.size()) {
                    const auto &it = cartItems[r];
                    const int qty = it.value("quantity").toInt();
                    const double listPrice = it.contains("listPrice") ? it.value("listPrice").toDouble() : it.value("price").toDouble();
                    const bool onSale = it.value("onSale").toBool();
                    const double dprice = it.contains("discountPrice") ? it.value("discountPrice").toDouble() : 0.0;
                    const bool hasDiscount = onSale && dprice > 0.0 && dprice < listPrice;
                    const double unit = hasDiscount ? dprice : it.value("price").toDouble(); // 价格快照
                    totalOriginal += listPrice * qty;
                    totalEffective += unit * qty;
                    anyDiscount = anyDiscount || hasDiscount;
                }
            }
        }
    }
    // 找到标签（兼容两种命名）
    QLabel *lbl = findChild<QLabel*>("totalLabel");
    if (!lbl) lbl = findChild<QLabel*>("totalPriceLabel");
    if (lbl) {
        lbl->setTextFormat(Qt::RichText);
        // 若存在折扣且折后合计小于原价，则显示双价；否则仅显示一个黑色总计
        if (anyDiscount && (totalEffective + 1e-6) < totalOriginal) {
            const QString orig = QString::number(totalOriginal, 'f', 2);
            const QString disc = QString::number(totalEffective, 'f', 2);
            const QString html = QString("总计: <span style='color:#999;text-decoration:line-through;'>￥%1</span>  <span style='color:#E53935;font-weight:700;'>￥%2</span>")
                                  .arg(orig, disc);
            lbl->setText(html);
        } else {
            lbl->setText(QString("总计: ￥%1").arg(QString::number(totalEffective, 'f', 2)));
        }
    }
}

void ShoppingCart::on_checkoutButton_clicked()
{
    qInfo() << "checkoutButton clicked, itemsEmpty=" << cartItems.isEmpty() << "inFlight=" << checkoutInFlight;
    // 空车提示做轻量防抖，避免短时间重复点击产生多次弹窗
    if (cartItems.isEmpty()) {
        const qint64 now = monotonic.isValid() ? monotonic.elapsed() : 0;
        if (now - lastCheckoutMs < 400) {
            qInfo() << "checkout suppressed by local debounce";
            return;
        }
        lastCheckoutMs = now;
    // 将空车提示的去重窗口调大到 5s（与服务器返回保持一致）
    showOnce("empty_cart", QMessageBox::Warning, "结算失败", "购物车为空", 5000);
        // 暂时禁用按钮 800ms，进一步抑制双击造成的重复触发
        if (auto btn = findChild<QPushButton*>("checkoutButton")) {
            btn->setEnabled(false);
            QTimer::singleShot(800, btn, [btn]{ btn->setEnabled(true); });
        }
        return;
    }

    // 正在结算中则忽略后续点击
    if (checkoutInFlight) {
        return;
    }

    // 只结算勾选的项目；无勾选则提示
    QList<int> selectedRows;
    if (ui->cartTable) {
        const int rows = ui->cartTable->rowCount();
        for (int r = 0; r < rows; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk && chk->isChecked()) selectedRows.append(r);
        }
    }
    if (selectedRows.isEmpty()) {
        showOnce("checkout_no_selection", QMessageBox::Warning, "结算失败", "请勾选需要结算的商品", 3000);
        return;
    }
    QJsonArray arr;
    for (int row : selectedRows) {
        if (row < 0 || row >= cartItems.size()) continue;
        const auto &it = cartItems[row];
        QJsonObject o; o["productId"] = it.value("product_id").toInt();
        o["quantity"] = it.value("quantity").toInt(); if (o["quantity"].toInt() < 1) o["quantity"] = 1;
        arr.append(o);
    }
    QJsonObject request;
    request["type"] = "create_order";
    request["items"] = arr;
    if (!username.isEmpty()) request["username"] = username;
    checkoutInFlight = true;
    lastCheckoutUsedCreateOrder = true;
    lastCheckoutSelectedCount = selectedRows.size();
    lastCheckoutTotalRows = ui->cartTable ? ui->cartTable->rowCount() : 0;
    sendRequest(request);
    // 保护性超时：3.5s 未返回则解除 inFlight，提示网络慢
    QTimer::singleShot(3500, this, [this]{
        if (checkoutInFlight) {
            checkoutInFlight = false;
            showOnce("checkout_timeout", QMessageBox::Warning, "结算超时", "网络较慢或服务器繁忙，请稍后重试", 4000);
        }
    });
}

void ShoppingCart::on_deleteButton_clicked()
{
    qInfo() << "deleteButton clicked";
    QList<int> rows;
    if (ui->cartTable) {
        const int rc = ui->cartTable->rowCount();
        for (int r = 0; r < rc; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk && chk->isChecked()) rows.append(r);
        }
    }
    if (rows.isEmpty()) {
        showOnce("delete_no_selection", QMessageBox::Warning, "删除失败", "请选择要删除的商品", 3000);
        if (auto btn = findChild<QPushButton*>("deleteButton")) { btn->setEnabled(false); QTimer::singleShot(600, btn, [btn]{ btn->setEnabled(true); }); }
        return;
    }
    for (int row : rows) {
        if (row < 0 || row >= cartItems.size()) continue;
        QJsonObject request; request["type"] = "remove_from_cart"; request["product_id"] = cartItems[row]["product_id"]; sendRequest(request);
    }
}

void ShoppingCart::sendRequest(const QJsonObject &request)
{
    if (!socket) return;
    QJsonObject req = request;
    // 若未显式携带用户名且当前已登录，自动补充
    if (!username.isEmpty() && !req.contains("username")) req["username"] = username;
    QJsonDocument doc(req);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    socket->write(payload);
}

void ShoppingCart::refreshCart()
{
    loadCartItems();
}

void ShoppingCart::onItemClicked(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    // 可根据需要在此处理单元格点击，例如高亮、弹出详情等
}

void ShoppingCart::showOnce(const QString &tag, QMessageBox::Icon icon, const QString &title, const QString &text, int windowMs)
{
    const qint64 now = monotonic.elapsed();
    // 全局静默期：任意弹窗之间至少间隔 1.8s，避免顺序出现的“双弹”
    if (s_globalLastPopupMs >= 0 && (now - s_globalLastPopupMs) < 1800) {
        qInfo() << "showOnce suppress(global) tag=" << tag
                 << "now=" << now << "lastGlobal=" << s_globalLastPopupMs;
        return;
    }
    const qint64 last = lastPopupMsByTag.value(tag, -1);
    if (last >= 0 && (now - last) < windowMs) {
        qInfo() << "showOnce suppress(tagWindow) tag=" << tag
                 << "now=" << now << "lastTagMs=" << last << "window=" << windowMs;
        return;
    }
    lastPopupMsByTag.insert(tag, now);
    // 若已有活动弹窗，则直接忽略，避免同屏双弹
    if (s_activePopup && s_activePopup->isVisible()) {
        qInfo() << "showOnce suppress(activePopup) tag=" << tag;
        return;
    }
    // 使用堆对象搭配 QPointer 作为活动弹窗的哨兵
    QMessageBox *box = new QMessageBox(icon, title, text, QMessageBox::Ok, this);
    // 记录全局静默期起点
    s_globalLastPopupMs = now;
    qInfo() << "showOnce show tag=" << tag << "title=" << title << "text=" << text;
    box->setAttribute(Qt::WA_DeleteOnClose, false);
    box->exec();
    s_activePopup.clear();
    delete box;
}

// 新增：更新“已选择 X 项”标签
void ShoppingCart::updateSelectedCount()
{
    int count = 0;
    int totalRows = 0;
    if (ui->cartTable) {
        const int rows = ui->cartTable->rowCount(); totalRows = rows;
        for (int r = 0; r < rows; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk && chk->isChecked()) ++count;
        }
    }
    if (auto lbl = findChild<QLabel*>("selectedLabel")) {
        lbl->setText(QString("已选择 %1 项").arg(count));
    }
    if (auto all = findChild<QCheckBox*>("selectAllCheck")) {
        QSignalBlocker b(*all);
        all->setTristate(true);
        if (totalRows <= 0) {
            all->setCheckState(Qt::Unchecked);
        } else if (count == 0) {
            all->setCheckState(Qt::Unchecked);
        } else if (count == totalRows) {
            all->setCheckState(Qt::Checked);
        } else {
            all->setCheckState(Qt::PartiallyChecked);
        }
    }
}

// 事件过滤器：点击行切换首列复选框状态（数量列除外）
bool ShoppingCart::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->cartTable->viewport() && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QModelIndex idx = ui->cartTable->indexAt(me->pos());
        if (idx.isValid()) {
            int row = idx.row(); int col = idx.column();
            if (col != 3) { // 排除数量列
                QWidget *wrap = ui->cartTable->cellWidget(row, 0);
                if (wrap) {
                    auto chk = wrap->findChild<QCheckBox*>();
                    if (chk) { 
                        chk->setChecked(!chk->isChecked());
                        updateSelectedCount();
                        updateTotalPrice();
                        return true; 
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

int ShoppingCart::selectedRowCount() const {
    if (!ui->cartTable) return 0;
    int cnt = 0;
    const int rows = ui->cartTable->rowCount();
    for (int r = 0; r < rows; ++r) {
        QWidget *wrap = ui->cartTable->cellWidget(r, 0);
        if (!wrap) continue;
        auto chk = wrap->findChild<QCheckBox*>();
        if (chk && chk->isChecked()) ++cnt;
    }
    return cnt;
}

void ShoppingCart::ensureSelectAllHook()
{
    QCheckBox *all = findChild<QCheckBox*>("selectAllCheck");
    if (!all) {
        all = new QCheckBox(tr("全选/全不选"), this);
        all->setObjectName("selectAllCheck");
        if (auto topLay = findChild<QVBoxLayout*>()) {
            topLay->insertWidget(0, all);
            qInfo() << "[selectAllCheck] ensure() inserted dynamically";
        } else {
            all->move(8, 8);
            all->show();
            qInfo() << "[selectAllCheck] ensure() created no layout";
        }
    }
    // 重新绑定（安全起见，避免重复 lambda）
    QObject::disconnect(all, nullptr, this, nullptr);
    all->setTristate(true);
    connect(all, &QCheckBox::clicked, this, [this, all](bool){
        if (!ui->cartTable) return;
        const int rows = ui->cartTable->rowCount();
        bool anyUnchecked = false;
        for (int r = 0; r < rows; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk && !chk->isChecked()) { anyUnchecked = true; break; }
        }
        const bool target = anyUnchecked;
        qInfo() << "[selectAllClick] rows=" << rows << "anyUnchecked=" << anyUnchecked << "=> target=" << target;
        for (int r = 0; r < rows; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk) {
                QSignalBlocker b(*chk);
                chk->setChecked(target);
            }
        }
        qInfo() << "[selectAllClick] after toggle checkedCount=" << selectedRowCount();
        updateSelectedCount();
        updateTotalPrice();
    });
}

// 若服务器不支持 create_order 或返回语义上等价的“暂不支持/未知类型”等错误，
// 并且本次是全选（selectedCount == totalRows），则尝试回退到旧协议：checkout（全量结算）。
// 这样至少可以满足“全选结算”的场景，同时避免误把"部分勾选"走成全量结算。
void ShoppingCart::fallbackToLegacyCheckoutIfPossible(const QString &errorMsg)
{
    // 仅在最近一次是 create_order 且为“全选”并且仍有 socket 可用时尝试回退
    if (!lastCheckoutUsedCreateOrder) return;
    if (!socket) return;
    if (lastCheckoutTotalRows <= 0) return;
    if (lastCheckoutSelectedCount != lastCheckoutTotalRows) return; // 只在全选时回退，避免误伤部分勾选

    const QString m = errorMsg.toLower();
    const bool looksLikeUnsupported = (m.contains("unknown")
                                     || m.contains("unsupported")
                                     || m.contains("not implemented")
                                     || m.contains("未实现")
                                     || m.contains("不支持")
                                     || m.contains("未知类型"));
    if (!looksLikeUnsupported) return;

    qInfo() << "[fallback] create_order not supported, trying legacy 'checkout' (full cart)";
    QJsonObject req; req["type"] = "checkout"; if (!username.isEmpty()) req["username"] = username;
    checkoutInFlight = true; // 标记进入回退流程
    lastCheckoutUsedCreateOrder = false; // 进入旧协议
    sendRequest(req);
}

// 抑制自动连接警告的空槽：如果界面没有 prev/nextPage 控件也无副作用
void ShoppingCart::on_prevPage_clicked() { /* no-op */ }
void ShoppingCart::on_nextPage_clicked() { /* no-op */ }