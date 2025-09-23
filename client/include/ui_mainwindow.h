/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui' (manually crafted)
** This header mimics Qt's uic output for use without running uic.
********************************************************************************/
#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *rootLayout;
    QHBoxLayout *topBarLayout;
    QLineEdit *searchInput;
    QPushButton *searchButton;
    QToolButton *cartButton;
    QToolButton *chatButton;
    QToolButton *accountButton;
    QToolButton *ordersButton;
    // top pager buttons removed
    QWidget *carouselArea;
    QWidget *recommendationsArea;
    QWidget *promotionsArea;
    QHBoxLayout *bottomPagerLayout;
    QLabel *pageInfoLabel;
    QLabel *totalInfoLabel;
    QSpinBox *gotoPageSpin;
    QPushButton *gotoPageButton;
    QStatusBar *statusbar;
    QMenuBar *menubar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1024, 768);

        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        rootLayout = new QVBoxLayout(centralwidget);
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setContentsMargins(12, 12, 12, 12);
        rootLayout->setSpacing(10);

        // Top bar: search + actions
        topBarLayout = new QHBoxLayout();
        topBarLayout->setObjectName(QString::fromUtf8("topBarLayout"));
    searchInput = new QLineEdit(centralwidget);
        searchInput->setObjectName(QString::fromUtf8("searchInput"));
    searchInput->setPlaceholderText(QString::fromUtf8("搜索商品、店铺"));
    QSizePolicy spInput(QSizePolicy::Expanding, QSizePolicy::Fixed);
    searchInput->setSizePolicy(spInput);
        topBarLayout->addWidget(searchInput);

        searchButton = new QPushButton(centralwidget);
        searchButton->setObjectName(QString::fromUtf8("searchButton"));
    searchButton->setMinimumWidth(80);
        topBarLayout->addWidget(searchButton);

    // push following buttons to the right
    topBarLayout->addStretch(1);

        chatButton = new QToolButton(centralwidget);
        chatButton->setObjectName(QString::fromUtf8("chatButton"));
    chatButton->setMinimumWidth(80);
        topBarLayout->addWidget(chatButton);

        cartButton = new QToolButton(centralwidget);
        cartButton->setObjectName(QString::fromUtf8("cartButton"));
    cartButton->setMinimumWidth(80);
        topBarLayout->addWidget(cartButton);

        accountButton = new QToolButton(centralwidget);
        accountButton->setObjectName(QString::fromUtf8("accountButton"));
        accountButton->setMinimumWidth(80);
        topBarLayout->addWidget(accountButton);

    ordersButton = new QToolButton(centralwidget);
    ordersButton->setObjectName(QString::fromUtf8("ordersButton"));
    ordersButton->setMinimumWidth(80);
    topBarLayout->addWidget(ordersButton);

    // pagination controls removed from top bar

        rootLayout->addLayout(topBarLayout);

        // Content placeholders
    carouselArea = new QWidget(centralwidget);
        carouselArea->setObjectName(QString::fromUtf8("carouselArea"));
    carouselArea->setMinimumHeight(200);
    carouselArea->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
        rootLayout->addWidget(carouselArea);

    recommendationsArea = new QWidget(centralwidget);
        recommendationsArea->setObjectName(QString::fromUtf8("recommendationsArea"));
    recommendationsArea->setMinimumHeight(200);
    recommendationsArea->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
        rootLayout->addWidget(recommendationsArea);

    promotionsArea = new QWidget(centralwidget);
        promotionsArea->setObjectName(QString::fromUtf8("promotionsArea"));
    promotionsArea->setMinimumHeight(150);
    promotionsArea->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
        rootLayout->addWidget(promotionsArea);

        // bottom pager bar
        bottomPagerLayout = new QHBoxLayout();
        bottomPagerLayout->setObjectName(QString::fromUtf8("bottomPagerLayout"));
        pageInfoLabel = new QLabel(centralwidget);
        pageInfoLabel->setObjectName(QString::fromUtf8("pageInfoLabel"));
        pageInfoLabel->setText(QString::fromUtf8("第 1/1 页"));
        bottomPagerLayout->addWidget(pageInfoLabel);

        totalInfoLabel = new QLabel(centralwidget);
        totalInfoLabel->setObjectName(QString::fromUtf8("totalInfoLabel"));
        totalInfoLabel->setText(QString::fromUtf8("共 0 条"));
        bottomPagerLayout->addWidget(totalInfoLabel);

        QSpacerItem *pagerSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        bottomPagerLayout->addItem(pagerSpacer);

        QLabel *gotoLabel = new QLabel(centralwidget);
        gotoLabel->setObjectName(QString::fromUtf8("gotoLabel"));
        gotoLabel->setText(QString::fromUtf8("跳转到"));
        bottomPagerLayout->addWidget(gotoLabel);

        gotoPageSpin = new QSpinBox(centralwidget);
        gotoPageSpin->setObjectName(QString::fromUtf8("gotoPageSpin"));
        gotoPageSpin->setMinimum(1);
        gotoPageSpin->setMaximum(9999);
        gotoPageSpin->setValue(1);
        QSize maxSz; maxSz.setWidth(80); maxSz.setHeight(16777215);
        gotoPageSpin->setMaximumSize(maxSz);
        bottomPagerLayout->addWidget(gotoPageSpin);

        gotoPageButton = new QPushButton(centralwidget);
        gotoPageButton->setObjectName(QString::fromUtf8("gotoPageButton"));
        gotoPageButton->setText(QString::fromUtf8("跳转"));
        gotoPageButton->setMinimumWidth(80);
        bottomPagerLayout->addWidget(gotoPageButton);

        rootLayout->addLayout(bottomPagerLayout);

        MainWindow->setCentralWidget(centralwidget);

        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 1024, 22));
        MainWindow->setMenuBar(menubar);

        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);
        QMetaObject::connectSlotsByName(MainWindow);
    }

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QString::fromUtf8("购物系统"));
        searchButton->setText(QString::fromUtf8("搜索"));
        chatButton->setText(QString::fromUtf8("客服"));
        cartButton->setText(QString::fromUtf8("购物车"));
    accountButton->setText(QString::fromUtf8("账号"));
    ordersButton->setText(QString::fromUtf8("订单"));
        // no top pager button texts
    }
};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
}

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
