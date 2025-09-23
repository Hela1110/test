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
    // 通过 findChild 再次确认关键控件；并挂载仅日志用连接（不影响业务）
    if (auto btn = findChild<QPushButton*>("checkoutButton")) {
        // 日志 + 兜底连接（UniqueConnection 防止重复连接）
        connect(btn, &QPushButton::clicked, this, [](){ qInfo() << "checkoutButton native click"; }, Qt::UniqueConnection);
        connect(btn, &QPushButton::clicked, this, &ShoppingCart::on_checkoutButton_clicked, Qt::UniqueConnection);
    }
    if (auto delBtn = findChild<QPushButton*>("deleteButton")) {
        connect(delBtn, &QPushButton::clicked, this, [](){ qInfo() << "deleteButton native click"; }, Qt::UniqueConnection);
        connect(delBtn, &QPushButton::clicked, this, &ShoppingCart::on_deleteButton_clicked, Qt::UniqueConnection);
    }
    if (auto clrBtn = findChild<QPushButton*>("clearButton")) {
        connect(clrBtn, &QPushButton::clicked, this, [this]{
            if (QMessageBox::question(this, tr("清空购物车"), tr("确定要清空购物车吗？")) != QMessageBox::Yes) return;
            QJsonObject req; req["type"] = "clear_cart"; if (!username.isEmpty()) req["username"] = username; sendRequest(req);
            // 无论服务器响应如何，短延时后主动刷新以避免“无反应”的感知
            QTimer::singleShot(250, this, [this]{ loadCartItems(); });
        }, Qt::UniqueConnection);
        clrBtn->setVisible(true);
        clrBtn->setEnabled(true);
    }
    if (auto all = findChild<QCheckBox*>("selectAllCheck")) {
        all->setTristate(true);
        // 使用 clicked 事件，避免三态状态机导致的“半选”无法正确下发到行复选框
        connect(all, &QCheckBox::clicked, this, [this, all](bool /*checked*/){
            if (!ui->cartTable) return;
            const int rows = ui->cartTable->rowCount();
            bool anyUnchecked = false;
            for (int r = 0; r < rows; ++r) {
                QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                if (!wrap) continue;
                auto chk = wrap->findChild<QCheckBox*>();
                if (chk && !chk->isChecked()) { anyUnchecked = true; break; }
            }
            const bool target = anyUnchecked; // 存在未选则本次点击视为“全选”，否则“全不选”
            for (int r = 0; r < rows; ++r) {
                QWidget *wrap = ui->cartTable->cellWidget(r, 0);
                if (!wrap) continue;
                auto chk = wrap->findChild<QCheckBox*>();
                if (chk) {
                    QSignalBlocker b(*chk);
                    chk->setChecked(target);
                }
            }
            updateSelectedCount();
            updateTotalPrice();
        }, Qt::UniqueConnection);
    }
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
            ui->cartTable->setItem(row, 2, new QTableWidgetItem(QString::number(price, 'f', 2)));
            // 数量使用 SpinBox，带上下箭头
            auto *spin = new QSpinBox(ui->cartTable);
            int maxQty = (stock >= 0 ? qMax(0, stock) : 9999);
            spin->setRange(0, maxQty);
            spin->setValue(quantity);
            ui->cartTable->setCellWidget(row, 3, spin);
            // 小计
            ui->cartTable->setItem(row, 4, new QTableWidgetItem(QString::number(price * quantity, 'f', 2)));

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
                    auto *sub = ui->cartTable->item(row, 4);
                    if (sub) sub->setText(QString::number(price * val, 'f', 2));
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
        }
        updateSelectedCount();
        updateTotalPrice();
        // 安装事件过滤器以支持点击行切换选中
        ui->cartTable->viewport()->installEventFilter(this);
    } else if (type == QLatin1String("checkout_response")) {
        // 仅在一次有效结算流程中处理响应，避免重复弹窗
        if (!checkoutInFlight) {
            return;
        }

        checkoutInFlight = false; // 无论成功失败，结束本次流程
        if (response.value("success").toBool()) {
            // 结算成功提示含总金额
            double total = 0;
            for (const auto &it : cartItems) total += it.value("price").toDouble() * it.value("quantity").toInt();
            showOnce("checkout_ok", QMessageBox::Information, "结算成功", QString("总金额：￥%1").arg(QString::number(total, 'f', 2)));
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
                            QJsonObject request; request["type"] = "remove_from_cart"; request["product_id"] = cartItems[r]["product_id"]; sendRequest(request);
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
            double total = 0;
            for (const auto &it : cartItems) total += it.value("price").toDouble() * it.value("quantity").toInt();
            showOnce("order_ok", QMessageBox::Information, "下单成功", QString("订单已创建：#%1\n总金额：￥%2").arg(orderId).arg(QString::number(total, 'f', 2)));
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
        }
    } else if (type == QLatin1String("clear_cart_response")) {
        if (response.value("success").toBool()) {
            // 立即清空本地 UI，提升操作反馈
            QSignalBlocker blocker(ui->cartTable);
            cartItems.clear();
            if (ui->cartTable) { ui->cartTable->setRowCount(0); ui->cartTable->clearContents(); }
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
    double total = 0;
    // 仅统计被勾选的行；若没有任何行被勾选，则为 0
    if (ui->cartTable) {
        const int rows = ui->cartTable->rowCount();
        for (int r = 0; r < rows; ++r) {
            QWidget *wrap = ui->cartTable->cellWidget(r, 0);
            if (!wrap) continue;
            auto chk = wrap->findChild<QCheckBox*>();
            if (chk && chk->isChecked()) {
                // 优先以小计列为准
                if (auto *sub = ui->cartTable->item(r, 4)) {
                    bool ok = false; double v = sub->text().toDouble(&ok);
                    if (ok) { total += v; continue; }
                }
                // 兜底：从数据模型计算（避免显示格式导致 toDouble 失败）
                if (r >=0 && r < cartItems.size()) {
                    const auto &it = cartItems[r];
                    total += it.value("price").toDouble() * it.value("quantity").toInt();
                }
            }
        }
    }
    if (auto lbl = findChild<QLabel*>("totalLabel")) {
        lbl->setText(QString("总计: ￥%1").arg(QString::number(total, 'f', 2)));
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
    sendRequest(request);
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