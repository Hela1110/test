#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include "shopping/shoppingcart.h"
#include "chat/chatwindow.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setSocket(QTcpSocket *socket);

private slots:
    void on_searchButton_clicked();
    void on_cartButton_clicked();
    void on_chatButton_clicked();
    void onProductClicked(int productId);
    void onReadyRead();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    ShoppingCart *cart;
    ChatWindow *chat;
    
    void setupUi();
    void loadCarousel();
    void loadRecommendations();
    void loadPromotions();
    void setupConnections();
};