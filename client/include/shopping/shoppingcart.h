#ifndef SHOPPING_SHOPPINGCART_H
#define SHOPPING_SHOPPINGCART_H

#include <QWidget>
#include <QTcpSocket>
#include <QVector>
#include <QJsonObject>

namespace Ui {
class ShoppingCart;
}

class ShoppingCart : public QWidget {
	Q_OBJECT

public:
	explicit ShoppingCart(QTcpSocket *socket, QWidget *parent = nullptr);
	~ShoppingCart();
	void refreshCart();

signals:
	void checkoutCompleted();

private slots:
	void on_checkoutButton_clicked();
	void on_deleteButton_clicked();
	void onItemClicked(int row, int column);
	void onReadyRead();

private:
	Ui::ShoppingCart *ui;
	QTcpSocket *socket;
	QVector<QJsonObject> cartItems;

	void setupUi();
	void loadCartItems();
	void updateTotalPrice();
	void sendRequest(const QJsonObject &request);
};

#endif // SHOPPING_SHOPPINGCART_H