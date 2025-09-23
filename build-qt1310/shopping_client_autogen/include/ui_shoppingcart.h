/********************************************************************************
** Form generated from reading UI file 'shoppingcart.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SHOPPINGCART_H
#define UI_SHOPPINGCART_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
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
            ShoppingCart->setObjectName("ShoppingCart");
        ShoppingCart->resize(800, 600);
        verticalLayout = new QVBoxLayout(ShoppingCart);
        verticalLayout->setObjectName("verticalLayout");
        cartTable = new QTableWidget(ShoppingCart);
        if (cartTable->columnCount() < 5)
            cartTable->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        cartTable->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        cartTable->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        cartTable->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        cartTable->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        cartTable->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        cartTable->setObjectName("cartTable");
        cartTable->setSelectionMode(QAbstractItemView::NoSelection);
        cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);

        verticalLayout->addWidget(cartTable);

        bottomLayout = new QHBoxLayout();
        bottomLayout->setObjectName("bottomLayout");
        backButton = new QPushButton(ShoppingCart);
        backButton->setObjectName("backButton");

        bottomLayout->addWidget(backButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        bottomLayout->addItem(horizontalSpacer);

        selectAllCheck = new QCheckBox(ShoppingCart);
        selectAllCheck->setObjectName("selectAllCheck");

        bottomLayout->addWidget(selectAllCheck);

        selectedLabel = new QLabel(ShoppingCart);
        selectedLabel->setObjectName("selectedLabel");

        bottomLayout->addWidget(selectedLabel);

        totalLabel = new QLabel(ShoppingCart);
        totalLabel->setObjectName("totalLabel");

        bottomLayout->addWidget(totalLabel);

        deleteButton = new QPushButton(ShoppingCart);
        deleteButton->setObjectName("deleteButton");

        bottomLayout->addWidget(deleteButton);

        clearButton = new QPushButton(ShoppingCart);
        clearButton->setObjectName("clearButton");

        bottomLayout->addWidget(clearButton);

        checkoutButton = new QPushButton(ShoppingCart);
        checkoutButton->setObjectName("checkoutButton");
        checkoutButton->setMinimumSize(QSize(120, 40));

        bottomLayout->addWidget(checkoutButton);


        verticalLayout->addLayout(bottomLayout);


        retranslateUi(ShoppingCart);

        QMetaObject::connectSlotsByName(ShoppingCart);
    } // setupUi

    void retranslateUi(QWidget *ShoppingCart)
    {
        ShoppingCart->setWindowTitle(QCoreApplication::translate("ShoppingCart", "\350\264\255\347\211\251\350\275\246", nullptr));
        QTableWidgetItem *___qtablewidgetitem = cartTable->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("ShoppingCart", "\351\200\211\346\213\251", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = cartTable->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("ShoppingCart", "\345\225\206\345\223\201\345\220\215\347\247\260", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = cartTable->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("ShoppingCart", "\345\215\225\344\273\267", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = cartTable->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("ShoppingCart", "\346\225\260\351\207\217", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = cartTable->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("ShoppingCart", "\345\260\217\350\256\241", nullptr));
        backButton->setText(QCoreApplication::translate("ShoppingCart", "\350\277\224\345\233\236", nullptr));
        selectAllCheck->setText(QCoreApplication::translate("ShoppingCart", "\345\205\250\351\200\211/\345\205\250\344\270\215\351\200\211", nullptr));
        selectedLabel->setText(QCoreApplication::translate("ShoppingCart", "\345\267\262\351\200\211\346\213\251 0 \351\241\271", nullptr));
        totalLabel->setText(QCoreApplication::translate("ShoppingCart", "\346\200\273\350\256\241: \357\277\2450.00", nullptr));
        deleteButton->setText(QCoreApplication::translate("ShoppingCart", "\345\210\240\351\231\244\346\211\200\351\200\211", nullptr));
        clearButton->setText(QCoreApplication::translate("ShoppingCart", "\346\270\205\347\251\272\350\264\255\347\211\251\350\275\246", nullptr));
        checkoutButton->setText(QCoreApplication::translate("ShoppingCart", "\347\273\223\347\256\227", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ShoppingCart: public Ui_ShoppingCart {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SHOPPINGCART_H
