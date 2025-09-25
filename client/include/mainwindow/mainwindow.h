#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QTcpSocket>
#include "shopping/shoppingcart.h"
#include "chat/chatwindow.h"
#include <QElapsedTimer>
#include <QJsonArray>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setSocket(QTcpSocket *socket);
    // 设置当前登录用户名，供购物车与请求携带
    void setCurrentUsername(const QString &username) { currentUsername = username; }

private slots:
    void on_searchButton_clicked();
    void on_cartButton_clicked();
    void on_chatButton_clicked();
    void onProductClicked(int productId);
    void onReadyRead();
    // 分页控制
    void on_prevPage_clicked();
    void on_nextPage_clicked();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    ShoppingCart *cart;
    ChatWindow *chat;
    QString currentUsername; // 当前登录用户名，可为空表示匿名
    // simple debounce
    qint64 lastSearchMs = 0;
    qint64 lastAddMs = 0;
    QElapsedTimer monotonic;
    // 分页状态
    int currentPage = 1;
    int pageSize = 6;
    int totalProducts = 0;
    // 自适应网格重排支持
    QJsonArray lastRecommendations;
    QJsonArray lastResults;
    bool showingList = false; // true 表示当前展示的是“商品列表”，否则展示“推荐”
    qint64 lastReflowMs = 0;
    int lastColumns = 0;     // 上次网格列数，用于阈值判断
    
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
    void addToCart(int productId, int stock = -1);
    void requestProductsPage(int page);
    // 自适应列数与重排
    int computeColumns(int availableWidth) const;
    void reflowGrids();

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // MAINWINDOW_H