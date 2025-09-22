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
    QWidget *carouselArea;
    QWidget *recommendationsArea;
    QWidget *promotionsArea;
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

        // Top bar: search + actions
        topBarLayout = new QHBoxLayout();
        topBarLayout->setObjectName(QString::fromUtf8("topBarLayout"));
        searchInput = new QLineEdit(centralwidget);
        searchInput->setObjectName(QString::fromUtf8("searchInput"));
        searchInput->setPlaceholderText(QString::fromUtf8("搜索商品、店铺"));
        topBarLayout->addWidget(searchInput);

        searchButton = new QPushButton(centralwidget);
        searchButton->setObjectName(QString::fromUtf8("searchButton"));
        topBarLayout->addWidget(searchButton);

        chatButton = new QToolButton(centralwidget);
        chatButton->setObjectName(QString::fromUtf8("chatButton"));
        topBarLayout->addWidget(chatButton);

        cartButton = new QToolButton(centralwidget);
        cartButton->setObjectName(QString::fromUtf8("cartButton"));
        topBarLayout->addWidget(cartButton);

        rootLayout->addLayout(topBarLayout);

        // Content placeholders
        carouselArea = new QWidget(centralwidget);
        carouselArea->setObjectName(QString::fromUtf8("carouselArea"));
        carouselArea->setMinimumHeight(200);
        rootLayout->addWidget(carouselArea);

        recommendationsArea = new QWidget(centralwidget);
        recommendationsArea->setObjectName(QString::fromUtf8("recommendationsArea"));
        recommendationsArea->setMinimumHeight(200);
        rootLayout->addWidget(recommendationsArea);

        promotionsArea = new QWidget(centralwidget);
        promotionsArea->setObjectName(QString::fromUtf8("promotionsArea"));
        promotionsArea->setMinimumHeight(150);
        rootLayout->addWidget(promotionsArea);

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
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\350\265\267\345\210\253\347\263\273\351\227\264", nullptr));
        searchButton->setText(QCoreApplication::translate("MainWindow", "\346\241\243\\u7d22\u7f51", nullptr));
        chatButton->setText(QCoreApplication::translate("MainWindow", "\345\256\242\346\234\215", nullptr));
        cartButton->setText(QCoreApplication::translate("MainWindow", "\350\267\257\347\224\250\350\275\246", nullptr));
    }
};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
}

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
