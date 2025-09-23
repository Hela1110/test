/*
 Hand-written replacement for uic output matching ui/shoppingcart.ui
 Root widget: QWidget named "ShoppingCart"
 Members: cartTable, bottomLayout (unused in code), backButton, totalLabel, deleteButton, checkoutButton
*/
#ifndef UI_SHOPPINGCART_H
#define UI_SHOPPINGCART_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ShoppingCart
{
public:
	QVBoxLayout *verticalLayout;
	QTableWidget *cartTable;
	QHBoxLayout *bottomLayout;
	QPushButton *backButton;
	QSpacerItem *horizontalSpacer;
	QCheckBox *selectAllCheck;
	QLabel *selectedLabel;
	QLabel *totalLabel;
	QPushButton *deleteButton;
	QPushButton *clearButton;
	QPushButton *checkoutButton;

	void setupUi(QWidget *ShoppingCart)
	{
		if (ShoppingCart->objectName().isEmpty())
			ShoppingCart->setObjectName(QString::fromUtf8("ShoppingCart"));
		ShoppingCart->resize(800, 600);
		verticalLayout = new QVBoxLayout(ShoppingCart);
		verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));

	cartTable = new QTableWidget(ShoppingCart);
		cartTable->setObjectName(QString::fromUtf8("cartTable"));
	cartTable->setSelectionMode(QAbstractItemView::NoSelection);
	cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	cartTable->setColumnCount(5);
		QStringList headers;
	headers << QString::fromUtf8("选择")
		<< QString::fromUtf8("商品名称")
				<< QString::fromUtf8("单价")
				<< QString::fromUtf8("数量")
				<< QString::fromUtf8("小计");
		cartTable->setHorizontalHeaderLabels(headers);
		verticalLayout->addWidget(cartTable);

		bottomLayout = new QHBoxLayout();
		bottomLayout->setObjectName(QString::fromUtf8("bottomLayout"));

		backButton = new QPushButton(ShoppingCart);
		backButton->setObjectName(QString::fromUtf8("backButton"));
		bottomLayout->addWidget(backButton);

		horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
		bottomLayout->addItem(horizontalSpacer);

	selectAllCheck = new QCheckBox(ShoppingCart);
	selectAllCheck->setObjectName(QString::fromUtf8("selectAllCheck"));
	bottomLayout->addWidget(selectAllCheck);

	selectedLabel = new QLabel(ShoppingCart);
	selectedLabel->setObjectName(QString::fromUtf8("selectedLabel"));
	bottomLayout->addWidget(selectedLabel);

	totalLabel = new QLabel(ShoppingCart);
		totalLabel->setObjectName(QString::fromUtf8("totalLabel"));
		bottomLayout->addWidget(totalLabel);

	deleteButton = new QPushButton(ShoppingCart);
		deleteButton->setObjectName(QString::fromUtf8("deleteButton"));
		bottomLayout->addWidget(deleteButton);

	clearButton = new QPushButton(ShoppingCart);
	clearButton->setObjectName(QString::fromUtf8("clearButton"));
	bottomLayout->addWidget(clearButton);

		checkoutButton = new QPushButton(ShoppingCart);
		checkoutButton->setObjectName(QString::fromUtf8("checkoutButton"));
		QSize s; s.setWidth(120); s.setHeight(40);
		checkoutButton->setMinimumSize(s);
		bottomLayout->addWidget(checkoutButton);

		verticalLayout->addLayout(bottomLayout);

		retranslateUi(ShoppingCart);
		QMetaObject::connectSlotsByName(ShoppingCart);
	}

	void retranslateUi(QWidget *ShoppingCart)
	{
		ShoppingCart->setWindowTitle(QCoreApplication::translate("ShoppingCart", "\346\210\277\350\241\250\350\275\246", nullptr));
		backButton->setText(QCoreApplication::translate("ShoppingCart", "\345\274\200\345\217\267", nullptr));
	selectAllCheck->setText(QCoreApplication::translate("ShoppingCart", "\345\205\250\351\224\201/\345\205\250\344\270\272\351\224\201", nullptr));
		selectedLabel->setText(QCoreApplication::translate("ShoppingCart", "\345\267\262\351\224\201\x20\x30\x20\xe9\xa1\xb9", nullptr));
		totalLabel->setText(QCoreApplication::translate("ShoppingCart", "\346\200\273\350\256\241: \xEF\xBF\xA50.00", nullptr));
	deleteButton->setText(QCoreApplication::translate("ShoppingCart", "\345\210\240\351\231\244\346\211\223\351\231\244", nullptr));
	clearButton->setText(QCoreApplication::translate("ShoppingCart", "\346\260\221\347\273\264\350\241\250\350\275\246", nullptr));
		checkoutButton->setText(QCoreApplication::translate("ShoppingCart", "\347\273\223\347\256\227", nullptr));
	}
};

namespace Ui {
	class ShoppingCart: public Ui_ShoppingCart {};
}

QT_END_NAMESPACE

#endif // UI_SHOPPINGCART_H