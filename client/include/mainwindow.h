#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
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
    // 设置当前登录用户名，供后续请求携带
    void setCurrentUsername(const QString &username) { currentUsername = username; }
    // 主题模式
    enum class ThemeMode { Light, Dark };

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
    QString currentUsername; // 当前登录用户名，可为空表示匿名
    // 搜索状态：当处于搜索结果展示时，禁止自动请求分页列表
    bool searchActive = false;
    QString currentSearchKeyword;
    // 主题相关
    ThemeMode currentThemeMode = ThemeMode::Light;
    QPalette defaultAppPalette; // 用于恢复浅色模式
    QString defaultAppStyleSheet;
    bool paletteCaptured = false;
    
    void setupUi();
    void loadCarousel();
    void loadRecommendations();
    void loadPromotions();
    void setupConnections();
    // 主题切换实现
    void applyTheme(ThemeMode mode);
    void saveThemeToSettings(const QString &modeStr);
    QString loadThemeFromSettings() const;
};

#endif // MAINWINDOW_H