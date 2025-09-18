#include "shopping/shoppingcart.h"
#include "ui_shoppingcart.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>

ShoppingCart::ShoppingCart(QTcpSocket *socket, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ShoppingCart),
    socket(socket)
{
    ui->setupUi(this);
    setupUi();
    connect(socket, &QTcpSocket::readyRead, this, &ShoppingCart::onReadyRead);
    loadCartItems();
}

ShoppingCart::~ShoppingCart()
{
    delete ui;
}

void ShoppingCart::setupUi()
{
    // 设置表格属性
    ui->cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->cartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // 连接信号槽
    connect(ui->cartTable, &QTableWidget::cellClicked, this, &ShoppingCart::onItemClicked);
    connect(ui->checkoutButton, &QPushButton::clicked, this, &ShoppingCart::on_checkoutButton_clicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &ShoppingCart::on_deleteButton_clicked);
}

void ShoppingCart::loadCartItems()
{
    QJsonObject request;
    request["type"] = "get_cart";
    sendRequest(request);
}

void ShoppingCart::onReadyRead()
{
    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject response = doc.object();
    
    if (response["type"].toString() == "cart_items") {
        // 清空表格
        ui->cartTable->clearContents();
        ui->cartTable->setRowCount(0);
        cartItems.clear();
        
        // 添加新的购物车项目
        QJsonArray items = response["items"].toArray();
        for (const QJsonValue &value : items) {
            QJsonObject item = value.toObject();
            cartItems.append(item);
            
            int row = ui->cartTable->rowCount();
            ui->cartTable->insertRow(row);
            
            ui->cartTable->setItem(row, 0, new QTableWidgetItem(item["name"].toString()));
            ui->cartTable->setItem(row, 1, new QTableWidgetItem(QString::number(item["price"].toDouble())));
            ui->cartTable->setItem(row, 2, new QTableWidgetItem(QString::number(item["quantity"].toInt())));
            ui->cartTable->setItem(row, 3, new QTableWidgetItem(QString::number(
                item["price"].toDouble() * item["quantity"].toInt()
            )));
        }
        
        updateTotalPrice();
    }
    else if (response["type"].toString() == "checkout_response") {
        if (response["success"].toBool()) {
            QMessageBox::information(this, "结算成功", "购物车结算成功！");
            emit checkoutCompleted();
            loadCartItems();
        } else {
            QMessageBox::warning(this, "结算失败", response["message"].toString());
        }
    }
}

void ShoppingCart::updateTotalPrice()
{
    double total = 0;
    for (const QJsonObject &item : cartItems) {
        total += item["price"].toDouble() * item["quantity"].toInt();
    }
    ui->totalLabel->setText(QString("总计: ￥%1").arg(total));
}

void ShoppingCart::on_checkoutButton_clicked()
{
    if (cartItems.isEmpty()) {
        QMessageBox::warning(this, "结算失败", "购物车为空");
        return;
    }
    
    QJsonObject request;
    request["type"] = "checkout";
    sendRequest(request);
}

void ShoppingCart::on_deleteButton_clicked()
{
    QList<QTableWidgetItem*> items = ui->cartTable->selectedItems();
    if (items.isEmpty()) {
        QMessageBox::warning(this, "删除失败", "请选择要删除的商品");
        return;
    }
    
    QJsonObject request;
    request["type"] = "remove_from_cart";
    request["product_id"] = cartItems[items[0]->row()]["product_id"];
    sendRequest(request);
}

void ShoppingCart::sendRequest(const QJsonObject &request)
{
    QJsonDocument doc(request);
    socket->write(doc.toJson());
}

void ShoppingCart::refreshCart()
{
    loadCartItems();
}