#ifndef SHOPPINGCART_H
#define SHOPPINGCART_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace Ui {
class ShoppingCart;
}

struct CartItem {
    QString name;
    QString imageUrl;
    double price;
    int quantity;
    bool favorite;
};

class ShoppingCart : public QMainWindow {
    Q_OBJECT

public:
    explicit ShoppingCart(QWidget *parent = nullptr);
    ~ShoppingCart();

    void addItem(const QString &name, const QString &imageUrl, double price, int quantity);
    void updateTotal();
    void clearCart();
    void saveCartToFile();
    void loadCartFromFile();

private slots:
    void on_checkoutButton_clicked();
    void on_cartTable_cellChanged(int row, int column);
    void removeItem();
    void toggleFavorite();
    void on_selectAllCheckBox_stateChanged(int state);
    void on_batchDeleteButton_clicked();
    void saveCartState();

private:
    Ui::ShoppingCart *ui;
    double total;
    QList<CartItem> cartItems;
    QString cartFilePath;
    void setupTableHeaders();
    void updateTableRow(int row, const CartItem &item);
    void loadImages();
};

#endif // SHOPPINGCART_H