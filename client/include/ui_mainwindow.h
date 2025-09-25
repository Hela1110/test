/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui' (manually crafted)
** This header mimics Qt's uic output for use without running uic.
********************************************************************************/
#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    // 外层水平布局：左侧功能栏 + 右侧主内容
    QHBoxLayout *outerLayout;
    // 左侧功能栏
    QVBoxLayout *leftSidebarLayout;
    QPushButton *accountButton;
    QPushButton *ordersButton;
    QPushButton *chatButton;
    QPushButton *cartButton;
    // 右侧主内容（纵向）：顶部搜索 + 内容区 + 底部分页
    QVBoxLayout *rootLayout;
    QHBoxLayout *topBarLayout;
    QLineEdit *searchInput;
    QPushButton *searchButton;
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
    MainWindow->resize(1366, 900);

        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        // 外层水平布局
        outerLayout = new QHBoxLayout(centralwidget);
        outerLayout->setObjectName(QString::fromUtf8("outerLayout"));
        outerLayout->setContentsMargins(12, 12, 12, 12);
        outerLayout->setSpacing(10);

        // 左侧功能栏
        leftSidebarLayout = new QVBoxLayout();
        leftSidebarLayout->setObjectName(QString::fromUtf8("leftSidebarLayout"));
        accountButton = new QPushButton(centralwidget);
        accountButton->setObjectName(QString::fromUtf8("accountButton"));
    accountButton->setMinimumWidth(120);
        leftSidebarLayout->addWidget(accountButton);
        ordersButton = new QPushButton(centralwidget);
        ordersButton->setObjectName(QString::fromUtf8("ordersButton"));
    ordersButton->setMinimumWidth(120);
        leftSidebarLayout->addWidget(ordersButton);
        chatButton = new QPushButton(centralwidget);
        chatButton->setObjectName(QString::fromUtf8("chatButton"));
    chatButton->setMinimumWidth(120);
        leftSidebarLayout->addWidget(chatButton);
        cartButton = new QPushButton(centralwidget);
        cartButton->setObjectName(QString::fromUtf8("cartButton"));
    cartButton->setMinimumWidth(120);
        leftSidebarLayout->addWidget(cartButton);
        // 占位拉伸
        QSpacerItem *leftSidebarSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
        leftSidebarLayout->addItem(leftSidebarSpacer);
        outerLayout->addLayout(leftSidebarLayout);

        // 右侧主内容
        rootLayout = new QVBoxLayout();
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setSpacing(10);

        // 顶部搜索栏
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
        // 右对齐占位
        QSpacerItem *topBarSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        topBarLayout->addItem(topBarSpacer);
        rootLayout->addLayout(topBarLayout);

        // 内容区
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

        // 底部分页条
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

        // 外层拼装：左侧 + 右侧
        outerLayout->addLayout(rootLayout);

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
        accountButton->setText(QString::fromUtf8("账号"));
        ordersButton->setText(QString::fromUtf8("账单"));
        chatButton->setText(QString::fromUtf8("客服"));
        cartButton->setText(QString::fromUtf8("购物车"));
        // no top pager button texts
    }
};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
}

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
