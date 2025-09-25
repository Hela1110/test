/********************************************************************************
** Form generated from reading UI file 'registerdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_REGISTERDIALOG_H
#define UI_REGISTERDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_RegisterDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QLabel *labelUser;
    QLineEdit *usernameInput;
    QLabel *labelPass;
    QLineEdit *passwordInput;
    QLabel *labelConfirm;
    QLineEdit *confirmInput;
    QLabel *labelPhone;
    QLineEdit *phoneInput;
    QHBoxLayout *buttonLayout;
    QSpacerItem *spacer;
    QPushButton *submitButton;
    QPushButton *cancelButton;

    void setupUi(QDialog *RegisterDialog)
    {
        if (RegisterDialog->objectName().isEmpty())
            RegisterDialog->setObjectName("RegisterDialog");
        RegisterDialog->resize(360, 220);
        verticalLayout = new QVBoxLayout(RegisterDialog);
        verticalLayout->setObjectName("verticalLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        labelUser = new QLabel(RegisterDialog);
        labelUser->setObjectName("labelUser");

        formLayout->setWidget(0, QFormLayout::LabelRole, labelUser);

        usernameInput = new QLineEdit(RegisterDialog);
        usernameInput->setObjectName("usernameInput");

        formLayout->setWidget(0, QFormLayout::FieldRole, usernameInput);

        labelPass = new QLabel(RegisterDialog);
        labelPass->setObjectName("labelPass");

        formLayout->setWidget(1, QFormLayout::LabelRole, labelPass);

        passwordInput = new QLineEdit(RegisterDialog);
        passwordInput->setObjectName("passwordInput");
        passwordInput->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(1, QFormLayout::FieldRole, passwordInput);

        labelConfirm = new QLabel(RegisterDialog);
        labelConfirm->setObjectName("labelConfirm");

        formLayout->setWidget(2, QFormLayout::LabelRole, labelConfirm);

        confirmInput = new QLineEdit(RegisterDialog);
        confirmInput->setObjectName("confirmInput");
        confirmInput->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(2, QFormLayout::FieldRole, confirmInput);

        labelPhone = new QLabel(RegisterDialog);
        labelPhone->setObjectName("labelPhone");

        formLayout->setWidget(3, QFormLayout::LabelRole, labelPhone);

        phoneInput = new QLineEdit(RegisterDialog);
        phoneInput->setObjectName("phoneInput");

        formLayout->setWidget(3, QFormLayout::FieldRole, phoneInput);


        verticalLayout->addLayout(formLayout);

        buttonLayout = new QHBoxLayout();
        buttonLayout->setObjectName("buttonLayout");
        spacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonLayout->addItem(spacer);

        submitButton = new QPushButton(RegisterDialog);
        submitButton->setObjectName("submitButton");

        buttonLayout->addWidget(submitButton);

        cancelButton = new QPushButton(RegisterDialog);
        cancelButton->setObjectName("cancelButton");

        buttonLayout->addWidget(cancelButton);


        verticalLayout->addLayout(buttonLayout);


        retranslateUi(RegisterDialog);

        QMetaObject::connectSlotsByName(RegisterDialog);
    } // setupUi

    void retranslateUi(QDialog *RegisterDialog)
    {
        RegisterDialog->setWindowTitle(QCoreApplication::translate("RegisterDialog", "\347\224\250\346\210\267\346\263\250\345\206\214", nullptr));
        labelUser->setText(QCoreApplication::translate("RegisterDialog", "\347\224\250\346\210\267\345\220\215\357\274\232", nullptr));
        labelPass->setText(QCoreApplication::translate("RegisterDialog", "\345\257\206\347\240\201\357\274\232", nullptr));
        labelConfirm->setText(QCoreApplication::translate("RegisterDialog", "\347\241\256\350\256\244\345\257\206\347\240\201\357\274\232", nullptr));
        labelPhone->setText(QCoreApplication::translate("RegisterDialog", "\346\211\213\346\234\272\345\217\267\357\274\232", nullptr));
        submitButton->setText(QCoreApplication::translate("RegisterDialog", "\346\263\250\345\206\214", nullptr));
        cancelButton->setText(QCoreApplication::translate("RegisterDialog", "\345\217\226\346\266\210", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RegisterDialog: public Ui_RegisterDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REGISTERDIALOG_H
