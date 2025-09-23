#ifndef SHOPPING_SHOPPINGCART_H
#define SHOPPING_SHOPPINGCART_H

#include <QWidget>
#include <QTcpSocket>
#include <QVector>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QHash>

namespace Ui {
class ShoppingCart;
}

class ShoppingCart : public QWidget {
	Q_OBJECT

public:
	explicit ShoppingCart(QTcpSocket *socket, QWidget *parent = nullptr);
	~ShoppingCart();
	void refreshCart();
	// 由 MainWindow 设置当前登录用户，用于请求中携带
	void setUsername(const QString &u) { username = u; }
	// 由 MainWindow 转发的消息入口
	public slots:
	void handleMessage(const QJsonObject &response);

signals:
	void checkoutCompleted();

private slots:
	void on_checkoutButton_clicked();
	void on_deleteButton_clicked();
	void onItemClicked(int row, int column);

private:
	Ui::ShoppingCart *ui;
	QTcpSocket *socket;
	QVector<QJsonObject> cartItems;
	bool checkoutInFlight = false;
	qint64 lastCheckoutMs = 0;
	QElapsedTimer monotonic;
    // 轻量弹窗去重：tag -> lastShownMs
    QHash<QString, qint64> lastPopupMsByTag;
	QString username; // 当前用户名，可为空表示匿名

	void setupUi();
	void loadCartItems();
	void updateTotalPrice();
	void updateSelectedCount();
	void sendRequest(const QJsonObject &request);
	void showOnce(const QString &tag, QMessageBox::Icon icon, const QString &title, const QString &text, int windowMs = 400);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // SHOPPING_SHOPPINGCART_H