#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

namespace Ui { class RegisterDialog; }

class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(QWidget* parent = nullptr);
    ~RegisterDialog();

signals:
    void submitRegister(const QString& username, const QString& password, const QString& phone, const QString& email);

public slots:
    void onRegisterResult(bool success, const QString& message);

private slots:
    void on_submitButton_clicked();

private:
    Ui::RegisterDialog* ui;
    bool submitting = false;
};

#endif // REGISTERDIALOG_H
