/********************************************************************************
** Form generated from reading UI file 'chatwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CHATWINDOW_H
#define UI_CHATWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ChatWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QListWidget *chatList;
    QWidget *toolBar;
    QHBoxLayout *horizontalLayout;
    QPushButton *emojiButton;
    QPushButton *imageButton;
    QPushButton *fileButton;
    QSpacerItem *horizontalSpacer;
    QTextEdit *messageInput;
    QHBoxLayout *horizontalLayout_2;
    QLabel *tipLabel;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *sendButton;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *ChatWindow)
    {
        if (ChatWindow->objectName().isEmpty())
            ChatWindow->setObjectName(QString::fromUtf8("ChatWindow"));
        ChatWindow->resize(800, 600);
        centralwidget = new QWidget(ChatWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        
        chatList = new QListWidget(centralwidget);
        chatList->setObjectName(QString::fromUtf8("chatList"));
        chatList->setFrameShape(QFrame::NoFrame);
        chatList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        chatList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        verticalLayout->addWidget(chatList);
        
        toolBar = new QWidget(centralwidget);
        toolBar->setObjectName(QString::fromUtf8("toolBar"));
        horizontalLayout = new QHBoxLayout(toolBar);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        
        emojiButton = new QPushButton(toolBar);
        emojiButton->setObjectName(QString::fromUtf8("emojiButton"));
        horizontalLayout->addWidget(emojiButton);
        
        imageButton = new QPushButton(toolBar);
        imageButton->setObjectName(QString::fromUtf8("imageButton"));
        horizontalLayout->addWidget(imageButton);
        
        fileButton = new QPushButton(toolBar);
        fileButton->setObjectName(QString::fromUtf8("fileButton"));
        horizontalLayout->addWidget(fileButton);
        
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        horizontalLayout->addItem(horizontalSpacer);
        verticalLayout->addWidget(toolBar);
        
        messageInput = new QTextEdit(centralwidget);
        messageInput->setObjectName(QString::fromUtf8("messageInput"));
        messageInput->setMaximumSize(QSize(16777215, 100));
        verticalLayout->addWidget(messageInput);
        
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        
        tipLabel = new QLabel(centralwidget);
        tipLabel->setObjectName(QString::fromUtf8("tipLabel"));
        tipLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        horizontalLayout_2->addWidget(tipLabel);
        
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        horizontalLayout_2->addItem(horizontalSpacer_2);
        
        sendButton = new QPushButton(centralwidget);
        sendButton->setObjectName(QString::fromUtf8("sendButton"));
        sendButton->setMinimumSize(QSize(100, 30));
        horizontalLayout_2->addWidget(sendButton);
        
        verticalLayout->addLayout(horizontalLayout_2);
        
        ChatWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(ChatWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 800, 22));
        ChatWindow->setMenuBar(menubar);
        
        statusbar = new QStatusBar(ChatWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        ChatWindow->setStatusBar(statusbar);

        retranslateUi(ChatWindow);
        QMetaObject::connectSlotsByName(ChatWindow);
    } // setupUi

    void retranslateUi(QMainWindow *ChatWindow)
    {
        ChatWindow->setWindowTitle(QCoreApplication::translate("ChatWindow", "\345\256\242\346\234\215\350\201\212\345\244\251", nullptr));
        emojiButton->setText(QCoreApplication::translate("ChatWindow", "\350\241\250\346\203\205", nullptr));
        imageButton->setText(QCoreApplication::translate("ChatWindow", "\345\233\276\347\211\207", nullptr));
        fileButton->setText(QCoreApplication::translate("ChatWindow", "\346\226\207\344\273\266", nullptr));
        messageInput->setPlaceholderText(QCoreApplication::translate("ChatWindow", "\350\276\223\345\205\245\346\266\210\346\201\257...", nullptr));
        tipLabel->setText(QCoreApplication::translate("ChatWindow", "\346\214\211Enter\345\217\221\351\200\201\346\266\210\346\201\257\357\274\214Shift+Enter\346\215\242\350\241\214", nullptr));
        sendButton->setText(QCoreApplication::translate("ChatWindow", "\345\217\221\351\200\201", nullptr));
    } // retranslateUi
};

namespace Ui {
    class ChatWindow: public Ui_ChatWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CHATWINDOW_H