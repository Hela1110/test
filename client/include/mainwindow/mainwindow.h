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

    // Placeholder and render helpers
    void renderCarouselPlaceholder();
    void renderRecommendationsPlaceholder();
    void renderPromotionsPlaceholder();
    void renderCarousel(const QJsonArray &images);
    void renderRecommendations(const QJsonArray &products);
    void renderPromotions(const QJsonArray &promotions);
    void clearLayout(QLayout *layout);
    QLayout* ensureVBoxLayout(QWidget *area);
    void renderSearchResults(const QJsonArray &results);
    void showProductDetail(const QJsonObject &product);
    void addToCart(int productId);
};

#endif // MAINWINDOW_H