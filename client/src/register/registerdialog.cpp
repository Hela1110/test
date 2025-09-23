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
    if (submitting) return; // 防止重复提交
    const QString username = ui->usernameInput->text().trimmed();
    const QString password = ui->passwordInput->text();
    const QString confirm  = ui->confirmInput->text();
    const QString phone    = ui->phoneInput ? ui->phoneInput->text().trimmed() : QString();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "注册", "用户名和密码不能为空");
        return;
    }
    if (password != confirm) {
        QMessageBox::warning(this, "注册", "两次输入的密码不一致");
        return;
    }
    submitting = true;
    if (ui->submitButton) ui->submitButton->setEnabled(false);
    if (ui->cancelButton) ui->cancelButton->setEnabled(false);
    emit submitRegister(username, password, phone);
}

void RegisterDialog::onRegisterResult(bool success, const QString& message) {
    // 收到结果后允许再次交互
    submitting = false;
    if (ui->submitButton) ui->submitButton->setEnabled(true);
    if (ui->cancelButton) ui->cancelButton->setEnabled(true);
    if (success) {
        QMessageBox::information(this, "注册成功", message.isEmpty() ? "注册成功" : message);
        accept();
    } else {
        QMessageBox::warning(this, "注册失败", message.isEmpty() ? "请重试" : message);
    }
}
