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
    void submitRegister(const QString& username, const QString& password);

public slots:
    void onRegisterResult(bool success, const QString& message);

private slots:
    void on_submitButton_clicked();

private:
    Ui::RegisterDialog* ui;
};

#endif // REGISTERDIALOG_H
