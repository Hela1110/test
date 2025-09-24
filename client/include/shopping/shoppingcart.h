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
	// 为了抑制 Qt connectSlotsByName 未找到信号的警告（如果 UI 上没有这两个按钮也安全）
	void on_prevPage_clicked();
	void on_nextPage_clicked();
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
    int selectedRowCount() const; // 新增：统计已勾选行数（调试、日志）
	void ensureSelectAllHook();   // 确保 selectAll 复选框存在并已连接

	QString lastCartSignature;    // 上一次构建的购物车签名（去重）
	qint64  lastCartBuildMs = -1; // 上一次构建时间（ms）

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // SHOPPING_SHOPPINGCART_H