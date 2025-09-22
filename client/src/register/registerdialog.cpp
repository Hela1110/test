#include "register/registerdialog.h"
#include "ui_registerdialog.h"
#include <QMessageBox>

RegisterDialog::RegisterDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::RegisterDialog) {
    ui->setupUi(this);
    setWindowTitle("用户注册");
}

RegisterDialog::~RegisterDialog() {
    delete ui;
}

void RegisterDialog::on_submitButton_clicked() {
    const QString username = ui->usernameInput->text().trimmed();
    const QString password = ui->passwordInput->text();
    const QString confirm  = ui->confirmInput->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "注册", "用户名和密码不能为空");
        return;
    }
    if (password != confirm) {
        QMessageBox::warning(this, "注册", "两次输入的密码不一致");
        return;
    }
    emit submitRegister(username, password);
}

void RegisterDialog::onRegisterResult(bool success, const QString& message) {
    if (success) {
        QMessageBox::information(this, "注册成功", message.isEmpty() ? "注册成功" : message);
        accept();
    } else {
        QMessageBox::warning(this, "注册失败", message.isEmpty() ? "请重试" : message);
    }
}
