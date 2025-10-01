#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QTcpSocket>
#include "shopping/shoppingcart.h"
#include "chat/chatwindow.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QHash>
#include <QPixmap>
#include <QStringList>
#include <QVector>

class QTabBar;
class QNetworkAccessManager;
class QLabel;
class QTimer;

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
    void setCurrentUsername(const QString &username) { currentUsername = username; updateGreeting(); }

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
    void addToCart(int productId, int stock = -1, int size = -1);
    void requestProductsPage(int page);
    // 自适应列数与重排
    int computeColumns(int availableWidth) const;
    void reflowGrids();

    // 视图模式：首页/商城/购物车
    enum class ViewMode { Home, Mall, Cart };
    ViewMode currentView = ViewMode::Home;
    ViewMode lastNonCartView = ViewMode::Home; // 用于从购物车返回时恢复
    void showHomeView();
    void showMallView();
    void showCartView();
    void exitCartView();
    void showAccountView();
    void showOrdersView();
    void showChatView();
    void updateGreeting();
    void clearToFullPage(QWidget *page);
    // 顶部搜索栏显隐
    void setSearchBarVisible(bool visible);
    // Tabs helpers（左侧垂直 TabBar 作为主导航）
    QTabBar* ensureSideTabBar();
    void setTabActive(const QString &key);

    // 防止 Tab 切换递归触发
    bool tabSwitching = false;

    // 网络图片加载与缓存
    QNetworkAccessManager *http = nullptr;
    QHash<QString, QPixmap> imageCache;
    void setImageFromUrl(const QString &url, QLabel *label, const QSize &targetSize);
    static QPixmap scaledAspect(const QPixmap &src, const QSize &target);
    // 等比扩展并居中裁剪（类似 CSS background-size: cover）
    static QPixmap scaledCover(const QPixmap &src, const QSize &target);
    // HTTP 服务器基址（用于将相对路径如 /images/1.jpg 自动补齐为完整 URL）
    QString httpBase;
    QString resolveHttpUrl(const QString &url) const;

    // 首页问候语下方的头图支持
    QLabel *homeHeaderImage = nullptr;         // 首页头图标签（仅首页显示）
    QString homeHeaderImagePath;               // 本地图片路径
    QPixmap homeHeaderOriginal;                // 原始像素图，用于按窗口宽度等比缩放
    void applyHomeHeaderImage();               // 根据当前宽度将图片缩放并显示

    // 首页轮播图（carouselArea）使用本地图片实现
    QStringList carouselLocalPaths;            // 本地轮播图片路径列表
    QVector<QPixmap> carouselOriginals;        // 原始像素图缓存
    QLabel *carouselImageLabel = nullptr;      // 展示轮播的大图
    QLabel *carouselPrevLabel = nullptr;       // 左侧预览
    QLabel *carouselNextLabel = nullptr;       // 右侧预览
    QTimer *carouselTimer = nullptr;           // 自动轮播定时器
    int carouselIndex = 0;                     // 当前轮播索引
    QVector<QLabel*> carouselDots;             // 底部小圆点
    void setupLocalCarousel();                 // 创建控件、加载本地图片并启动轮播
    void refreshCarouselPixmap();              // 按容器宽度缩放当前帧
    void updateCarouselDots();                 // 根据当前索引刷新指示器

    // 首页活动文案交替显示
    QTimer *promoTimer = nullptr;
    QStringList promoMessages; // 例如：{"商城内商品单次订单每满200减20", "部分商品折扣中"}
    int promoIndex = 0;

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAINWINDOW_H