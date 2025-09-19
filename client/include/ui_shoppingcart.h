/********************************************************************************
** Form generated from reading UI file 'shoppingcart.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
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
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ShoppingCart
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *toolbarLayout;
    QCheckBox *selectAllCheckBox;
    QPushButton *batchDeleteButton;
    QSpacerItem *horizontalSpacer_2;
    QTableWidget *cartTable;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QLabel *label;
    QLabel *totalPriceLabel;
    QPushButton *checkoutButton;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *ShoppingCart)
    {
        if (ShoppingCart->objectName().isEmpty())
            ShoppingCart->setObjectName(QString::fromUtf8("ShoppingCart"));
        ShoppingCart->resize(1000, 600);
        centralwidget = new QWidget(ShoppingCart);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        
        toolbarLayout = new QHBoxLayout();
        toolbarLayout->setObjectName(QString::fromUtf8("toolbarLayout"));
        selectAllCheckBox = new QCheckBox(centralwidget);
        selectAllCheckBox->setObjectName(QString::fromUtf8("selectAllCheckBox"));
        toolbarLayout->addWidget(selectAllCheckBox);
        
        batchDeleteButton = new QPushButton(centralwidget);
        batchDeleteButton->setObjectName(QString::fromUtf8("batchDeleteButton"));
        toolbarLayout->addWidget(batchDeleteButton);
        
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        toolbarLayout->addItem(horizontalSpacer_2);
        verticalLayout->addLayout(toolbarLayout);
        
        cartTable = new QTableWidget(centralwidget);
        cartTable->setObjectName(QString::fromUtf8("cartTable"));
        cartTable->setSelectionMode(QAbstractItemView::MultiSelection);
        cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        cartTable->setColumnCount(8);
        cartTable->setHorizontalHeaderLabels(QStringList() 
            << QString::fromUtf8("选择")
            << QString::fromUtf8("商品图片")
            << QString::fromUtf8("商品名称")
            << QString::fromUtf8("单价")
            << QString::fromUtf8("数量")
            << QString::fromUtf8("小计")
            << QString::fromUtf8("收藏")
            << QString::fromUtf8("操作"));
        verticalLayout->addWidget(cartTable);
        
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        horizontalLayout->addItem(horizontalSpacer);
        
        label = new QLabel(centralwidget);
        label->setObjectName(QString::fromUtf8("label"));
        horizontalLayout->addWidget(label);
        
        totalPriceLabel = new QLabel(centralwidget);
        totalPriceLabel->setObjectName(QString::fromUtf8("totalPriceLabel"));
        horizontalLayout->addWidget(totalPriceLabel);
        
        checkoutButton = new QPushButton(centralwidget);
        checkoutButton->setObjectName(QString::fromUtf8("checkoutButton"));
        checkoutButton->setMinimumSize(QSize(120, 40));
        horizontalLayout->addWidget(checkoutButton);
        verticalLayout->addLayout(horizontalLayout);
        
        ShoppingCart->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(ShoppingCart);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        ShoppingCart->setStatusBar(statusbar);

        retranslateUi(ShoppingCart);
        QMetaObject::connectSlotsByName(ShoppingCart);
    } // setupUi

    void retranslateUi(QMainWindow *ShoppingCart)
    {
        ShoppingCart->setWindowTitle(QCoreApplication::translate("ShoppingCart", "\350\264\255\347\211\251\350\275\246", nullptr));
        selectAllCheckBox->setText(QCoreApplication::translate("ShoppingCart", "\345\205\250\351\200\211", nullptr));
        batchDeleteButton->setText(QCoreApplication::translate("ShoppingCart", "\346\211\271\351\207\217\345\210\240\351\231\244", nullptr));
        label->setText(QCoreApplication::translate("ShoppingCart", "\346\200\273\350\256\241\357\274\232", nullptr));
        totalPriceLabel->setText(QCoreApplication::translate("ShoppingCart", "\302\2450.00", nullptr));
        checkoutButton->setText(QCoreApplication::translate("ShoppingCart", "\347\273\223\347\256\227", nullptr));
    } // retranslateUi
};

namespace Ui {
    class ShoppingCart: public Ui_ShoppingCart {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SHOPPINGCART_H