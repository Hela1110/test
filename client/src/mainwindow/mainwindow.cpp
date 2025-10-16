#include "mainwindow/mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QDebug>
#include <QAbstractButton>
#include <QInputDialog>
#include <QSpinBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QGridLayout>
#include <QFrame>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTabBar>
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QBuffer>
#include <QPixmap>
#include <QLabel>
#include <QTimer>
#include <QPointer>
#include <QHash>
#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QProgressBar>
// 将一帧加入微批处理队列，并在短时间窗口内批量写出
void MainWindow::enqueueFrame(const QByteArray &frame) {
    // 允许在未连接时也暂存，等待 timer 重试写出
    pendingFrames.push_back(frame);
    if (!sendFlushTimer) {
        sendFlushTimer = new QTimer(this);
        sendFlushTimer->setSingleShot(true);
        connect(sendFlushTimer, &QTimer::timeout, this, [this]() {
            if (pendingFrames.isEmpty()) return;
            if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
                // 网络未就绪，稍后重试（保留队列）
                sendFlushTimer->start(200);
                return;
            }
            QByteArray batch;
            batch.reserve(pendingFrames.size() * 64);
            for (const auto &f : pendingFrames) batch.append(f);
            pendingFrames.clear();
            socket->write(batch);
        });
    }
    if (!sendFlushTimer->isActive()) sendFlushTimer->start(60);
}

// 商品数据缓存：用于直接点击“加入购物车”时获取最新的尺码库存
QHash<int,QJsonObject> *productCachePtr = nullptr;  // 推荐/首页区域
QHash<int,QJsonObject> *productCachePtr2 = nullptr; // 商品列表区域

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(nullptr)
    , cart(nullptr)
    , chat(nullptr)
{
    ui->setupUi(this);
    // 初始化网络图片加载器
    http = new QNetworkAccessManager(this);
    // 默认 HTTP 基址为空，等 socket 建立后从对端地址推断（例如 http://127.0.0.1:8081）
    httpBase.clear();
    qInfo() << "MainWindow constructed";
    monotonic.start();
    setupUi();
    setupConnections();
    // 轻量重连定时器
    reconnectTimer = new QTimer(this);
    reconnectTimer->setSingleShot(true);
    connect(reconnectTimer, &QTimer::timeout, this, [this]{
        if (!socket) return;
        auto st = socket->state();
        if (st == QAbstractSocket::ConnectedState) return;
        if (st == QAbstractSocket::UnconnectedState) {
            qInfo() << "reconnect attempt" << reconnectAttempts << "to" << socketHost << socketPort;
            socket->connectToHost(socketHost, socketPort);
            return;
        }
        // 其它过渡态（HostLookup/Connecting/Closing），不再强行断开，延后重试避免产生 RST
        qInfo() << "reconnect postponed, state=" << st;
        reconnectTimer->start(200);
    });

    // Tab 切换防抖队列
    tabSwitchTimer = new QTimer(this);
    tabSwitchTimer->setSingleShot(true);
    tabSwitchTimer->setInterval(50);
    connect(tabSwitchTimer, &QTimer::timeout, this, [this]{
        if (pendingTabKey.isEmpty()) return;
        const QString key = pendingTabKey; pendingTabKey.clear();
        switchToTabKey(key);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
    if (cart) {
        delete cart;
    }
    if (chat) {
        delete chat;
    }
}

// 初始化窗口的自定义部分（与 Qt UI 生成的 ui->setupUi 区分开）
void MainWindow::setupUi()
{
    // 基本窗口属性
    setWindowTitle("购物系统");
    resize(1366, 900);

    // 子组件延迟创建
    cart = nullptr;
    chat = nullptr;

    // 左侧垂直 Tab 导航（首页/商城/购物车/历史订单/客服/个人中心）
    ensureSideTabBar();

    // 在问候语旁添加红色漂浮活动文案
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        // 查找问候标签所在的索引
        int greetIndex = -1;
        for (int i=0;i<root->count();++i) {
            if (root->itemAt(i)->widget() == ui->greetingLabel) { greetIndex = i; break; }
        }
        // 先从原布局中移除 greetingLabel 的项，避免重复持有
        if (greetIndex >= 0) {
            auto *item = root->takeAt(greetIndex);
            // item 中持有的是 greetingLabel，对象仍然存活，无需 delete
            delete item; item = nullptr;
        }
        // 构造一行：左侧问候，右侧活动文案
        auto *row = new QWidget(this);
        row->setObjectName("greetingRow");
        auto *hl = new QHBoxLayout(row); hl->setContentsMargins(0,0,0,0); hl->setSpacing(8);
        // 将原 greetingLabel 放入该行
        ui->greetingLabel->setParent(row);
        hl->addWidget(ui->greetingLabel, 0, Qt::AlignLeft);
        // 活动文案（红色加粗，不使用 Qt 不支持的 CSS 动画）
    auto *promo = new QLabel(row);
        promo->setObjectName("promoFloatingLabel");
    // 初始文本由定时器轮播填充
    promo->setText("");
        promo->setStyleSheet("color:#E53935;font-weight:700;");
        hl->addSpacing(8);
        hl->addWidget(promo, 0, Qt::AlignLeft);
        hl->addStretch(1);
    // 右上角主题切换
    auto *themeLabel = new QLabel(tr("主题"), row);
    themeLabel->setStyleSheet("color:#666; margin-right:4px;");
    auto *themeBox = new QComboBox(row);
        themeBox->setObjectName("themeModeBox");
    themeBox->addItem(tr("白天"), static_cast<int>(MainWindow::ThemeMode::Light));
    themeBox->addItem(tr("夜间"), static_cast<int>(MainWindow::ThemeMode::Dark));
        themeBox->setToolTip(tr("切换浅色/深色主题"));
    auto *themeWrap = new QWidget(row);
    auto *wrapLay = new QHBoxLayout(themeWrap); wrapLay->setContentsMargins(0,0,0,0); wrapLay->setSpacing(4);
    wrapLay->addWidget(themeLabel);
    wrapLay->addWidget(themeBox);
    hl->addWidget(themeWrap, 0, Qt::AlignRight);
        // 将新行插入原位置（若未知则放顶部）
        if (greetIndex >= 0) root->insertWidget(greetIndex, row);
        else root->insertWidget(0, row);

        // 初始化活动文案轮播
        promoMessages = QStringList{
            tr("商城内商品单次订单每满200减20"),
            tr("部分商品折扣中")
        };
        promoIndex = 0;
        if (!promoTimer) promoTimer = new QTimer(this);
        promoTimer->setInterval(2000);
        QObject::disconnect(promoTimer, nullptr, nullptr, nullptr);
        connect(promoTimer, &QTimer::timeout, this, [this]() {
            auto *label = findChild<QLabel*>("promoFloatingLabel");
            if (!label) return;
            if (promoMessages.isEmpty()) { label->clear(); return; }
            label->setText(promoMessages.at(promoIndex % promoMessages.size()));
            promoIndex = (promoIndex + 1) % qMax(1, promoMessages.size());
        });
        // 立即触发一次并启动
        if (auto label = findChild<QLabel*>("promoFloatingLabel")) {
            if (!promoMessages.isEmpty()) label->setText(promoMessages.first());
        }
        promoTimer->start();

        // 主题初始化：捕获默认调色板并从设置恢复
        if (!paletteCaptured) {
            defaultAppPalette = qApp->palette();
            defaultAppStyleSheet = qApp->styleSheet();
            paletteCaptured = true;
        }
        const QString themeStr = loadThemeFromSettings();
    if (themeStr == QLatin1String("dark")) currentThemeMode = MainWindow::ThemeMode::Dark; else currentThemeMode = MainWindow::ThemeMode::Light;
        {
            QSignalBlocker blocker(themeBox);
            themeBox->setCurrentIndex(currentThemeMode == MainWindow::ThemeMode::Dark ? 1 : 0);
        }
        applyTheme(currentThemeMode);
        connect(themeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, themeBox](int idx){
            MainWindow::ThemeMode mode = static_cast<MainWindow::ThemeMode>(themeBox->itemData(idx).toInt());
            applyTheme(mode);
            saveThemeToSettings(mode == MainWindow::ThemeMode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
        });
    }
}

void MainWindow::setSocket(QTcpSocket *s)
{
    socket = s;
    qInfo() << "MainWindow setSocket, state=" << socket->state();
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::stateChanged, this, [this](QAbstractSocket::SocketState st){
        qInfo() << "Socket stateChanged:" << st;
    });
    // 记录当前连接参数用于重连（优先环境变量）
    QString envHost = qEnvironmentVariable("APP_HOST").trimmed();
    if (!envHost.isEmpty()) socketHost = envHost; else socketHost = QStringLiteral("127.0.0.1");
    bool ok=false; int envSockPort = qEnvironmentVariableIntValue("APP_SOCKET_PORT", &ok);
    socketPort = static_cast<quint16>((ok && envSockPort>0)? envSockPort : 8080);
    connect(socket, &QTcpSocket::disconnected, this, [this]{
        qWarning() << "Socket disconnected. activeTab=" << activeTabKey;
        statusBar()->showMessage(tr("网络连接已断开，正在尝试重连…"), 3000);
        scheduleReconnect();
    });
    // 增加错误日志，定位潜在 RST 前的最后错误
    connect(socket, qOverload<QAbstractSocket::SocketError>(&QTcpSocket::errorOccurred), this, [this](QAbstractSocket::SocketError e){
        Q_UNUSED(e);
        qCritical() << "Socket error in MainWindow:" << socket->errorString() << " state=" << socket->state();
    });
    // 通过 TCP 连接对端推断静态资源 HTTP 基址（同一台服务器通常同时提供 9090 套接字与 8081 HTTP）
    // 若对端是 127.0.0.1 或 ::1，则使用 localhost
    if (socket) {
        // 首选环境变量（由 StartAll 传递），否则根据对端地址推断
        QString envHost = qEnvironmentVariable("APP_HOST");
        if (envHost.trimmed().isEmpty()) envHost = QString();
        bool ok = false; int envHttpPort = qEnvironmentVariableIntValue("APP_HTTP_PORT", &ok);
        const QHostAddress addr = socket->peerAddress();
        QString host;
        if (!envHost.isEmpty()) {
            host = envHost;
        } else if (addr.isNull()) {
            host = QStringLiteral("localhost");
        } else if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6) {
            host = QStringLiteral("localhost");
        } else {
            host = addr.toString();
        }
        int httpPort = (ok && envHttpPort > 0) ? envHttpPort : 8081;
        httpBase = QStringLiteral("http://%1:%2").arg(host).arg(httpPort);
        qInfo() << "HTTP base resolved to" << httpBase;
    }
    // 刚设置好 socket 时，后台拉一次首页数据（不切 UI，不重复）
    if (!homeDataRequested) {
        homeDataRequested = true;
        loadCarousel();
        loadRecommendations();
    }
}

void MainWindow::scheduleReconnect()
{
    if (!socket) return;
    if (socket->state() == QAbstractSocket::ConnectedState) return;
    // 最多尝试 3 次，间隔 0.5s, 1s, 2s
    if (reconnectAttempts >= 3) {
        qWarning() << "reconnect attempts exceeded";
        statusBar()->showMessage(tr("重连失败，请检查服务器状态"), 4000);
        reconnectAttempts = 0;
        return;
    }
    int delays[] = {500, 1000, 2000};
    int ms = delays[qBound(0, reconnectAttempts, 2)];
    ++reconnectAttempts;
    reconnectTimer->start(ms);
}

// 创建并插入左侧垂直 TabBar；如已存在则复用
QTabBar* MainWindow::ensureSideTabBar()
{
    if (auto exist = findChild<QTabBar*>("leftTabBar")) return exist;
    auto *tab = new QTabBar(this);
    tab->setObjectName("leftTabBar");
    tab->setExpanding(true);
    tab->setMovable(false);
    tab->setDrawBase(false);
    tab->setShape(QTabBar::RoundedWest);
    tab->setFocusPolicy(Qt::NoFocus);

    // 插入到左侧栏顶部
    if (auto left = ui->centralwidget->findChild<QVBoxLayout*>("leftSidebarLayout")) {
        left->insertWidget(0, tab);
    }
    // 侧栏 Tab 定义
    struct TabDef { const char* key; const char* text; const char* icon; };
    static const TabDef defs[] = {
        {"home",   "首页",       ":/icons/home.svg"},
        {"mall",   "发现好物",   ":/icons/mall.svg"},
        {"cart",   "购物车",     ":/icons/cart.svg"},
        {"orders", "历史订单",   ":/icons/orders.svg"},
        {"chat",   "客服/聊天",  ":/icons/chat.svg"},
        {"account","个人中心",   ":/icons/account.svg"}
    };
    for (const auto &d : defs) {
        int idx = tab->addTab(QIcon(QString::fromLatin1(d.icon)), tr(d.text));
        tab->setTabData(idx, QString::fromLatin1(d.key));
    }

    // 样式统一交由全局 QSS 管控（见 resources/styles/style.qss 与 applyTheme 的 darkCss）

    connect(tab, &QTabBar::currentChanged, this, [this, tab](int idx){
        if (idx < 0) return;
        const QString key = tab->tabData(idx).toString();
        qint64 now = monotonic.elapsed();
        if (tabSwitching || (now - lastTabSwitchMs) < 250) { pendingTabKey = key; tabSwitchTimer->start(50); return; }
        pendingTabKey = key; tabSwitchTimer->start(50);
    });
    tab->setCurrentIndex(0);
    activeTabKey = QStringLiteral("home");
    return tab;
}

void MainWindow::switchToTabKey(const QString &key)
{
    if (key.isEmpty()) return;
    if (tabSwitching) { pendingTabKey = key; tabSwitchTimer->start(0); return; }
    tabSwitching = true;
    lastTabSwitchMs = monotonic.elapsed();
    activeTabKey = key;
    if (key == QLatin1String("home")) showHomeView();
    else if (key == QLatin1String("mall")) showMallView();
    else if (key == QLatin1String("cart")) showCartView();
    else if (key == QLatin1String("orders")) showOrdersView();
    else if (key == QLatin1String("chat")) showChatView();
    else if (key == QLatin1String("account")) showAccountView();
    tabSwitching = false;
}

// 外部调用以同步 Tab 选中态，避免递归
void MainWindow::setTabActive(const QString &key)
{
    if (tabSwitching) return;
    auto *tab = ensureSideTabBar();
    if (!tab) return;
    for (int i = 0; i < tab->count(); ++i) {
        if (tab->tabData(i).toString() == key) {
            QSignalBlocker blocker(tab);
            tab->setCurrentIndex(i);
            activeTabKey = key;
            break;
        }
    }
}

void MainWindow::setupConnections()
{
    // 连接搜索按钮信号
    connect(ui->searchButton, &QAbstractButton::clicked, this, &MainWindow::on_searchButton_clicked);
    // 搜索框占位提示与回车触发
    if (ui->searchInput) ui->searchInput->setPlaceholderText(tr("搜索商品名称…"));
    connect(ui->searchInput, &QLineEdit::returnPressed, this, &MainWindow::on_searchButton_clicked);
    
    qInfo() << "setupConnections: searchButton=" << static_cast<void*>(ui->searchButton)
             << "cartButton=" << static_cast<void*>(ui->cartButton)
             << "chatButton=" << static_cast<void*>(ui->chatButton);
    // 再用 findChild 兜底查找（防止 ui 成员名不一致）
    if (auto fb = findChild<QAbstractButton*>("cartButton")) {
        if (fb != ui->cartButton) {
            qInfo() << "setupConnections: findChild(cartButton) found different instance" << static_cast<void*>(fb);
        } else {
            qInfo() << "setupConnections: findChild(cartButton) matches ui->cartButton";
        }
    } else {
        qInfo() << "setupConnections: findChild(cartButton) not found";
    }

    // 购物车/客服按钮改走 Qt AutoConnect 的 on_* 槽，避免重复触发
    // 这里仅保留轻量日志，不绑定额外的 clicked 行为
    if (ui->cartButton) {
        connect(ui->cartButton, &QAbstractButton::pressed, this, [](){ qInfo() << "cartButton pressed"; });
        connect(ui->cartButton, &QAbstractButton::released, this, [](){ qInfo() << "cartButton released"; });
    }

    // 账号与订单按钮（内嵌页面）
    if (auto ab = findChild<QAbstractButton*>("accountButton")) {
        connect(ab, &QAbstractButton::clicked, this, [this]{ showAccountView(); });
    }
    if (auto ob = findChild<QAbstractButton*>("ordersButton")) {
        connect(ob, &QAbstractButton::clicked, this, [this]{ showOrdersView(); });
    }

    // 首页/进入商城 切换
    // 侧栏已改为 Tab 导航，隐藏旧按钮避免双导航
    if (auto home = findChild<QWidget*>("homeButton")) home->hide();
    if (auto mall = findChild<QWidget*>("enterMallButton")) mall->hide();
    if (auto ab = findChild<QWidget*>("accountButton")) ab->hide();
    if (auto ob = findChild<QWidget*>("ordersButton")) ob->hide();
    if (auto cb = findChild<QWidget*>("chatButton")) cb->hide();
    if (auto car = findChild<QWidget*>("cartButton")) car->hide();

    // 底部分页条：跳转按钮
    if (auto btn = findChild<QPushButton*>("gotoPageButton")) {
        connect(btn, &QPushButton::clicked, this, [this]{
            if (auto spin = findChild<QSpinBox*>("gotoPageSpin")) {
                int page = spin->value();
                int totalPages = (pageSize>0) ? ((totalProducts + pageSize - 1) / pageSize) : 1;
                if (page < 1) page = 1;
                if (page > totalPages) page = totalPages;
                requestProductsPage(page);
            }
        });
    }
    // 也支持直接改动 SpinBox 立即跳页（做一个小的防抖）
    if (auto spin = findChild<QSpinBox*>("gotoPageSpin")) {
        static qint64 lastJumpMs = 0;
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val){
            Q_UNUSED(val);
            static QElapsedTimer t; if (!t.isValid()) t.start();
            qint64 now = t.elapsed();
            static qint64 last = 0;
            if (now - last < 200) return;
            last = now;
            requestProductsPage(qMax(1, val));
        });
    }

    // 可选：如果 UI 暂无分页按钮，这里用键盘快捷键或在状态栏增加上一页/下一页动作
    // 初次加载：作为首页，隐藏分页器（商城里再显示）并加载首页模块
    showHomeView();
    updateGreeting();
    // 不再在主界面初始化时预取历史订单，避免与其它初始化请求叠加
}

// 主题应用
void MainWindow::applyTheme(MainWindow::ThemeMode mode)
{
    if (!paletteCaptured) {
        defaultAppPalette = qApp->palette();
        defaultAppStyleSheet = qApp->styleSheet();
        paletteCaptured = true;
    }
    currentThemeMode = mode;
    if (mode == MainWindow::ThemeMode::Light) {
        qApp->setPalette(defaultAppPalette);
        qApp->setStyleSheet(defaultAppStyleSheet);
        this->setStyleSheet("");
        return;
    }
    // Dark — 高级深色主题（与浅色版风格一致、对比度与可读性优化）
    QPalette pal = defaultAppPalette;
    pal.setColor(QPalette::Window, QColor(26,26,28));           // 背景
    pal.setColor(QPalette::WindowText, QColor(224,224,226));
    pal.setColor(QPalette::Base, QColor(22,22,24));             // 输入底
    pal.setColor(QPalette::AlternateBase, QColor(34,34,38));
    pal.setColor(QPalette::ToolTipBase, QColor(34,34,38));
    pal.setColor(QPalette::ToolTipText, QColor(232,232,236));
    pal.setColor(QPalette::Text, QColor(224,224,226));
    pal.setColor(QPalette::Button, QColor(40,40,44));
    pal.setColor(QPalette::ButtonText, QColor(232,232,236));
    pal.setColor(QPalette::BrightText, QColor(255,68,68));
    pal.setColor(QPalette::Highlight, QColor(22,119,255));
    pal.setColor(QPalette::HighlightedText, QColor(255,255,255));
    qApp->setPalette(pal);
    // 与浅色 style.qss 对齐的部件级样式（仅更换色板与对比度）
    QString darkCss = R"(
        /* ========= 全局 ========= */
        QWidget { font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", Arial, sans-serif; color:#e0e0e2; }
        QMainWindow, QDialog, QMessageBox, QWidget#centralwidget { background: #1a1a1c; }

        /* 标题/标签 */
        QLabel { color: #e0e0e2; }
        QLabel[objectName="promoFloatingLabel"] { color: #ff6b6b; font-weight: 700; }

        /* 按钮（与浅色风格一致，仅替换色板） */
        QPushButton { background: #1677ff; color: #fff; border: 1px solid #1677ff; padding: 7px 14px; border-radius: 8px; }
        QPushButton:hover { background: #3c8cff; border-color: #3c8cff; }
        QPushButton:pressed { background: #0e5ad1; border-color: #0e5ad1; }
        QPushButton:disabled { background: #2f3b4d; border-color: #2f3b4d; color: #8b96a6; }
        /* 次级按钮（扁平边框，一致的尺寸与圆角） */
        QPushButton[flat="true"], QPushButton.secondary {
            background: transparent; color: #8ab4ff; border: 1px solid #8ab4ff; padding: 7px 14px; border-radius: 6px;
        }
        QPushButton[flat="true"]:hover, QPushButton.secondary:hover { background: rgba(22,119,255,0.12); }
        QPushButton[flat="true"]:pressed, QPushButton.secondary:pressed { background: rgba(22,119,255,0.18); }

        /* 输入类 */
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit {
            background: #161618; color:#e0e0e2; border:1px solid #3a3f46; border-radius:6px; padding:6px 8px;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus {
            border-color: #1677ff; box-shadow: 0 0 0 3px rgba(22,119,255,0.22);
        }
        QComboBox::drop-down { width: 26px; border-left: 1px solid #3a3f46; border-top-right-radius: 6px; border-bottom-right-radius: 6px; }
        QComboBox::down-arrow {
            image: none; width: 0; height: 0; margin-right: 9px;
            border-left: 6px solid transparent; border-right: 6px solid transparent; border-top: 7px solid #a6adbb;
        }

        /* SpinBox（与浅色版相同的三角箭头与尺寸） */
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            width: 24px; border-left: 1px solid #3a3f46; border-top-right-radius: 6px; border-bottom-right-radius: 0;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            width: 24px; border-left: 1px solid #3a3f46; border-bottom-right-radius: 6px; border-top-right-radius: 0;
        }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
            background: rgba(22,119,255,0.12);
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: none; width: 0; height: 0;
            border-left: 6px solid transparent; border-right: 6px solid transparent; border-bottom: 7px solid #a6adbb;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: none; width: 0; height: 0;
            border-left: 6px solid transparent; border-right: 6px solid transparent; border-top: 7px solid #a6adbb;
        }
        QSpinBox::up-arrow:disabled, QDoubleSpinBox::up-arrow:disabled,
        QSpinBox::down-arrow:disabled, QDoubleSpinBox::down-arrow:disabled {
            border-color: #4a4f59;
        }

        /* 复选/单选 */
        QCheckBox, QRadioButton { spacing: 6px; }
        QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
        QCheckBox::indicator { border: 1px solid #3a3f46; border-radius: 3px; background: #161618; }
        QCheckBox::indicator:checked { background: #1677ff; image: none; }
        QRadioButton::indicator { border: 1px solid #3a3f46; border-radius: 8px; background: #161618; }
        QRadioButton::indicator:checked { background: #1677ff; }

        /* GroupBox */
        QGroupBox { border: 1px solid #2a2e33; border-radius: 8px; margin-top: 12px; background: #1f2023; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; color: #a6adbb; }

        /* 表格 / 列表 */
        QTableView, QListView, QTreeView {
            background: #1f2023; border: 1px solid #2a2e33; border-radius: 8px;
            selection-background-color: #1c2b45; selection-color: #e0e0e2; gridline-color: #2a2e33;
        }
        QHeaderView::section { background: #23252a; color: #b8bfcc; padding: 8px 10px; border: 1px solid #2a2e33; }
        QTableView::item { padding: 6px; }
        QTableView::item:selected { background: #1c2b45; }

        /* 状态栏 */
        QStatusBar { background: #1f2023; border-top: 1px solid #2a2e33; }
        QStatusBar QLabel { color: #a6adbb; }

        /* Tab */
        QTabWidget::pane { border: 1px solid #2a2e33; border-radius: 8px; top: -1px; }
        QTabBar { font-weight: 500; }
        QTabBar::tab {
            background: #212226; color: #d8dbe2; border: 1px solid #2a2e33; border-left-width: 3px; border-radius: 12px;
            padding: 10px 12px; margin: 6px 8px; min-height: 34px; min-width: 96px; qproperty-iconSize: 18px 18px;
        }
        QTabBar::tab:selected { background: #24262b; border-left-color: #1677ff; color: #8ab4ff; }
        QTabBar::tab:hover { background: #262a30; }

        /* 左侧竖向 TabBar（对象名 leftTabBar）在暗色下更明显的选中条 */
        QTabBar#leftTabBar { margin: 6px 0; }
        QTabBar#leftTabBar::tab { min-height: 84px; min-width: 24px; text-align: center; }
        QTabBar#leftTabBar::tab:selected { box-shadow: inset 3px 0 0 #1677ff; }

        /* 滚动条 */
        QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 0; }
        QScrollBar::handle:vertical { background: #3a4150; border-radius: 6px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #4a5263; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 0 4px; }
        QScrollBar::handle:horizontal { background: #3a4150; border-radius: 6px; min-width: 30px; }
        QScrollBar::handle:horizontal:hover { background: #4a5263; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        /* 工具提示 */
        QToolTip { background: #0f172a; color: #fff; border: 1px solid #0f172a; border-radius: 6px; padding: 6px 8px; }

        /* 卡片容器（暗色白卡等价） */
        QWidget.card, QWidget#ordersPage, QWidget#accountPage, QWidget#chatPage {
            background: #1f2023; border: 1px solid #2a2e33; border-radius: 12px; padding: 12px;
        }

        /* 商城页页尾提示（暗色） */
        QWidget#mallFooter QLabel#mallFooterLabel { color: #a6adbb; }
        QWidget#mallFooter QFrame#mallFooterDivider { background: #2a2e33; height: 1px; }

    /* 商城商品卡片（暗色卡片） */
    QFrame#mallCard { border: 1px solid #2a2e33; border-radius: 10px; background: #1f2023; }
    QFrame#mallCard QLabel { color: #e0e0e2; font-size: 13px; }
    QLabel#mallImageHolder { background: #24262b; border: 1px solid #2a2e33; border-radius: 8px; }
    )";
    qApp->setStyleSheet(darkCss);
}

void MainWindow::saveThemeToSettings(const QString &modeStr)
{
    QSettings st("ShoppingApp", "Client");
    st.setValue("ui/theme", modeStr);
}

QString MainWindow::loadThemeFromSettings() const
{
    QSettings st("ShoppingApp", "Client");
    return st.value("ui/theme", "light").toString();
}

void MainWindow::loadCarousel()
{
    // TODO: 从服务器加载轮播图数据
    if (socket) {
        QJsonObject request;
        request["type"] = "get_carousel";
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    enqueueFrame(payload);
    }
}

void MainWindow::loadRecommendations()
{
    // TODO: 从服务器加载推荐商品
    if (socket) {
        QJsonObject request;
        request["type"] = "get_recommendations";
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    enqueueFrame(payload);
    }
}

void MainWindow::loadPromotions()
{
    // TODO: 从服务器加载促销信息
    if (socket) {
        QJsonObject request;
        request["type"] = "get_promotions";
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    enqueueFrame(payload);
    }
}

void MainWindow::on_searchButton_clicked()
{
    QString keyword = ui->searchInput->text().trimmed();
    // 300ms 防抖，避免双击/回车重复
    if (monotonic.elapsed() - lastSearchMs < 300) return;
    lastSearchMs = monotonic.elapsed();
    // 空关键词表示退出搜索模式，恢复正常“发现好物”滚动列表
    if (keyword.isEmpty()) {
        searchActive = false;
        currentSearchKeyword.clear();
        lastResults = QJsonArray{};
        if (currentView != ViewMode::Mall) showMallView();
        else {
            // 已在商城页则直接刷新到第 1 页
            requestProductsPage(1);
        }
        statusBar()->showMessage(tr("显示全部商品"), 1200);
        return;
    }
    // 进入商城页容器布局，但若是搜索，不触发默认的第一页拉取
    searchActive = true;
    currentSearchKeyword = keyword;
    currentPage = 1;
    if (currentView != ViewMode::Mall) showMallView();
    // 立即清空“已加载列表”状态，准备渲染搜索结果
    lastResults = QJsonArray{};
    mallLoading = true; updateMallFooter();
    if (socket) {
        QJsonObject request;
        // 与文档对齐：使用 search_products；服务器仍兼容旧的 search
        request["type"] = "search_products";
        request["keyword"] = keyword;
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    enqueueFrame(payload);
        statusBar()->showMessage(keyword.isEmpty() ? tr("正在获取全部商品…") : tr("正在搜索：%1").arg(keyword), 2000);
    }
}

void MainWindow::on_cartButton_clicked()
{
    // 购物车作为主窗内的“页面”展示
    if (!socket) {
        QMessageBox::warning(this, tr("提示"), tr("未连接服务器，无法打开购物车"));
        return;
    }
    showCartView();
}

void MainWindow::on_chatButton_clicked()
{
    showChatView();
}

void MainWindow::onProductClicked(int productId)
{
    // 发送获取商品详情请求
    if (socket) {
        QJsonObject request;
        request["type"] = "get_product_detail";
        request["product_id"] = productId;
        
    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    enqueueFrame(payload);
    }
}

void MainWindow::onReadyRead()
{
    if (!socket) return;
    // 追加到缓冲区，并按行处理完整帧；保留最后一个未以'\n'结尾的半帧
    recvBuf += socket->readAll();
    int idx;
    while ((idx = recvBuf.indexOf('\n')) != -1) {
        QByteArray one = recvBuf.left(idx);
        recvBuf.remove(0, idx + 1);
        const QByteArray trimmed = one.trimmed();
        if (trimmed.isEmpty()) continue;
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(trimmed, &err);
        if (err.error != QJsonParseError::NoError) continue;
    QJsonObject response = doc.object();
    QString type = response["type"].toString();
    // 调试日志：记录首页相关的返回
    if (type == QLatin1String("carousel_data")) {
        qInfo() << "recv carousel_data images=" << response.value("images").toArray().size();
    } else if (type == QLatin1String("recommendations")) {
        qInfo() << "recv recommendations products=" << response.value("products").toArray().size();
    } else if (type == QLatin1String("promotions")) {
        qInfo() << "recv promotions count=" << response.value("promotions").toArray().size();
    } else if (type == QLatin1String("products_response")) {
        qInfo() << "recv products_response total=" << response.value("total").toInt()
                << " page=" << currentPage;
    }
    if (type == "carousel_data") {
        QJsonArray images = response["images"].toArray();
        renderCarousel(images);
    }
    else if (type == "recommendations") {
        QJsonArray products = response["products"].toArray();
        renderRecommendations(products);
    }
    else if (type == "promotions") {
        QJsonArray promotions = response["promotions"].toArray();
        renderPromotions(promotions);
    }
    else if (type == "search_results") {
        // 兼容旧协议：服务端返回 search_results
        if (currentView != ViewMode::Mall) showMallView();
        searchActive = true;
        mallLoading = false;
        mallHasMore = false; // 搜索结果不再自动翻页
        QJsonArray results = response["results"].toArray();
        renderSearchResults(results);
        updateMallFooter();
    }
    else if (type == "product_detail") {
        QJsonObject product = response["product"].toObject();
        // 若处于“等待尺码弹窗”的流程，则优先走尺码选择
        if (waitingSizeSelectProductId > 0 && product.value("product_id").toInt() == waitingSizeSelectProductId) {
            handleProductDetailForSizeSelect(product);
            waitingSizeSelectProductId = -1;
        } else {
            showProductDetail(product);
        }
    }
    else if (type == "add_to_cart_response") {
        const bool ok = response.value("success").toBool();
        const QString msg = response.value("message").toString();
        // 弹窗提示
        QMessageBox box(this);
        box.setWindowTitle(ok ? tr("已加入购物车") : tr("加入失败"));
        box.setIcon(ok ? QMessageBox::Information : QMessageBox::Warning);
        box.setText(ok ? (msg.isEmpty()? tr("加入购物车成功！"): msg)
                       : (msg.isEmpty()? tr("加入购物车失败，请稍后重试"): msg));
        box.setStandardButtons(QMessageBox::Ok);
        auto f = box.font(); f.setPointSize(f.pointSize()+1); box.setFont(f);
        box.exec();
        // 成功时刷新购物车
        if (ok && cart) {
            cart->refreshCart();
        }
    }
    else if (type == "products_response") {
        // 仅在商城视图中处理商品列表，避免后台更新覆盖其他页面
        if (currentView != ViewMode::Mall) {
            qInfo() << "skip products_response due to inactive view" << static_cast<int>(currentView);
            continue;
        }
        // 将文档格式的 products 列表归一化为现有 renderSearchResults 接受的结构
        QJsonArray products = response.value("products").toArray();
        totalProducts = response.value("total").toInt();
        QJsonArray results;
        for (const auto &v : products) {
            const auto o = v.toObject();
            QJsonObject item;
            item["product_id"] = o.value("id").toInt();
            item["name"] = o.value("name").toString();
            item["price"] = o.value("price").toDouble();
            if (o.contains("description")) item["description"] = o.value("description");
            if (o.contains("imageUrl")) item["image_url"] = o.value("imageUrl");
            if (o.contains("stock")) item["stock"] = o.value("stock");
            if (o.contains("sales")) item["sales"] = o.value("sales");
            if (o.contains("onSale")) item["onSale"] = o.value("onSale");
            if (o.contains("discountPrice")) item["discountPrice"] = o.value("discountPrice");
            if (o.contains("isNew")) item["isNew"] = o.value("isNew");
            results.append(item);
        }
        // 首次或刷新：直接渲染；后续页：合并并渲染
        if (currentPage <= 1 || lastResults.isEmpty()) {
            renderSearchResults(results);
        } else {
            int before = lastResults.size();
            // 简单去重合并：按 product_id 排重，避免重复导致“永远还有更多”
            QSet<int> existing;
            for (const auto &v : lastResults) existing.insert(v.toObject().value("product_id").toInt());
            QJsonArray merged = lastResults;
            int added = 0;
            for (const auto &v : results) {
                int pid = v.toObject().value("product_id").toInt();
                if (!existing.contains(pid)) { merged.append(v); existing.insert(pid); ++added; }
            }
            renderSearchResults(merged);
            if (added == 0) {
                mallHasMore = false; // 这一页没有新增内容，视为已到末尾
            }
        }
        // 更新滚动加载状态（多信号兜底计算）
        mallLoading = false;
    bool hasMore = false;
        // 1) 服务器显式给出总页数或 hasMore
        const int totalPagesHint = response.value("totalPages").toInt(-1);
        const bool hasMoreHint = response.value("hasMore").toBool(false);
        const int pageResp = response.value("page").toInt(currentPage);
        if (totalPagesHint >= 0) {
            hasMore = pageResp < totalPagesHint;
        } else if (response.contains("hasMore")) {
            hasMore = hasMoreHint;
        } else if (totalProducts > 0 && pageSize > 0) {
            int totalPages = (totalProducts + pageSize - 1) / pageSize;
            hasMore = pageResp < totalPages;
        } else {
            // 4) 回退：当前页条目数达到 pageSize 视为可能还有下一页
            hasMore = (products.size() >= pageSize);
        }
    // 若当前页条目少于 pageSize，认为已到最后一页（兜底，优先生效）
    if (products.size() < pageSize) hasMore = false;
    // 收到空页也视为没有更多
    if (products.isEmpty()) hasMore = false;
    mallHasMore = hasMore;
        qInfo() << "products_response pages calc:" << "pageResp=" << pageResp
                << "totalProducts=" << totalProducts << "pageSize=" << pageSize
                << "hasMore=" << mallHasMore;
        updateMallFooter();
        // 如果当前内容不足以出现滚动条，尝试预取下一页
        QTimer::singleShot(0, this, [this]{ tryLoadNextProductsOnScroll(); });
    }
    else if (type == "account_info") {
        // 仅在当前仍停留在“个人中心”页时处理，避免离开后回调构建/访问已删除的控件
        if (activeTabKey != QLatin1String("account")) {
            qInfo() << "skip account_info due to inactive tab" << activeTabKey;
            continue;
        }
        // 在 accountPage 中展示并可直接编辑保存
        QWidget *page = findChild<QWidget*>("accountPage");
        if (!page) {
            page = new QWidget(this);
            page->setObjectName("accountPage");
            auto *v = new QVBoxLayout(page);
            auto *topBar = new QHBoxLayout();
            auto *title = new QLabel(tr("个人中心 ✨"), page); title->setStyleSheet("font-weight:600;font-size:16px;");
            auto *back = new QPushButton(tr("返回"), page);
            auto *saveToggle = new QPushButton(tr("修改"), page); saveToggle->setObjectName("saveTopButton");
            // 统一按钮样式，避免看起来像透明
            saveToggle->setMinimumWidth(64);
            back->setMinimumWidth(64);
            topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(saveToggle); topBar->addWidget(back); v->addLayout(topBar);
            // 如果此前 showAccountView 放入了占位标签，这里优先移除占位
            if (auto vlyt = qobject_cast<QVBoxLayout*>(page->layout())) {
                for (int i = vlyt->count()-1; i>=0; --i) {
                    if (auto w = vlyt->itemAt(i)->widget()) {
                        if (qobject_cast<QLabel*>(w) && w->objectName().isEmpty()) { w->deleteLater(); vlyt->removeWidget(w); }
                    }
                }
            }
            auto *form = new QFormLayout();
            auto *idEdit = new QLineEdit(page); idEdit->setObjectName("acc_id"); idEdit->setReadOnly(true);
            auto *user = new QLineEdit(page); user->setObjectName("acc_user");
            auto *phone = new QLineEdit(page); phone->setObjectName("acc_phone");
            auto *email = new QLineEdit(page); email->setObjectName("acc_email");
            auto *pwd = new QLineEdit(page); pwd->setObjectName("acc_pwd"); pwd->setEchoMode(QLineEdit::Password);
            form->addRow(tr("🆔 账号ID"), idEdit);
            form->addRow(tr("👤 用户名"), user);
            form->addRow(tr("📱 手机号"), phone);
            form->addRow(tr("✉️ 邮箱"), email);
            form->addRow(tr("🔒 新密码"), pwd);
            v->addLayout(form);
            // 默认禁用编辑，点击“修改”后启用并把按钮文案变为“保存”
            user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
            if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(page);
            // 行为
            connect(back, &QPushButton::clicked, this, [this, p=QPointer<QWidget>(page)]{
                if (p) { p->hide(); }
                if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView();
            });
            auto sendSave = [this, user, phone, email, pwd]{
                QJsonObject r; r["type"] = "update_account_info";
                const QString newName = user->text().trimmed();
                const QString newPhone = phone->text().trimmed();
                const QString newEmail = email->text().trimmed();
                const QString newPwd = pwd->text();
                if (!newName.isEmpty()) r["new_username"] = newName;
                if (!newPhone.isEmpty()) r["phone"] = newPhone;
                if (!newEmail.isEmpty()) r["email"] = newEmail;
                if (!newPwd.isEmpty()) r["password"] = newPwd;
                if (!currentUsername.isEmpty()) r["username"] = currentUsername;
                QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); enqueueFrame(p);
                statusBar()->showMessage(tr("正在保存修改…"), 2000);
            };
            // 单按钮切换：修改 <-> 保存
            connect(saveToggle, &QPushButton::clicked, this, [saveToggle, user, phone, email, pwd, sendSave]() mutable {
                if (saveToggle->text() == QObject::tr("修改")) {
                    user->setEnabled(true); phone->setEnabled(true); email->setEnabled(true); pwd->setEnabled(true);
                    user->setFocus();
                    saveToggle->setText(QObject::tr("保存"));
                } else {
                    sendSave();
                    user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
                    saveToggle->setText(QObject::tr("修改"));
                }
            });
        }
        // 如果页面已存在但尚未搭建表单（只有占位），则在此构建表单
        if (page && !page->findChild<QLineEdit*>("acc_user")) {
            if (auto v = qobject_cast<QVBoxLayout*>(page->layout())) {
                // 移除占位
                for (int i = v->count()-1; i>=0; --i) {
                    if (auto w = v->itemAt(i)->widget()) {
                        if (qobject_cast<QLabel*>(w) && w->objectName().isEmpty()) { w->deleteLater(); v->removeWidget(w); }
                    }
                }
                auto *form = new QFormLayout();
                auto *idEdit = new QLineEdit(page); idEdit->setObjectName("acc_id"); idEdit->setReadOnly(true);
                auto *user = new QLineEdit(page); user->setObjectName("acc_user");
                auto *phone = new QLineEdit(page); phone->setObjectName("acc_phone");
                auto *email = new QLineEdit(page); email->setObjectName("acc_email");
                auto *pwd = new QLineEdit(page); pwd->setObjectName("acc_pwd"); pwd->setEchoMode(QLineEdit::Password);
                form->addRow(tr("🆔 账号ID"), idEdit);
                form->addRow(tr("👤 用户名"), user);
                form->addRow(tr("📱 手机号"), phone);
                form->addRow(tr("✉️ 邮箱"), email);
                form->addRow(tr("🔒 新密码"), pwd);
                v->addLayout(form);
                // 默认禁用编辑
                user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
                if (auto saveToggle = page->findChild<QPushButton*>("saveTopButton")) {
                    QObject::disconnect(saveToggle, nullptr, nullptr, nullptr);
                    connect(saveToggle, &QPushButton::clicked, this, [saveToggle, user, phone, email, pwd, this]() mutable {
                        auto sendSave = [this, user, phone, email, pwd]{
                            QJsonObject r; r["type"] = "update_account_info";
                            const QString newName = user->text().trimmed();
                            const QString newPhone = phone->text().trimmed();
                            const QString newEmail = email->text().trimmed();
                            const QString newPwd = pwd->text();
                            if (!newName.isEmpty()) r["new_username"] = newName;
                            if (!newPhone.isEmpty()) r["phone"] = newPhone;
                            if (!newEmail.isEmpty()) r["email"] = newEmail;
                            if (!newPwd.isEmpty()) r["password"] = newPwd;
                            if (!currentUsername.isEmpty()) r["username"] = currentUsername;
                            QJsonDocument d(r); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); enqueueFrame(p);
                            statusBar()->showMessage(tr("正在保存修改…"), 2000);
                        };
                        if (saveToggle->text() == QObject::tr("修改")) {
                            user->setEnabled(true); phone->setEnabled(true); email->setEnabled(true); pwd->setEnabled(true);
                            user->setFocus();
                            saveToggle->setText(QObject::tr("保存"));
                        } else {
                            sendSave();
                            user->setEnabled(false); phone->setEnabled(false); email->setEnabled(false); pwd->setEnabled(false);
                            saveToggle->setText(QObject::tr("修改"));
                        }
                    });
                }
            }
        }
        // 填充数据
        if (auto loading = page->findChild<QLabel*>("acc_loading", Qt::FindDirectChildrenOnly)) loading->hide();
        if (auto idEdit = page->findChild<QLineEdit*>("acc_id"))
            idEdit->setText(QString::number(response.value("clientId").toVariant().toLongLong()));
        if (auto user = page->findChild<QLineEdit*>("acc_user"))
            user->setText(response.value("username").toString());
        if (auto phone = page->findChild<QLineEdit*>("acc_phone"))
            phone->setText(response.value("phone").toString());
        if (auto email = page->findChild<QLineEdit*>("acc_email"))
            email->setText(response.value("email").toString());
        if (auto pwd = page->findChild<QLineEdit*>("acc_pwd"))
            pwd->clear();
        clearToFullPage(page);
    }
    else if (type == "update_account_response") {
        if (activeTabKey != QLatin1String("account")) {
            qInfo() << "skip update_account_response due to inactive tab" << activeTabKey;
            return;
        }
        bool ok = response.value("success").toBool(); QString msg = response.value("message").toString();
        statusBar()->showMessage(ok ? (msg.isEmpty()? tr("保存成功"): msg) : (msg.isEmpty()? tr("保存失败"): msg), 3000);
        if (ok) {
            QMessageBox::information(this, tr("修改成功"), msg.isEmpty() ? tr("账户信息已更新") : msg);
        }
        if (ok) {
            // 若用户名被修改，更新本地状态与问候语
            if (auto page = findChild<QWidget*>("accountPage")) {
                if (auto user = page->findChild<QLineEdit*>("acc_user")) {
                    const QString newName = user->text().trimmed();
                    if (!newName.isEmpty() && newName != currentUsername) { currentUsername = newName; updateGreeting(); }
                }
            }
            // 成功后刷新一次账号信息，确保 UI 与服务端一致
            if (socket) {
                QJsonObject req; req["type"] = "get_account_info";
                QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); enqueueFrame(payload);
            }
        }
    }
    else if (type == "orders_response") {
        // 若来源于聊天（origin=chat/detail），或聊天窗口正在等待订单详情（比如从卡片双击发起），则交由 ChatWindow 处理
        const QString origin = response.value("origin").toString();
        if (chat) {
            bool shouldRouteToChat = (origin == QLatin1String("chat") || origin == QLatin1String("detail"));
            // 服务器可能不回传 origin，这里做兜底：如果聊天窗口正在等待详情，也转发给它
            if (!shouldRouteToChat) {
                // ChatWindow 暴露的查询方法
                if (chat->isWaitingOrderDetail()) shouldRouteToChat = true;
            }
            if (shouldRouteToChat) { chat->handleMessage(response); return; }
        }
        // 非聊天路由，仅当“历史订单”页处于激活时才构建页面，避免切换时闪退
        if (activeTabKey != QLatin1String("orders")) {
            qInfo() << "skip orders_response due to inactive tab" << activeTabKey;
            continue;
        }
        // 强防御：始终重建内嵌订单页面，彻底避免复用导致的生命周期重叠
        if (auto exist = findChild<QWidget*>("ordersPage")) {
            QObject::disconnect(exist, nullptr, nullptr, nullptr);
            exist->blockSignals(true);
            exist->deleteLater();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    QWidget *container = new QWidget(this);
    container->setObjectName("ordersPage");
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(container);
    auto *v = new QVBoxLayout(container);

        // 统一控件样式（与侧栏主色一致）
        container->setStyleSheet(
            "QPushButton { background:#ffffff; border:1px solid #ddd; border-radius:6px; padding:6px 10px; }"
            "QPushButton:hover { background:#fafafa; }"
            "QPushButton:pressed { background:#f0f0f0; }"
            /* 筛选按钮：白字蓝底 */
            "QPushButton#ordersFilterBtn { background:#1677ff; border:1px solid #1677ff; color:#ffffff; }"
            "QPushButton#ordersFilterBtn:hover { background:#3a8cff; border-color:#3a8cff; }"
            "QPushButton#ordersFilterBtn:pressed { background:#2a6bdb; }"
            /* 分页器按钮：白字蓝底，与筛选按钮一致 */
            "QPushButton#ordersPrev, QPushButton#ordersNext { background:#1677ff; border:1px solid #1677ff; color:#ffffff; }"
            "QPushButton#ordersPrev:hover, QPushButton#ordersNext:hover { background:#3a8cff; border-color:#3a8cff; }"
            "QPushButton#ordersPrev:pressed, QPushButton#ordersNext:pressed { background:#2a6bdb; }"
            "QLineEdit, QComboBox { border:1px solid #ddd; border-radius:6px; padding:4px 6px; }"
        );

        // 顶部栏
    auto *topBar = new QHBoxLayout();
        auto *title = new QLabel(tr("历史订单"), container); title->setStyleSheet("font-weight:600;font-size:16px;");
    auto *back = new QPushButton(tr("返回"), container); back->setMinimumWidth(64); back->setStyleSheet("QPushButton{color:#333;}");
        topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(back); v->addLayout(topBar);

        // 筛选栏
        auto *filterBar = new QHBoxLayout();
    auto *statusLbl = new QLabel(tr("状态"), container);
    auto *statusCmb = new QComboBox(container); statusCmb->setObjectName("ordersStatus");
    statusCmb->addItems({ tr("全部"), tr("已支付"), tr("待支付"), tr("已取消") });
        auto *kwLbl = new QLabel(tr("关键字"), container);
        auto *kwEdit = new QLineEdit(container); kwEdit->setObjectName("ordersKeyword"); kwEdit->setPlaceholderText(tr("订单号/用户名"));
    auto *filterBtn = new QPushButton(tr("筛选"), container); filterBtn->setObjectName("ordersFilterBtn");
        filterBar->addWidget(statusLbl); filterBar->addWidget(statusCmb);
        filterBar->addSpacing(8);
        filterBar->addWidget(kwLbl); filterBar->addWidget(kwEdit);
        filterBar->addSpacing(8);
        filterBar->addStretch(1);
        filterBar->addWidget(filterBtn);
        v->addLayout(filterBar);

        // 表格
    auto *table = new QTableWidget(container); table->setObjectName("ordersTable"); table->setColumnCount(5);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::StrongFocus);
    // 放大行高和列宽策略
    table->verticalHeader()->setDefaultSectionSize(40); // 每行更高一些
    table->setAlternatingRowColors(true);
    table->setStyleSheet("QTableWidget{alternate-background-color:#fafafa;} ");
        QStringList headers; headers << tr("订单号") << tr("用户名") << tr("金额") << tr("状态") << tr("时间");
        table->setHorizontalHeaderLabels(headers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->horizontalHeader()->setMinimumSectionSize(80);
        // 给金额列一个稍大的最小宽度，便于阅读
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table->setColumnWidth(2, 120);
        v->addWidget(table);

        // 分页栏
        auto *pager = new QHBoxLayout();
        auto *pageInfo = new QLabel(container); pageInfo->setObjectName("ordersPageInfo");
    auto *prevBtn = new QPushButton(tr("上一页"), container); prevBtn->setObjectName("ordersPrev");
    auto *nextBtn = new QPushButton(tr("下一页"), container); nextBtn->setObjectName("ordersNext");
    // 分页条左对齐：信息 + 上一页/下一页，右侧留空
    pager->addWidget(pageInfo);
    pager->addSpacing(12);
    pager->addWidget(prevBtn);
    pager->addWidget(nextBtn);
    pager->addStretch(1);
        v->addLayout(pager);

        // 数据与刷新函数
    QJsonArray arr = response.value("orders").toArray();
    container->setProperty("ordersData", arr.toVariantList());
        container->setProperty("ordersPageNo", 1);
        container->setProperty("ordersPageSize", 10);

        // use QPointer to avoid dangling pointer if widget is deleted asynchronously
        QPointer<QWidget> containerPtr(container);
        auto refresh = [this, containerPtr]() {
            if (!containerPtr) return;
            auto *table = containerPtr->findChild<QTableWidget*>("ordersTable");
            auto *statusCmb = containerPtr->findChild<QComboBox*>("ordersStatus");
            auto *kwEdit = containerPtr->findChild<QLineEdit*>("ordersKeyword");
            auto *pageInfo = containerPtr->findChild<QLabel*>("ordersPageInfo");
            auto *prevBtn = containerPtr->findChild<QPushButton*>("ordersPrev");
            auto *nextBtn = containerPtr->findChild<QPushButton*>("ordersNext");
            if (!table || !statusCmb || !kwEdit || !pageInfo || !prevBtn || !nextBtn) return;

            const QVariantList data = containerPtr->property("ordersData").toList();
            const int pageSize = containerPtr->property("ordersPageSize").toInt();
            int pageNo = containerPtr->property("ordersPageNo").toInt(); if (pageNo < 1) pageNo = 1;
            const QString statusSel = statusCmb->currentText();
            const QString kw = kwEdit->text().trimmed();

            // 过滤
            QVariantList filtered;
            filtered.reserve(data.size());
            for (const QVariant &v : data) {
                const QVariantMap m = v.toMap();
                QString status = m.value("status").toString();
                // 兼容后端或数据里出现的英文枚举，统一映射到中文以用于比较
                if (status.compare("PAID", Qt::CaseInsensitive) == 0) status = tr("已支付");
                else if (status.compare("CART", Qt::CaseInsensitive) == 0 || status.compare("PENDING", Qt::CaseInsensitive) == 0) status = tr("待支付");
                else if (status.compare("REFUNDED", Qt::CaseInsensitive) == 0 || status.compare("CANCELLED", Qt::CaseInsensitive) == 0) status = tr("已取消");
                const QString uname = m.value("username").toString();
                const QString idStr = QString::number(m.value("orderId").toLongLong());
                if (statusSel != tr("全部") && status != statusSel) continue;
                if (!kw.isEmpty() && !(idStr.contains(kw, Qt::CaseInsensitive) || uname.contains(kw, Qt::CaseInsensitive))) continue;
                filtered.push_back(v);
            }

            const int total = filtered.size();
            const int totalPages = qMax(1, (total + pageSize - 1) / pageSize);
            if (pageNo > totalPages) pageNo = totalPages;
            containerPtr->setProperty("ordersPageNo", pageNo);

            // 填充当前页
            const int start = (pageNo - 1) * pageSize;
            const int end = qMin(start + pageSize, total);
            table->setRowCount(qMax(0, end - start));
            int r = 0;
            for (int i = start; i < end; ++i) {
                const QVariantMap o = filtered.at(i).toMap();
                auto *idItem = new QTableWidgetItem(QString::number(o.value("orderId").toLongLong()));
                // 绑定整单数据供双击时查看详情
                idItem->setData(Qt::UserRole, o);
                table->setItem(r, 0, idItem);
                QString uname = o.value("username").toString(); if (uname.isEmpty()) uname = currentUsername;
                table->setItem(r, 1, new QTableWidgetItem(uname));
                // 计算订单维度的单品折扣与满减（每满200减20）并展示
                auto items = o.value("items").toList();
                double sumOriginal = 0.0;   // 原价合计
                double sumEffective = 0.0;  // 单品折扣后合计
                for (const QVariant &iv : items) {
                    const QVariantMap it = iv.toMap();
                    const int qty = it.value("quantity").toInt();
                    const double price = it.value("price").toDouble();
                    const double listPrice = it.contains("listPrice") ? it.value("listPrice").toDouble() : price;
                    const bool onSale = it.value("onSale").toBool();
                    const double dprice = it.contains("discountPrice") ? it.value("discountPrice").toDouble() : 0.0;
                    const bool hasDiscount = onSale && dprice>0.0 && dprice<listPrice;
                    const double unit = hasDiscount ? dprice : price;
                    sumOriginal += listPrice * qty;
                    sumEffective += unit * qty;
                }
                // 若后端列表未返回明细，则用 total_price 作为基准计算满减展示
                if (items.isEmpty()) {
                    sumOriginal = o.value("total_price").toDouble();
                    sumEffective = sumOriginal;
                }
                // 满减：每满200减20（基于单品折后合计）
                const qint64 cents = static_cast<qint64>(qRound64(sumEffective * 100.0));
                const qint64 threshold = 20000; // 200元
                const qint64 stepOff  = 2000;   // 20元
                const qint64 times    = (cents>0 ? (cents/threshold) : 0);
                const qint64 promoOff = times * stepOff;
                const qint64 finalPay = qMax<qint64>(0, cents - promoOff);
                const bool showStrike = finalPay < static_cast<qint64>(qRound64(sumOriginal * 100.0));
                // 列表仅显示“实付金额”（黑色），满减拆解放在详情弹窗
                table->setItem(r, 2, new QTableWidgetItem(QString::number(finalPay/100.0, 'f', 2)));
                table->setItem(r, 3, new QTableWidgetItem(o.value("status").toString()));
                // 标准化时间显示：YYYY-MM-DD HH:mm:ss
                auto fmtTime = [](const QVariant &v)->QString{
                    // 支持字符串(ISO或常见格式)与毫秒时间戳
                    if (v.typeId() == QMetaType::LongLong || v.typeId() == QMetaType::Int || v.typeId() == QMetaType::Double) {
                        const qint64 ms = v.toLongLong();
                        return QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss");
                    }
                    const QString s = v.toString().trimmed();
                    if (s.isEmpty()) return QString();
                    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
                    if (!dt.isValid()) dt = QDateTime::fromString(s, Qt::ISODate);
                    if (!dt.isValid()) dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
                    if (!dt.isValid()) dt = QDateTime::fromString(s, "yyyy/MM/dd HH:mm:ss");
                    if (!dt.isValid()) {
                        // 尝试解析形如 2025-09-26T19:14:30.917175（微秒）
                        QString t = s;
                        t.replace('T', ' ');
                        int dot = t.indexOf('.');
                        if (dot>0) t = t.left(dot); // 去掉小数部分
                        dt = QDateTime::fromString(t, "yyyy-MM-dd HH:mm:ss");
                    }
                    if (!dt.isValid()) return s; // 保底返回原文
                    return dt.toString("yyyy-MM-dd HH:mm:ss");
                };
                table->setItem(r, 4, new QTableWidgetItem(fmtTime(o.value("order_time"))));
                // 单独提升该行的高度（在默认40基础上再稍微增加一点可读性）
                table->setRowHeight(r, 42);
                ++r;
            }
            pageInfo->setText(tr("第 %1 / %2 页 · 共 %3 条").arg(pageNo).arg(totalPages).arg(total));
            prevBtn->setEnabled(pageNo > 1);
            nextBtn->setEnabled(pageNo < totalPages);
        };

        // 交互
    // 为避免重复连接导致的重复回调/野指针，先断开旧的（如果有），再建立一次新的连接
                    // 改为滚动加载，隐藏底部分页器
                    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
                    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
                    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
                    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
                    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
                    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
                    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    QObject::disconnect(kwEdit, nullptr, nullptr, nullptr);
    QObject::disconnect(statusCmb, nullptr, nullptr, nullptr);
    QObject::disconnect(prevBtn, nullptr, nullptr, nullptr);
    QObject::disconnect(nextBtn, nullptr, nullptr, nullptr);
    // 将 context 设为 container，页面销毁时自动解绑；不使用 UniqueConnection，避免 lambda 唯一性判定问题
    connect(filterBtn, &QPushButton::clicked, container, [containerPtr, refresh]{ if (containerPtr) { containerPtr->setProperty("ordersPageNo", 1); refresh(); } });
    connect(kwEdit, &QLineEdit::returnPressed, container, [containerPtr, refresh]{ if (containerPtr) { containerPtr->setProperty("ordersPageNo", 1); refresh(); } });
    connect(statusCmb, qOverload<int>(&QComboBox::currentIndexChanged), container, [containerPtr, refresh](int){ if (containerPtr) { containerPtr->setProperty("ordersPageNo", 1); refresh(); } });
    connect(prevBtn, &QPushButton::clicked, container, [containerPtr, refresh]{ if (!containerPtr) return; int p = containerPtr->property("ordersPageNo").toInt(); if (p>1) { containerPtr->setProperty("ordersPageNo", p-1); refresh(); } });
    connect(nextBtn, &QPushButton::clicked, container, [containerPtr, refresh]{ if (!containerPtr) return; int p = containerPtr->property("ordersPageNo").toInt(); containerPtr->setProperty("ordersPageNo", p+1); refresh(); });

        // 双击表格行查看订单详情（小票样式）
        if (auto *tbl = container->findChild<QTableWidget*>("ordersTable")) {
            QPointer<QTableWidget> tblPtr(tbl);
            // 仅断开与当前页面相关的旧连接，避免误伤其他对象的信号
            QObject::disconnect(tbl, nullptr, container, nullptr);
            auto showOrderDetail = [this, tblPtr](int row){
                if (!tblPtr) return;
                if (row < 0) return;
                auto *idItem = tblPtr->item(row, 0);
                if (!idItem) { QMessageBox::warning(this, tr("提示"), tr("未获取到订单数据")); return; }
                const QVariantMap order = idItem->data(Qt::UserRole).toMap();
                if (order.isEmpty()) { QMessageBox::warning(this, tr("提示"), tr("未获取到订单数据")); return; }
                const qlonglong orderId = order.value("orderId").toLongLong();
                const QString rawStatus = order.value("status").toString();
                // 归一化状态并添加 emoji 徽标
                QString status = rawStatus;
                if (status.compare("PAID", Qt::CaseInsensitive) == 0) status = tr("已支付");
                else if (status.compare("CART", Qt::CaseInsensitive) == 0 || status.compare("PENDING", Qt::CaseInsensitive) == 0) status = tr("待支付");
                else if (status.compare("REFUNDED", Qt::CaseInsensitive) == 0 || status.compare("CANCELLED", Qt::CaseInsensitive) == 0) status = tr("已取消");
                QString statusEmoji, statusColor;
                if (status == tr("已支付")) { statusEmoji = QString::fromUtf8("✅ "); statusColor = "#2e7d32"; }
                else if (status == tr("待支付")) { statusEmoji = QString::fromUtf8("⏳ "); statusColor = "#f57c00"; }
                else if (status == tr("已取消")) { statusEmoji = QString::fromUtf8("❌ "); statusColor = "#c62828"; }
                else { statusEmoji = QString::fromUtf8("ℹ️ "); statusColor = "#455a64"; }
                QString timeStr = order.value("order_time").toString();
                // 友好化时间显示：将 ISO 8601 中的 'T' 替换为空格
                if (!timeStr.isEmpty()) timeStr.replace('T', ' ');
                const QVariantList items = order.value("items").toList();
                // 生成小票 HTML
                QString html;
                html += QString("<div style='font-weight:700;font-size:14px;margin-bottom:6px;'>🧾 ")
                        + tr("订单") + QString(" #%1</div>").arg(orderId);
                if (!timeStr.isEmpty()) html += QString("<div style='color:#666;'>🕒 ") + tr("时间：") + timeStr + "</div>";
                if (!status.isEmpty()) html += QString("<div style='color:%1;margin-bottom:6px;'>%2")
                                                .arg(statusColor, statusEmoji)
                                            + tr("状态：") + status + "</div>";
                html += QString("<table style='width:100%;border-collapse:collapse;%1%2%3'>")
                            .arg("font-family:'Microsoft YaHei','Segoe UI','PingFang SC','Helvetica Neue',Arial,sans-serif;")
                            .arg("font-size:13px;")
                            .arg("");
                html += "<tr><th style='text-align:left;border-bottom:1px solid #eee;padding:4px 0;'>🛍️ " + tr("商品") + "</th>"
                        "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>✖️ " + tr("数量") + "</th>"
                        "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>💵 " + tr("单价") + "</th>"
                        "<th style='text-align:right;border-bottom:1px solid #eee;padding:4px 0;'>💴 " + tr("小计") + "</th></tr>";
                double sumOriginal = 0.0;
                double sumEffective = 0.0;
                for (const QVariant &iv : items) {
                    const QVariantMap it = iv.toMap();
                    QString name = it.value("name").toString();
                    if (it.contains("size") && it.value("size").toInt()>0) {
                        name += QString("  (") + tr("尺码:") + QString::number(it.value("size").toInt()) + ")";
                    }
                    const int qty = it.value("quantity").toInt();
            const double price = it.value("price").toDouble();
            const double listPrice = it.contains("listPrice") ? it.value("listPrice").toDouble() : price;
            const bool onSale = it.value("onSale").toBool();
            const double dprice = it.contains("discountPrice") ? it.value("discountPrice").toDouble() : 0.0;
            const bool hasDiscount = onSale && dprice>0.0 && dprice<listPrice;
            const double unit = hasDiscount ? dprice : price;
            const double sub = unit * qty;
            sumOriginal += listPrice * qty;
            sumEffective += sub;
                    html += QString("<tr><td style='padding:4px 0;'>%1</td>"
                                    "<td style='text-align:right;padding:4px 0;'>%2</td>"
                    "<td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%3</td>"
                                    "<td style='text-align:right;padding:4px 0;'>&yen;&nbsp;%4</td></tr>")
                                .arg(name.toHtmlEscaped())
                                .arg(qty)
                .arg(QString::number(unit, 'f', 2))
                .arg(QString::number(sub, 'f', 2));
                }
                // 若无明细，使用 total_price 作为基准（仍按活动规则展示合计信息）
                if (items.isEmpty()) {
                    sumOriginal = order.value("total_price").toDouble();
                    sumEffective = sumOriginal;
                }
        // 订单级满减（每满200减20），基于单品折后合计
        const qint64 cents = static_cast<qint64>(qRound64(sumEffective * 100.0));
        const qint64 threshold = 20000; // 200元
        const qint64 stepOff  = 2000;   // 20元
        const qint64 times    = (cents>0 ? (cents/threshold) : 0);
        const qint64 promoOff = times * stepOff;
        const qint64 finalPay = qMax<qint64>(0, cents - promoOff);
        // 合计区：原价（划线）/ 折后（可选）/ 满减 / 应付
        html += QString("<tr><td colspan='4' style='border-top:1px solid #eee;padding-top:6px;text-align:right;'>");
        if (sumOriginal > sumEffective + 1e-6) {
            html += QString("<div style='color:#999;text-decoration:line-through;'>✨ ") + tr("原价合计：")
                + QString("&yen;&nbsp;%1</div>").arg(QString::number(sumOriginal, 'f', 2));
        }
        if (promoOff > 0) {
            html += QString("<div>🏷️ ") + tr("商品折后：") + QString("&yen;&nbsp;%1</div>")
                .arg(QString::number(sumEffective, 'f', 2));
            html += QString("<div style='color:#43A047;'>🔻 ") + tr("满减：") + "-&yen;&nbsp;"
                + QString::number(promoOff/100.0, 'f', 2) + "</div>";
        }
        html += QString("<div style='font-weight:700;color:#E53935;'>💰 ") + tr("应付：")
            + QString("&yen;&nbsp;%1</div>").arg(QString::number(finalPay/100.0, 'f', 2));
        html += "</td></tr>";
                html += "</table>";

                QMessageBox box(this);
                box.setWindowTitle(tr("订单详情 🧾"));
                box.setTextFormat(Qt::RichText);
                box.setText(html);
                box.addButton(tr("关闭"), QMessageBox::RejectRole);
                box.exec();
            };
            // 仅保留一种双击信号，避免多路触发导致重复弹窗
            connect(tbl, &QTableWidget::itemDoubleClicked, container, [showOrderDetail](QTableWidgetItem *item){ showOrderDetail(item ? item->row() : -1); });
        }

        // 返回：只隐藏容器，避免异步回调期间删除导致崩溃
        QObject::disconnect(back, nullptr, nullptr, nullptr);
        QPointer<QWidget> backContainer(container);
        connect(back, &QPushButton::clicked, this, [this, backContainer]{
            if (backContainer) backContainer->hide();
            if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView();
        }, Qt::UniqueConnection);

        // 首次渲染
        refresh();
        clearToFullPage(container);
    }
    
    else if (type == "error") {
        const int code = response.value("code").toInt();
        const QString msg = response.value("message").toString();
        QString friendly;
        switch (code) {
            case 1001: friendly = tr("认证失败"); break;
            case 1002: friendly = tr("参数无效"); break;
            case 2001: friendly = tr("商品不存在"); break;
            case 2002: friendly = tr("库存不足"); break;
            case 3001: friendly = tr("购物车为空"); break;
            case 3002: friendly = tr("订单创建失败"); break;
            default: friendly = tr("发生错误");
        }
        statusBar()->showMessage(friendly + (msg.isEmpty() ? QString() : (" - " + msg)), 4000);
    }
    // 将购物车相关的响应转发到购物车窗口（若已创建）
    if (cart) {
        const QString t = response.value("type").toString();
        if (t == QLatin1String("cart_items")
            || t == QLatin1String("cart_response")
            || t == QLatin1String("checkout_response")
            || t == QLatin1String("order_response")
            || t == QLatin1String("clear_cart_response")
            || t == QLatin1String("error")) {
            cart->handleMessage(response);
        }
    }

    // 将聊天相关消息转发给 ChatWindow（若已创建）
    if (chat) {
        const QString t = response.value("type").toString();
        if (t == QLatin1String("chat_init_response")
            || t == QLatin1String("chat_message")
            || t == QLatin1String("presence")
            || t == QLatin1String("chat_delete_response")) {
            chat->handleMessage(response);
            if (t == QLatin1String("chat_init_response")) {
                if (this->property("awaitingChatInit").toBool()) {
                    this->setProperty("awaitingChatInit", false);
                    qInfo() << "chat_init_response received, resume queued requests";
                }
            }
        }
    }
    }
}

// ---- Helpers ----
QLayout* MainWindow::ensureVBoxLayout(QWidget *area)
{
    if (!area) return nullptr;
    if (!area->layout()) {
        auto *v = new QVBoxLayout(area);
        v->setContentsMargins(8,8,8,8);
        v->setSpacing(6);
        area->setLayout(v);
        return v;
    }
    return area->layout();
}

void MainWindow::clearLayout(QLayout *layout)
{
    if (!layout) return;
    // 对于“历史订单”页面，使用同步删除来避免 deleteLater 异步清理与二次构建重叠导致的不稳定/崩溃
    bool syncDelete = false;
    if (QWidget *pw = layout->parentWidget()) {
        if (pw->objectName() == QLatin1String("ordersPage")) syncDelete = true;
    }
    while (QLayoutItem *it = layout->takeAt(0)) {
        if (auto *childLayout = it->layout()) {
            // 递归清理并销毁子布局，防止遗留的控件/布局导致下次构建时错乱
            clearLayout(childLayout);
            delete childLayout;
        } else if (auto *w = it->widget()) {
            if (syncDelete) {
                // 防止回调访问已销毁对象：先断开所有信号，再同步删除
                QObject::disconnect(w, nullptr, nullptr, nullptr);
                w->blockSignals(true);
                delete w;
            } else {
                // 默认：异步删除更加安全
                w->deleteLater();
            }
        }
        // QSpacerItem 等通过删除 it 即可清理
        delete it;
    }
    // 失效布局缓存，确保后续计算重新进行
    layout->invalidate();
}

void MainWindow::renderCarouselPlaceholder()
{
    auto *layout = ensureVBoxLayout(ui->carouselArea);
    clearLayout(layout);
    auto *lbl = new QLabel(tr("这里显示轮播图（占位）"), ui->carouselArea);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setMinimumHeight(180);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
}

void MainWindow::renderRecommendationsPlaceholder()
{
    auto *layout = ensureVBoxLayout(ui->recommendationsArea);
    clearLayout(layout);
    auto *lbl = new QLabel(tr("为你推荐（占位）"), ui->recommendationsArea);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    for (int i=0;i<4;++i) {
        auto *item = new QPushButton(tr("推荐商品 %1").arg(i+1), ui->recommendationsArea);
        item->setMinimumHeight(32);
        layout->addWidget(item);
    }
}

void MainWindow::renderPromotionsPlaceholder()
{
    auto *layout = ensureVBoxLayout(ui->promotionsArea);
    clearLayout(layout);
    auto *lbl = new QLabel(tr("优惠活动（占位）"), ui->promotionsArea);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    for (int i=0;i<2;++i) {
        auto *promo = new QLabel(tr("限时优惠 %1：全场 9.%1 折").arg(i+1), ui->promotionsArea);
        promo->setWordWrap(true);
        layout->addWidget(promo);
    }
}

void MainWindow::renderCarousel(const QJsonArray &images)
{
    Q_UNUSED(images);
    auto *layout = ensureVBoxLayout(ui->carouselArea);
    // 重建前先停止并断开旧的计时器连接，防止回调引用已销毁的控件导致崩溃
    if (carouselTimer) {
        carouselTimer->stop();
        QObject::disconnect(carouselTimer, nullptr, nullptr, nullptr);
    }
    clearLayout(layout);
    // 读取本地路径（用户提供目录 C:/Users/Edward/Desktop/test/images 下的 home_page 1-4.*）
    {
        const QString baseDir = QStringLiteral("C:/Users/Edward/Desktop/test/images");
        QDir dir(baseDir);
        QStringList fromFolder;
        if (dir.exists()) {
            const QStringList exts = {"png","jpg","jpeg","webp","bmp"};
            for (int i = 1; i <= 4; ++i) {
                bool found = false;
                for (const QString &ext : exts) {
                    const QString fp = QStringLiteral("%1/home_page %2.%3").arg(baseDir).arg(i).arg(ext);
                    if (QFileInfo::exists(fp)) { fromFolder << fp; found = true; break; }
                }
                if (!found) {
                    // 容错：允许没有扩展一致时跳过该序号
                }
            }
        }
        if (!fromFolder.isEmpty()) {
            carouselLocalPaths = fromFolder;
        }
        // 预加载原图（每次刷新目录结果后都重建缓存）
        carouselOriginals.clear();
        for (const QString &p : carouselLocalPaths) carouselOriginals.push_back(QPixmap(p));
    }
    // 三联图容器：prev | main | next
    QWidget *row = new QWidget(ui->carouselArea);
    auto *h = new QHBoxLayout(row); h->setContentsMargins(0,0,0,0); h->setSpacing(4);
    QLabel *prevLbl = new QLabel(row); prevLbl->setAlignment(Qt::AlignCenter); prevLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QLabel *mainLbl = new QLabel(row); mainLbl->setAlignment(Qt::AlignCenter); mainLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QLabel *nextLbl = new QLabel(row); nextLbl->setAlignment(Qt::AlignCenter); nextLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    prevLbl->setObjectName("carouselPrev"); mainLbl->setObjectName("carouselMain"); nextLbl->setObjectName("carouselNext");
    prevLbl->setStyleSheet("border-radius:6px; border:1px solid #ddd;");
    mainLbl->setStyleSheet("border-radius:8px; border:1px solid #ccc;");
    nextLbl->setStyleSheet("border-radius:6px; border:1px solid #ddd;");
    auto *prevEff = new QGraphicsOpacityEffect(prevLbl); prevEff->setOpacity(0.45); prevLbl->setGraphicsEffect(prevEff);
    auto *nextEff = new QGraphicsOpacityEffect(nextLbl); nextEff->setOpacity(0.45); nextLbl->setGraphicsEffect(nextEff);
    // 调整权重：中间主图更大，两侧预览更小
    h->addWidget(prevLbl, 10); h->addWidget(mainLbl, 80); h->addWidget(nextLbl, 10);
    layout->addWidget(row);
    // 保存主/侧标签并允许点击切换
    carouselImageLabel = mainLbl; carouselPrevLabel = prevLbl; carouselNextLabel = nextLbl;
    prevLbl->installEventFilter(this); nextLbl->installEventFilter(this);
    auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
    auto refreshAll = [this, prevLbl, nextLbl, idxWrap](){
        refreshCarouselPixmap();
        if (!carouselOriginals.isEmpty()) {
            const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
            const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
            int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
            int sideW = qMax(110, (contentW - 40) * 10 / 100);
            int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
            const int maxSideH = qMax(110, mainH * 75 / 100);
            prevLbl->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
            nextLbl->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
            prevLbl->setFixedWidth(sideW);
            nextLbl->setFixedWidth(sideW);
            prevLbl->setFixedHeight(prevLbl->pixmap(Qt::ReturnByValue).height());
            nextLbl->setFixedHeight(nextLbl->pixmap(Qt::ReturnByValue).height());
        }
        updateCarouselDots();
    };
    // 初始帧
    carouselIndex = 0;
    refreshAll();
    // 主图淡入动画
    auto playFade = [this](){
        if (!carouselImageLabel) return;
        auto *eff = new QGraphicsOpacityEffect(carouselImageLabel);
        carouselImageLabel->setGraphicsEffect(eff);
        auto *ani = new QPropertyAnimation(eff, "opacity", carouselImageLabel);
        ani->setDuration(400);
        ani->setStartValue(0.0);
        ani->setEndValue(1.0);
        ani->start(QAbstractAnimation::DeleteWhenStopped);
    };
    // 定时自动轮播
    // 统一（重新）配置计时器，并把回调上下文绑定到本次渲染生成的 row，
    // 这样 row 被销毁时该连接会自动断开，避免悬空指针
    if (!carouselTimer) {
        carouselTimer = new QTimer(this);
    }
    carouselTimer->setInterval(2500);
    connect(carouselTimer, &QTimer::timeout, row, [this, refreshAll, playFade]{
        if (carouselOriginals.isEmpty()) return;
        carouselIndex = (carouselIndex + 1) % carouselOriginals.size();
        refreshAll();
        playFade();
    });
    carouselTimer->start();
    // 底部小圆点
    {
    auto *dotRow = new QHBoxLayout(); dotRow->setContentsMargins(0,4,0,0);
        dotRow->setSpacing(6); dotRow->addStretch(1);
        carouselDots.clear();
        for (int i=0;i<carouselOriginals.size();++i) {
            auto *d = new QLabel(ui->carouselArea);
            d->setFixedSize(10,10);
            d->setStyleSheet("border-radius:5px;background:#ddd;");
            d->setProperty("dotIndex", i);
            d->installEventFilter(this);
            carouselDots.push_back(d);
            dotRow->addWidget(d);
        }
        dotRow->addStretch(1);
        auto *host = new QWidget(ui->carouselArea); host->setLayout(dotRow);
        layout->addWidget(host);
        updateCarouselDots();
    }
}

void MainWindow::renderRecommendations(const QJsonArray &products)
{
    // 保存最近数据并标记当前区域
    lastRecommendations = products;
    showingList = false;
    auto *container = ui->recommendationsArea;
    auto *layout = ensureVBoxLayout(container);
    clearLayout(layout);
    // 标题行
    {
        auto *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0,0,0,0);
    auto *title = new QLabel(tr("首页精选"), container);
        title->setStyleSheet("font-weight:600;margin:6px 0;");
        titleRow->addWidget(title);
        titleRow->addStretch(1);
        auto *moreBtn = new QPushButton(tr("更多 >"), container);
        moreBtn->setObjectName("homeMoreDiscountBtn");
        moreBtn->setStyleSheet(
            "QPushButton{color:#ffffff;background:#1677ff;border:1px solid #1677ff;border-radius:6px;padding:2px 10px;}"
            "QPushButton:hover{background:#3a8cff;border-color:#3a8cff;}"
        );
        titleRow->addWidget(moreBtn);
        auto *host = new QWidget(container); host->setLayout(titleRow); layout->addWidget(host);
        connect(moreBtn, &QPushButton::clicked, this, [this]{ showMallView(); });
    }
    if (products.isEmpty()) { renderRecommendationsPlaceholder(); return; }
    // 选择规则：1 个新品 + 3 个打折；不足则用其它补齐，合计最多 4 个
    QList<QJsonObject> items; items.reserve(products.size());
    for (const auto &v : products) items.push_back(v.toObject());
    auto isDiscounted = [](const QJsonObject &o){
        const double price = o.value("price").toDouble();
        const double discount = o.value("discountPrice").toDouble(0.0);
        const bool onSale = o.value("onSale").toBool();
        return onSale && discount>0.0 && discount<price;
    };
    QJsonArray picked;
    int newIdx = -1;
    for (int i=0;i<items.size();++i) if (items[i].value("isNew").toBool()) { picked.append(items[i]); newIdx = i; break; }
    for (int i=0;i<items.size() && picked.size()<4; ++i) {
        if (i==newIdx) continue;
        if (isDiscounted(items[i])) picked.append(items[i]);
    }
    for (int i=0;i<items.size() && picked.size()<4; ++i) {
        if (i==newIdx) continue;
        const int pid = items[i].value("product_id").toInt();
        bool exists=false; for (const auto &pv : picked) { if (pv.toObject().value("product_id").toInt()==pid) { exists=true; break; } }
        if (!exists) picked.append(items[i]);
    }
    // 渲染 2x2 网格
    auto *grid = new QGridLayout();
    grid->setContentsMargins(4,4,4,4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    const int count = qMin(4, picked.size());
    for (int i=0;i<count; ++i) {
        const QJsonObject o = picked.at(i).toObject();
        const int pid = o.value("product_id").toInt();
        const QString name = o.value("name").toString(QLatin1String("商品"));
        const double price = o.value("price").toDouble();
        const int stock = o.value("stock").toInt(-1);
    // 缓存商品（含 sizes 信息）以便直接“加入购物车”时可获取尺码库存（使用全局指针）
    if (!productCachePtr) productCachePtr = new QHash<int,QJsonObject>();
    (*productCachePtr)[pid] = o;
    auto *card = new QFrame(container);
        card->setFrameShape(QFrame::StyledPanel);
    card->setObjectName(QLatin1String("mallCard"));
    card->setStyleSheet("");
        auto *vbox = new QVBoxLayout(card); vbox->setContentsMargins(8,8,8,8); vbox->setSpacing(4);
        // 角标 - 三级优先级: 售罄 > 热卖🔥 > 新品
        bool soldOut = (stock==0);
        bool isHot = o.value("isHot").toBool();
        bool isNew = o.value("isNew").toBool();
        if (soldOut || isHot || isNew) {
            auto *row = new QHBoxLayout(); row->setContentsMargins(0,0,0,0); row->addStretch(1);
            QString badgeText, badgeStyle;
            if (soldOut) {
                badgeText = tr("售罄");
                badgeStyle = "QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}";
            } else if (isHot) {
                badgeText = tr("热卖🔥");
                badgeStyle = "QLabel{background:#FF6F00;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}";
            } else {
                badgeText = tr("新品");
                badgeStyle = "QLabel{background:#34A853;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}";
            }
            auto *badge = new QLabel(badgeText, card);
            badge->setStyleSheet(badgeStyle);
            row->addWidget(badge, 0, Qt::AlignRight); vbox->addLayout(row);
        }
        // 图片
    auto *img = new QLabel(card); img->setAlignment(Qt::AlignCenter); img->setMinimumHeight(100);
    img->setObjectName(QLatin1String("mallImageHolder"));
    vbox->addWidget(img);
    // 兼容后端字段名 image_url / imageUrl
    QString imgUrl = o.value("image_url").toString();
    if (imgUrl.isEmpty()) imgUrl = o.value("imageUrl").toString();
    if (!imgUrl.isEmpty()) setImageFromUrl(imgUrl, img, QSize(160, 100));
        // 名称
        auto *nameLbl = new QLabel(name, card); nameLbl->setStyleSheet("font-weight:600;"); nameLbl->setWordWrap(true);
        vbox->addWidget(nameLbl);
        // 价格
        QLabel *priceLbl=nullptr, *discountLbl=nullptr;
        const bool onSale = o.value("onSale").toBool(); const double discount = o.value("discountPrice").toDouble(0.0);
        const bool hasDiscount = onSale && discount>0.0 && discount<price;
        if (hasDiscount) {
            priceLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
            priceLbl->setStyleSheet("color:#999;text-decoration:line-through;");
            discountLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(discount, 'f', 2)), card);
            discountLbl->setStyleSheet("color:#E53935;font-weight:700;");
        } else {
            priceLbl = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
        }
        vbox->addWidget(priceLbl); if (discountLbl) vbox->addWidget(discountLbl);
        // 按钮
        auto *btnRow = new QHBoxLayout(); auto *detailBtn = new QPushButton(tr("详情"), card); auto *addBtn = new QPushButton(tr("加入购物车"), card);
        detailBtn->setMinimumSize(64,28); addBtn->setMinimumSize(86,28); if (stock==0) addBtn->setEnabled(false);
        btnRow->addWidget(detailBtn); btnRow->addWidget(addBtn); btnRow->addStretch(1); vbox->addLayout(btnRow);
        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
        connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock, -1); });
        // 放入 2 列网格
        grid->addWidget(card, i/2, i%2);
    }
    auto *gridHost = new QWidget(container); gridHost->setLayout(grid); layout->addWidget(gridHost);
    lastColumns = 2;
}

void MainWindow::renderPromotions(const QJsonArray &promotions)
{
    auto *layout = ensureVBoxLayout(ui->promotionsArea);
    clearLayout(layout);
    if (promotions.isEmpty()) {
        renderPromotionsPlaceholder();
        return;
    }
    for (auto v : promotions) {
        const auto o = v.toObject();
        auto *lbl = new QLabel(o.value("title").toString(QLatin1String("活动")) + ": " + o.value("desc").toString(), ui->promotionsArea);
        lbl->setWordWrap(true);
        layout->addWidget(lbl);
    }
}

void MainWindow::renderSearchResults(const QJsonArray &results)
{
    // 保存最近数据并标记当前区域
    lastResults = results;
    showingList = true;
    // 使用网格矩形卡片展示，复用推荐区域容器
    auto *container = ui->recommendationsArea;
    auto *layout = ensureVBoxLayout(container);
    clearLayout(layout);
    // 排序工具条（商城页）
    {
        auto *bar = new QHBoxLayout();
        bar->setContentsMargins(0,0,0,0);
        auto *lbl = new QLabel(tr("排序:"), container);
        auto *cmb = new QComboBox(container); cmb->setObjectName("mallSortCombo");
        // 四个选项：销量优先、价格升序、价格降序、打折优先
        cmb->addItem(tr("销量优先"), QStringLiteral("sales_desc"));
        cmb->addItem(tr("价格升序"), QStringLiteral("price_asc"));
        cmb->addItem(tr("价格降序"), QStringLiteral("price_desc"));
        cmb->addItem(tr("打折优先"), QStringLiteral("discount_first"));
        // 从窗口属性恢复选择，默认打折优先
        QString mode = this->property("mallSortMode").toString();
        if (mode.isEmpty()) mode = QStringLiteral("discount_first");
        for (int i=0;i<cmb->count();++i) { if (cmb->itemData(i).toString()==mode) { cmb->setCurrentIndex(i); break; } }
        bar->addWidget(lbl); bar->addWidget(cmb); bar->addStretch(1);
        auto *barHost = new QWidget(container); barHost->setLayout(bar); layout->addWidget(barHost);
        // 选择变化时，记录到属性并按最新规则重新渲染当前 lastResults，同时请求第一页（服务端排序覆盖全量）
        connect(cmb, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, cmb]{
            this->setProperty("mallSortMode", cmb->currentData().toString());
            // 立即请求第一页，带上排序参数；本地立即重排作为视觉反馈
            requestProductsPage(1);
            if (!lastResults.isEmpty()) renderSearchResults(lastResults);
        });
    }
    auto *title = new QLabel(tr("商品列表"), container);
    title->setWordWrap(true);
    title->setStyleSheet("font-weight:600;margin:6px 0;");
    layout->addWidget(title);
    if (results.isEmpty()) {
        // 不在内容区插入提示，统一在页脚展示“没有找到相关商品”
        this->setProperty("mallNoResults", true);
        this->setProperty("mallNoResultsText", tr("没有找到相关商品"));
        mallLoading = false;
        mallHasMore = false;
        updateMallFooter();
        return;
    } else {
        this->setProperty("mallNoResults", false);
        this->setProperty("mallNoResultsText", QString());
    }
    // 根据选择应用排序（本地排序，若后端已排序则保持一致；此处只对当前页数据排序）
    QString mode = this->property("mallSortMode").toString();
    if (mode.isEmpty()) mode = QStringLiteral("discount_first");
    QList<QJsonObject> list; list.reserve(results.size());
    for (const auto &v : results) list.push_back(v.toObject());
    auto effectivePrice = [](const QJsonObject &o){
        const double price = o.value("price").toDouble();
        const double discount = o.value("discountPrice").toDouble(0.0);
        const bool onSale = o.value("onSale").toBool();
        if (onSale && discount>0.0 && discount<price) return discount; else return price;
    };
    auto hasDiscount = [](const QJsonObject &o){
        const double price = o.value("price").toDouble();
        const double discount = o.value("discountPrice").toDouble(0.0);
        const bool onSale = o.value("onSale").toBool();
        return onSale && discount>0.0 && discount<price;
    };
    std::sort(list.begin(), list.end(), [&](const QJsonObject &a, const QJsonObject &b){
        if (mode == QLatin1String("sales_desc")) {
            const int sa = a.value("sales").toInt();
            const int sb = b.value("sales").toInt();
            if (sa != sb) return sa > sb; // 降序
            return effectivePrice(a) < effectivePrice(b); // 次级：低价优先
        } else if (mode == QLatin1String("price_asc")) {
            const double pa = effectivePrice(a), pb = effectivePrice(b);
            if (pa != pb) return pa < pb;
            return a.value("sales").toInt() > b.value("sales").toInt();
        } else if (mode == QLatin1String("price_desc")) {
            const double pa = effectivePrice(a), pb = effectivePrice(b);
            if (pa != pb) return pa > pb;
            return a.value("sales").toInt() > b.value("sales").toInt();
        } else /* discount_first */ {
            const bool da = hasDiscount(a), db = hasDiscount(b);
            if (da != db) return da && !db; // 有折扣在前
            // 次级：按有效价升序
            const double pa = effectivePrice(a), pb = effectivePrice(b);
            if (pa != pb) return pa < pb;
            return a.value("sales").toInt() > b.value("sales").toInt();
        }
    });
    QJsonArray sorted; for (const auto &o : list) sorted.append(o);
    auto *grid = new QGridLayout();
    grid->setContentsMargins(4,4,4,4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    int available = container->width();
    int colCount = computeColumns(available);
    int idx = 0;
    for (auto v : sorted) {
        const auto o = v.toObject();
        int pid = o.value("product_id").toInt();
        QString name = o.value("name").toString(QLatin1String("商品"));
        double price = o.value("price").toDouble();
        int stock = o.value("stock").toInt(-1);

    auto *card = new QFrame(container);
    card->setFrameShape(QFrame::StyledPanel);
    card->setObjectName("mallCard");
    card->setStyleSheet("");
        card->setMinimumSize(190, 130);
            auto *vbox = new QVBoxLayout(card);
        vbox->setContentsMargins(8,8,8,8);
        vbox->setSpacing(4);
        // 右上角角标：优先“售罄”，否则“新品”
        bool showSoldOut2 = (stock == 0);
        bool showNew2 = (!showSoldOut2 && o.value("isNew").toBool());
        if (showSoldOut2 || showNew2) {
            auto *badgeRow = new QHBoxLayout();
            badgeRow->setContentsMargins(0,0,0,0);
            badgeRow->addStretch(1);
            auto *badge = new QLabel(showSoldOut2 ? tr("售罄") : tr("新品"), card);
            if (showSoldOut2) badge->setStyleSheet("QLabel{background:#E53935;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            else badge->setStyleSheet("QLabel{background:#34A853;color:#fff;border-radius:10px;padding:2px 8px;font-weight:600;font-size:12px;}");
            badgeRow->addWidget(badge, 0, Qt::AlignRight);
            vbox->addLayout(badgeRow);
        }
        // 图片区域（固定高，等比缩放）
        {
            auto *img = new QLabel(card);
            img->setObjectName("mallImageHolder");
            img->setAlignment(Qt::AlignCenter);
            img->setMinimumHeight(100);
            // style by QSS
            vbox->addWidget(img);
            // 若有 image_url 字段，发起加载
            const QString imgUrl = o.value("image_url").toString();
            if (!imgUrl.isEmpty()) {
                setImageFromUrl(imgUrl, img, QSize(160, 100));
            }
        }
        auto *nameLbl = new QLabel(name, card);
        nameLbl->setStyleSheet("font-weight:600;");
        nameLbl->setWordWrap(true);
        nameLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        nameLbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        {
            QFontMetrics fm(nameLbl->font());
            nameLbl->setFixedHeight(fm.lineSpacing()*2 + 4);
            nameLbl->setToolTip(name);
        }
    QLabel *priceLbl2 = nullptr;
    QLabel *discountLbl2 = nullptr;
    bool onSale2 = o.value("onSale").toBool();
    double discount2 = o.value("discountPrice").toDouble(0.0);
    bool hasDiscount2 = onSale2 && discount2 > 0.0 && discount2 < price;
        if (hasDiscount2) {
        priceLbl2 = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
        priceLbl2->setStyleSheet("color:#999;text-decoration:line-through;");
        discountLbl2 = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(discount2, 'f', 2)), card);
            // 暗色主题采用更柔和的红色，提高可读性
            const bool dark = (qApp->palette().color(QPalette::Window).value() < 80) || qApp->styleSheet().contains("#1a1a1c");
            discountLbl2->setStyleSheet(dark ? "color:#ff6b6b;font-weight:700;" : "color:#E53935;font-weight:700;");
    } else {
        priceLbl2 = new QLabel(QStringLiteral("\x00A5 %1").arg(QString::number(price, 'f', 2)), card);
    }
        auto *btnRow = new QHBoxLayout();
    auto *detailBtn = new QPushButton(tr("详情"), card);
    auto *addBtn = new QPushButton(tr("加入购物车"), card);
    detailBtn->setMinimumSize(64, 28);
    addBtn->setMinimumSize(86, 28);
        if (stock == 0) {
            auto *nmEff = new QGraphicsOpacityEffect(card); nmEff->setOpacity(0.6); nameLbl->setGraphicsEffect(nmEff);
            auto *prEff = new QGraphicsOpacityEffect(card); prEff->setOpacity(0.6); priceLbl2->setGraphicsEffect(prEff);
            addBtn->setEnabled(false);
        }
        btnRow->addWidget(detailBtn);
        btnRow->addWidget(addBtn);
        btnRow->addStretch(1);
        vbox->addWidget(nameLbl);
        vbox->addWidget(priceLbl2);
        if (discountLbl2) vbox->addWidget(discountLbl2);
        // 销量标签（若返回了 sales 字段）
        if (o.contains("sales")) {
            const int sales = o.value("sales").toInt();
            auto *salesLbl = new QLabel(tr("销量 %1").arg(sales), card);
            salesLbl->setStyleSheet("color:#666;font-size:12px;");
            vbox->addWidget(salesLbl);
        }
        vbox->addStretch(1); // 将按钮推到底部
        vbox->addLayout(btnRow); // 把按钮行加入布局

        int r = idx / colCount;
        int c = idx % colCount;
        grid->addWidget(card, r, c);
        ++idx;

    // 缓存商品对象（含 sizes），供 addToCart 使用（使用全局指针）
    if (!productCachePtr2) productCachePtr2 = new QHash<int,QJsonObject>();
    (*productCachePtr2)[pid] = o;
        connect(detailBtn, &QPushButton::clicked, this, [this, pid]{ onProductClicked(pid); });
        connect(addBtn, &QPushButton::clicked, this, [this, pid, stock]{ addToCart(pid, stock, -1); });
    }
    auto *gridHost2 = new QWidget(container);
    gridHost2->setLayout(grid);
    layout->addWidget(gridHost2);
    lastColumns = colCount;
    updateMallFooter();
}

int MainWindow::computeColumns(int availableWidth) const
{
    if (availableWidth >= 1600) return 5;
    if (availableWidth >= 1366) return 4;
    return 3;
}

void MainWindow::reflowGrids()
{
    QWidget *container = ui->recommendationsArea; // 推荐区与商品列表复用此容器
    if (!container) return;
    int available = container->width();
    int col = computeColumns(available);
    if (col == lastColumns) return; // 未跨阈值，不重排
    // 根据当前展示内容重绘
    if (showingList) {
        if (!lastResults.isEmpty()) {
            renderSearchResults(lastResults);
        }
    } else {
        if (!lastRecommendations.isEmpty()) {
            renderRecommendations(lastRecommendations);
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    reflowGrids();
    if (currentView == ViewMode::Home && homeHeaderImage && homeHeaderImage->isVisible()) {
        applyHomeHeaderImage();
    }
    // 首页时联动刷新轮播尺寸与左右预览，避免主图显示不全
    if (currentView == ViewMode::Home && carouselImageLabel) {
        refreshCarouselPixmap();
        if (carouselPrevLabel && carouselNextLabel && !carouselOriginals.isEmpty()) {
            int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
            int sideW = qMax(110, (contentW - 40) * 10 / 100);
            int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
            const int maxSideH = qMax(110, mainH * 75 / 100);
            auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
            const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
            const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
            carouselPrevLabel->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
            carouselNextLabel->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
            carouselPrevLabel->setFixedWidth(sideW);
            carouselNextLabel->setFixedWidth(sideW);
            carouselPrevLabel->setFixedHeight(carouselPrevLabel->pixmap(Qt::ReturnByValue).height());
            carouselNextLabel->setFixedHeight(carouselNextLabel->pixmap(Qt::ReturnByValue).height());
        }
    }
    // 让购物车窗口随主窗缩放并居中停靠
    if (cart && cart->isVisible()) {
        const int w = qMax(720, this->width() * 85 / 100);
        const int h = qMax(520, this->height() * 85 / 100);
        cart->resize(w, h);
        const QPoint center = this->geometry().center();
        QRect r = cart->frameGeometry();
        r.moveCenter(center);
        cart->move(r.topLeft());
    }
}

void MainWindow::showProductDetail(const QJsonObject &product)
{
    int pid = product.value("product_id").toInt();
    QString name = product.value("name").toString();
    double price = product.value("price").toDouble();
    int stock = product.value("stock").toInt(-1);
    int sales = product.value("sales").toInt(-1);
    QMessageBox box(this);
    // in showProductDetail, read discount and render both prices if applicable
    // before composing text
    bool onSaleD = product.value("onSale").toBool();
    double discountD = product.value("discountPrice").toDouble(0.0);
    bool hasDiscountD = onSaleD && discountD > 0.0 && discountD < price;
    QString priceLine;
    if (hasDiscountD) {
        const bool dark = (qApp->palette().color(QPalette::Window).value() < 80) || qApp->styleSheet().contains("#1a1a1c");
        const QString discColor = dark ? "#ff6b6b" : "#E53935";
        priceLine = QString("<span style='color:#999;text-decoration:line-through;'>%1</span> <span style='color:%3;font-weight:700;'>%2</span>")
                    .arg(QStringLiteral("\x00A5%1").arg(QString::number(price, 'f', 2)))
                    .arg(QStringLiteral("\x00A5%1").arg(QString::number(discountD, 'f', 2)))
                    .arg(discColor);
    } else {
        priceLine = QStringLiteral("\x00A5%1").arg(QString::number(price, 'f', 2));
    }
    // 可选描述；将属性改为竖排逐行展示
    const QString desc = product.value("description").toString();
    QString text;
    text += QString("<div><b>名称：</b>%1</div>").arg(name.toHtmlEscaped());
    if (!desc.isEmpty()) {
        text += QString("<div><b>描述：</b>%1</div>").arg(desc.toHtmlEscaped());
    }
    text += QString("<div><b>价格：</b>%1</div>").arg(priceLine);
    if (stock >= 0) text += QString("<div><b>库存：</b>%1</div>").arg(stock);
    if (sales >= 0) text += QString("<div><b>销量：</b>%1</div>").arg(sales);
    // 库存为0显示醒目的售罄徽标
    bool soldOut = (stock == 0);
    if (soldOut) {
        // 使用富文本渲染一个红底白字圆角徽标
        text += QString("<div style='margin-top:6px;'>%1</div>")
                    .arg("<span style='background:#E53935;color:#fff;border-radius:12px;padding:4px 10px;font-weight:600;'>售罄</span>");
    }
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    QPushButton *add = box.addButton(tr("加入购物车"), QMessageBox::AcceptRole);
    if (soldOut) add->setEnabled(false);
    box.addButton(tr("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (!soldOut && box.clickedButton() == add) {
        // 自定义尺码选择对话框（按钮网格）
        class SizeSelectDialog : public QDialog {
        public:
            int selectedSize = -1;
            explicit SizeSelectDialog(QWidget* parent, const QList<QPair<int,int>>& ss): QDialog(parent) {
                setWindowTitle(tr("选择尺码"));
                setModal(true);
                QVBoxLayout *root = new QVBoxLayout(this);
                QLabel *tip = new QLabel(tr("请选择一个可用尺码"), this);
                root->addWidget(tip);
                QGridLayout *grid = new QGridLayout();
                grid->setHorizontalSpacing(8); grid->setVerticalSpacing(8);
                int colCount = 5; int idx=0;
                for (auto pair : ss) {
                    int s = pair.first; int st = pair.second; bool enabled = (st != 0);
                    QPushButton *btn = new QPushButton(QString::number(s) + QString("\n库存:%1").arg(st>=0? st:0), this);
                    btn->setCheckable(true);
                    btn->setEnabled(enabled);
                    btn->setMinimumSize(66,52);
                    btn->setStyleSheet(
                        "QPushButton{background-color:#ffffff;color:#333;border:1px solid #d0d0d0;border-radius:6px;padding:4px;}"
                        "QPushButton:hover{background-color:#f5f7ff;border-color:#a3c5ff;}"
                        "QPushButton:checked{background-color:#1677ff;color:#ffffff;border-color:#1677ff;}"
                        "QPushButton:disabled{background-color:#f2f2f2;color:#999;border-color:#e0e0e0;}"
                    );
                    int r = idx / colCount; int c = idx % colCount; idx++;
                    grid->addWidget(btn, r, c);
                    connect(btn, &QPushButton::clicked, this, [this, btn, s](){
                        // 互斥选择
                        for (auto b: findChildren<QPushButton*>()) if (b->isCheckable() && b!=btn) b->setChecked(false);
                        btn->setChecked(true);
                        selectedSize = s;
                    });
                }
                root->addLayout(grid);
                QHBoxLayout *actions = new QHBoxLayout();
                actions->addStretch();
                QPushButton *okBtn = new QPushButton(tr("确定"), this);
                QPushButton *cancelBtn = new QPushButton(tr("取消"), this);
                actions->addWidget(okBtn); actions->addWidget(cancelBtn);
                root->addLayout(actions);
                connect(okBtn, &QPushButton::clicked, this, [this](){ if (selectedSize>0) accept(); else QMessageBox::information(this, tr("提示"), tr("请先选择尺码")); });
                connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
                resize(420, 260);
            }
        };

        QList<QPair<int,int>> sizeStock; // {size, stock}
        if (product.contains("sizes") && product.value("sizes").isArray()) {
            for (const auto &v : product.value("sizes").toArray()) {
                const auto o = v.toObject();
                sizeStock.append({ o.value("size").toInt(), o.value("stock").toInt() });
            }
        }
        if (sizeStock.isEmpty()) { for (int s=37; s<=45; ++s) sizeStock.append({s, -1}); }
        SizeSelectDialog dlg(this, sizeStock);
        if (dlg.exec() == QDialog::Accepted && dlg.selectedSize>0) {
            addToCart(pid, stock, dlg.selectedSize);
        }
    }
}

// 根据缓存获取某个商品在特定尺码下的可用库存；若未知返回 -1
static int getAvailableStockForSizeFromCaches(int productId, int size)
{
    extern QHash<int,QJsonObject> *productCachePtr;
    extern QHash<int,QJsonObject> *productCachePtr2;
    auto fetch = [&](QHash<int,QJsonObject>* cache)->int{
        if (!cache) return -1;
        if (!cache->contains(productId)) return -1;
        const QJsonObject o = (*cache)[productId];
        if (!o.contains("sizes")) return -1;
        const auto arr = o.value("sizes").toArray();
        for (const auto &v : arr) {
            const auto it = v.toObject();
            if (it.value("size").toInt() == size) return it.value("stock").toInt(-1);
        }
        return -1;
    };
    int s = fetch(productCachePtr);
    if (s >= 0) return s;
    s = fetch(productCachePtr2);
    return s;
}

void MainWindow::addToCart(int productId, int stock, int size)
{
    // 200ms 防抖，避免按钮连点导致多次请求
    if (monotonic.elapsed() - lastAddMs < 200) return;
    lastAddMs = monotonic.elapsed();
    if (!socket) return;
    // 若整体库存为 0 则直接提示（但以尺码库存为准，若未知则继续）
    if (stock == 0 && size <= 0) { statusBar()->showMessage(tr("该商品暂无库存，无法加入购物车"), 3000); return; }
    if (size <= 0) {
        // 统一使用与详情路径相同的对话框外观与逻辑（SizeSelectDialog），先保证拿到最新 sizes
        extern QHash<int,QJsonObject> *productCachePtr; 
        extern QHash<int,QJsonObject> *productCachePtr2;
        auto fetchSizes = [&](QList<QPair<int,int>>& out){
            QJsonObject prod;
            if (productCachePtr && productCachePtr->contains(productId)) prod = (*productCachePtr)[productId];
            else if (productCachePtr2 && productCachePtr2->contains(productId)) prod = (*productCachePtr2)[productId];
            if (prod.contains("sizes") && prod.value("sizes").isArray()) {
                for (auto v: prod.value("sizes").toArray()) { auto o=v.toObject(); out.append({o.value("size").toInt(), o.value("stock").toInt()}); }
            }
        };
        QList<QPair<int,int>> sizeStock; fetchSizes(sizeStock);
        if (sizeStock.isEmpty() && socket) {
            // 纯异步：请求 detail，等待 onReadyRead -> handleProductDetailForSizeSelect
            waitingSizeSelectProductId = productId;
            QJsonObject req; req["type"]="get_product_detail"; req["product_id"]=productId; QJsonDocument d(req); QByteArray pl=d.toJson(QJsonDocument::Compact); pl.append('\n'); enqueueFrame(pl);
            return; // 等待回调
        }
        // 已有库存数据（缓存命中）直接弹“尺码 + 数量”统一对话框
    class AddToCartDialog : public QDialog { public: int selectedSize=-1; int selectedQty=1; AddToCartDialog(QWidget* parent,const QList<QPair<int,int>>& ss):QDialog(parent){ setWindowTitle(QObject::tr("加入购物车")); setModal(true); QVBoxLayout *root=new QVBoxLayout(this); auto *tip=new QLabel(QObject::tr("请选择尺码，并设置数量"), this); root->addWidget(tip); QGridLayout *grid=new QGridLayout(); grid->setHorizontalSpacing(8); grid->setVerticalSpacing(8); int col=5, idx=0; for (auto pair:ss){ int s=pair.first, st=pair.second; bool enabled=(st>0); auto *btn=new QPushButton(QString::number(s)+QString("\n库存:%1").arg(st), this); btn->setCheckable(true); btn->setEnabled(enabled); btn->setMinimumSize(66,52); btn->setStyleSheet("QPushButton{background:#ffffff;color:#333;border:1px solid #d0d0d0;border-radius:6px;padding:4px;}QPushButton:hover{background:#f5f7ff;border-color:#a3c5ff;}QPushButton:checked{background:#1677ff;color:#ffffff;border-color:#1677ff;}QPushButton:disabled{background:#f2f2f2;color:#999;border-color:#e0e0e0;}"); int r=idx/col, c=idx%col; ++idx; grid->addWidget(btn, r, c); } root->addLayout(grid); // 数量
            auto *qtyRow = new QHBoxLayout(); qtyRow->addWidget(new QLabel(QObject::tr("数量"), this)); auto *spin = new QSpinBox(this); spin->setRange(1, 1); spin->setValue(1); spin->setEnabled(false); qtyRow->addWidget(spin); qtyRow->addStretch(1); root->addLayout(qtyRow);
            // Actions
            auto *acts=new QHBoxLayout(); acts->addStretch(); auto *ok=new QPushButton(QObject::tr("确定"),this); ok->setEnabled(false); ok->setStyleSheet("QPushButton{background:#f0f0f0;color:#999;border:1px solid #ddd;border-radius:6px;padding:4px 12px;}QPushButton:enabled{background:#1677ff;color:#fff;border-color:#1677ff;}"); auto *cancel=new QPushButton(QObject::tr("取消"),this); acts->addWidget(ok); acts->addWidget(cancel); root->addLayout(acts);
            // 行为：选择尺码 -> 启用 OK 与数量，并将上限设为库存
            for (auto btn: findChildren<QPushButton*>()) if (btn->isCheckable()) QObject::connect(btn,&QPushButton::clicked,this,[this,ok,btn,spin](){ for (auto b: findChildren<QPushButton*>()) if (b->isCheckable() && b!=btn) b->setChecked(false); btn->setChecked(true); selectedSize = btn->text().split('\n').first().toInt(); // 解析库存
                    int st = 1; const QStringList lines = btn->text().split('\n'); if (lines.size()>=2) { QString t=lines.at(1); t.remove(0, t.indexOf(QLatin1Char(':'))+1); st = t.trimmed().toInt(); }
                    spin->setEnabled(true); spin->setRange(1, qMax(1, st)); ok->setEnabled(true); });
            QObject::connect(ok, &QPushButton::clicked, this, [this,spin](){ selectedQty = spin->value(); if (selectedSize>0) accept(); else QMessageBox::information(this, QObject::tr("提示"), QObject::tr("请先选择尺码")); });
            QObject::connect(cancel, &QPushButton::clicked, this, &QDialog::reject); resize(460, 300);
        }
        };
        AddToCartDialog dlg(this, sizeStock);
        if (dlg.exec()==QDialog::Accepted && dlg.selectedSize>0) {
            size = dlg.selectedSize;
            const int quantity = qMax(1, dlg.selectedQty);
            QJsonObject req; req["type"] = QStringLiteral("add_to_cart"); req["product_id"] = productId; req["quantity"] = quantity; req["size"] = size; if (!currentUsername.isEmpty()) req["username"] = currentUsername; QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); enqueueFrame(payload);
        }
        return;
    }
    // 已有尺码：弹出数量选择，数量上限尽量取该尺码实际库存
    int perSizeStock = getAvailableStockForSizeFromCaches(productId, size);
    int maxQty =  (perSizeStock >= 1) ? perSizeStock : (stock >= 1 ? stock : 999);
    bool ok = false;
    int quantity = QInputDialog::getInt(this, tr("加入购物车"), tr("数量"), 1, 1, maxQty, 1, &ok);
    if (!ok) return;
    QJsonObject req; req["type"] = QStringLiteral("add_to_cart"); req["product_id"] = productId; req["quantity"] = quantity; req["size"] = size; if (!currentUsername.isEmpty()) req["username"] = currentUsername; QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); enqueueFrame(payload);
}

void MainWindow::handleProductDetailForSizeSelect(const QJsonObject &product)
{
    // 将此商品写入缓存，以便数量上限判断
    if (product.contains("product_id")) {
        const int pid = product.value("product_id").toInt();
        if (!productCachePtr2) productCachePtr2 = new QHash<int,QJsonObject>();
        (*productCachePtr2)[pid] = product;
    }
    QList<QPair<int,int>> sizeStock;
    if (product.contains("sizes") && product.value("sizes").isArray()) {
        for (const auto &v : product.value("sizes").toArray()) {
            const auto o = v.toObject();
            sizeStock.append({ o.value("size").toInt(), o.value("stock").toInt() });
        }
    }
    if (sizeStock.isEmpty()) { QMessageBox::information(this, tr("提示"), tr("该商品暂无可选尺码")); return; }
    // 统一“尺码 + 数量”对话框
    class AddToCartDialog2 : public QDialog { public: int selectedSize=-1; int selectedQty=1; AddToCartDialog2(QWidget* parent,const QList<QPair<int,int>>& ss):QDialog(parent){ setWindowTitle(QObject::tr("加入购物车")); setModal(true); QVBoxLayout *root=new QVBoxLayout(this); root->addWidget(new QLabel(QObject::tr("请选择尺码，并设置数量"), this)); QGridLayout *grid=new QGridLayout(); grid->setHorizontalSpacing(8); grid->setVerticalSpacing(8); int col=5, idx=0; for (auto pair:ss){ int s=pair.first, st=pair.second; bool enabled=(st>0); auto *btn=new QPushButton(QString::number(s)+QString("\n库存:%1").arg(st), this); btn->setCheckable(true); btn->setEnabled(enabled); btn->setMinimumSize(66,52); btn->setStyleSheet("QPushButton{background-color:#ffffff;color:#333;border:1px solid #d0d0d0;border-radius:6px;padding:4px;}QPushButton:hover{background-color:#f5f7ff;border-color:#a3c5ff;}QPushButton:checked{background-color:#1677ff;color:#ffffff;border-color:#1677ff;}QPushButton:disabled{background-color:#f2f2f2;color:#999;border-color:#e0e0e0;}"); int r=idx/col, c=idx%col; ++idx; grid->addWidget(btn, r, c); } root->addLayout(grid); auto *qtyRow = new QHBoxLayout(); qtyRow->addWidget(new QLabel(QObject::tr("数量"), this)); auto *spin = new QSpinBox(this); spin->setRange(1,1); spin->setEnabled(false); qtyRow->addWidget(spin); qtyRow->addStretch(1); root->addLayout(qtyRow); auto *acts=new QHBoxLayout(); acts->addStretch(); auto *ok=new QPushButton(QObject::tr("确定"),this); ok->setEnabled(false); ok->setStyleSheet("QPushButton{background:#f0f0f0;color:#999;border:1px solid #ddd;border-radius:6px;padding:4px 12px;}QPushButton:enabled{background:#1677ff;color:#fff;border-color:#1677ff;}"); auto *cancel=new QPushButton(QObject::tr("取消"),this); acts->addWidget(ok); acts->addWidget(cancel); root->addLayout(acts); for (auto btn: findChildren<QPushButton*>()) if (btn->isCheckable()) QObject::connect(btn,&QPushButton::clicked,this,[this,ok,btn,spin](){ for (auto b: findChildren<QPushButton*>()) if (b->isCheckable() && b!=btn) b->setChecked(false); btn->setChecked(true); selectedSize = btn->text().split('\n').first().toInt(); int st = 1; const QStringList lines = btn->text().split('\n'); if (lines.size()>=2) { QString t=lines.at(1); t.remove(0, t.indexOf(QLatin1Char(':'))+1); st = t.trimmed().toInt(); } spin->setEnabled(true); spin->setRange(1, qMax(1, st)); ok->setEnabled(true); }); QObject::connect(ok, &QPushButton::clicked, this, [this,spin](){ selectedQty = spin->value(); if (selectedSize>0) accept(); else QMessageBox::information(this, QObject::tr("提示"), QObject::tr("请先选择尺码")); }); QObject::connect(cancel, &QPushButton::clicked, this, &QDialog::reject); resize(460,300);} };
    AddToCartDialog2 dlg(this, sizeStock);
    if (dlg.exec()==QDialog::Accepted && dlg.selectedSize>0) {
        int pid = product.value("product_id").toInt();
        lastAddMs = 0; // 绕过防抖
        QJsonObject req; req["type"] = QStringLiteral("add_to_cart"); req["product_id"] = pid; req["size"] = dlg.selectedSize; req["quantity"] = qMax(1, dlg.selectedQty); if (!currentUsername.isEmpty()) req["username"] = currentUsername; QJsonDocument doc(req); QByteArray payload = doc.toJson(QJsonDocument::Compact); payload.append('\n'); enqueueFrame(payload);
    }
}

void MainWindow::requestProductsPage(int page)
{
    if (!socket) return;
    if (page < 1) page = 1;
    currentPage = page;
    // 进入加载中状态并更新页尾提示
    mallLoading = true;
    updateMallFooter();
    QJsonObject req;
    req["type"] = "get_products";
    req["page"] = currentPage;
    req["size"] = pageSize;
    // 携带排序参数（如果未设置则使用默认：打折优先）
    QString mode = this->property("mallSortMode").toString();
    if (mode.isEmpty()) mode = QStringLiteral("discount_first");
    req["sort"] = mode;
    QJsonDocument doc(req);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');
    enqueueFrame(payload);
}

void MainWindow::on_prevPage_clicked()
{
    if (currentPage > 1) {
        requestProductsPage(currentPage - 1);
    }
}

void MainWindow::on_nextPage_clicked()
{
    int totalPages = (pageSize>0) ? ((totalProducts + pageSize - 1) / pageSize) : 1;
    if (currentPage < totalPages) {
        requestProductsPage(currentPage + 1);
    }
}

void MainWindow::ensureMallScrollArea()
{
    if (mallScrollInit) return;
    QWidget *container = ui->recommendationsArea;
    if (!container) return;
    // 若已经在 QScrollArea 中则跳过
    if (qobject_cast<QScrollArea*>(container->parentWidget())) { mallScrollInit = true; mallScrollArea = qobject_cast<QScrollArea*>(container->parentWidget()); return; }
    // 创建滚动容器，并将原区域移入
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("mallScrollArea");
    scroll->setWidgetResizable(true);
    // 使用一个新的容器作为 viewport 内容
    auto *viewport = new QWidget(scroll);
    auto *vlay = new QVBoxLayout(viewport); vlay->setContentsMargins(0,0,0,0); vlay->setSpacing(0);
    scroll->setWidget(viewport);
    // 将 scroll 放回原位置（底部分页器之上）
    if (auto rootLayout = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        // 插入到底分页器之前（倒数第二个），若未知则追加
        int insertIndex = rootLayout->count() > 0 ? rootLayout->count() - 1 : rootLayout->count();
        rootLayout->insertWidget(insertIndex, scroll);
    }
    mallScrollArea = scroll;
    mallScrollViewport = viewport;
    mallScrollInit = true;
    // 监听滚动，触底加载
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int){ tryLoadNextProductsOnScroll(); });
}

// 将推荐/列表区域放入滚动区
void MainWindow::attachRecommendationsToScroll()
{
    if (!mallScrollInit) ensureMallScrollArea();
    if (!mallScrollViewport) return;
    QWidget *container = ui->recommendationsArea;
    if (!container) return;
    if (container->parent() == mallScrollViewport) return; // 已在滚动区
    // 从 rootLayout 移除并放入 viewport
    if (auto rootLayout = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        rootLayout->removeWidget(container);
    }
    container->setParent(mallScrollViewport);
    if (auto vlay = qobject_cast<QVBoxLayout*>(mallScrollViewport->layout())) {
        // 避免重复添加：若已存在则不再添加
        bool exists = false;
        for (int i=0;i<vlay->count();++i) {
            if (vlay->itemAt(i)->widget() == container) { exists = true; break; }
        }
        if (!exists) {
            // 保持内容+stretch 形成可滚动空间
            int insertIdx = qMax(0, vlay->count()-1);
            vlay->insertWidget(insertIdx, container);
            if (vlay->count()==0 || (vlay->itemAt(vlay->count()-1) && vlay->itemAt(vlay->count()-1)->spacerItem()==nullptr)) {
                vlay->addStretch(1);
            }
        }
    }
}

// 将推荐/列表区域还原到 rootLayout（退出商城或切换到非滚动页时）
void MainWindow::attachRecommendationsToRoot()
{
    QWidget *container = ui->recommendationsArea;
    if (!container) return;
    if (container->parent() != mallScrollViewport) return; // 已经在 root
    container->setParent(this);
    if (auto rootLayout = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        // 放在底部分页器之上（保持原先位置习惯）
        int insertIndex = rootLayout->count() > 0 ? rootLayout->count() - 1 : rootLayout->count();
        rootLayout->insertWidget(insertIndex, container);
    }
}

void MainWindow::tryLoadNextProductsOnScroll()
{
    // 仅在商城主列表视图生效
    if (currentView != ViewMode::Mall) return;
    // 搜索模式不触发通用列表的翻页预取
    if (searchActive) return;
    if (!mallScrollArea || !mallHasMore || mallLoading) return;
    auto *sb = mallScrollArea->verticalScrollBar();
    if (!sb) return;
    int threshold = 120; // 距底 120px 预加载
    // maximum==0 表示内容高度不超过 viewport；此时也尝试加载下一页
    if ((sb->maximum() == 0) || (sb->maximum() - sb->value() <= threshold)) {
        requestProductsPage(currentPage + 1);
    }
}

// 创建或更新商城页的页尾提示（“加载中…” / “已到底啦”）
void MainWindow::updateMallFooter()
{
    if (currentView != ViewMode::Mall) return;
    QWidget *container = ui->recommendationsArea;
    if (!container) return;
    // 优先把页尾挂载到滚动 viewport，保证处于滚动内容的末尾
    QWidget *parentForFooter = mallScrollViewport ? mallScrollViewport : container;
    QVBoxLayout *layout = nullptr;
    if (mallScrollViewport && mallScrollViewport->layout()) {
        layout = qobject_cast<QVBoxLayout*>(mallScrollViewport->layout());
    }
    if (!layout) {
        layout = qobject_cast<QVBoxLayout*>(ensureVBoxLayout(container));
        parentForFooter = container;
    }
    // 查找或创建底部提示区
    QWidget *footerHost = parentForFooter->findChild<QWidget*>("mallFooter");
    QLabel *footerLabel = footerHost ? footerHost->findChild<QLabel*>("mallFooterLabel") : nullptr;
    QProgressBar *spinner = footerHost ? footerHost->findChild<QProgressBar*>("mallFooterSpinner") : nullptr;
    QFrame *divider = footerHost ? footerHost->findChild<QFrame*>("mallFooterDivider") : nullptr;
    if (!footerHost) {
        footerHost = new QWidget(parentForFooter);
        footerHost->setObjectName("mallFooter");
        auto *vb = new QVBoxLayout(footerHost);
        vb->setContentsMargins(0,12,0,12);
        // 顶部分割线（仅在“已到底啦”显示）
        divider = new QFrame(footerHost);
    divider->setObjectName("mallFooterDivider");
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
        divider->setVisible(false);
        vb->addWidget(divider);
        // 中间一行：可选 spinner + 文案
        auto *hb = new QHBoxLayout();
        hb->setContentsMargins(0,0,0,0);
        hb->addStretch(1);
        spinner = new QProgressBar(footerHost);
        spinner->setObjectName("mallFooterSpinner");
        spinner->setFixedSize(80, 8);
        spinner->setTextVisible(false);
        spinner->setRange(0, 0); // busy
        spinner->setVisible(false);
        hb->addWidget(spinner);
    footerLabel = new QLabel(footerHost);
        footerLabel->setObjectName("mallFooterLabel");
    footerLabel->setStyleSheet("font-size:12px;margin-left:8px;");
        hb->addWidget(footerLabel);
        hb->addStretch(1);
        vb->addLayout(hb);
        // 将页尾插入到可滚动布局末尾（若末尾已有 stretch，则插入到 stretch 之前）
        int insertIndex = layout->count();
        if (insertIndex > 0) {
            auto *lastItem = layout->itemAt(insertIndex - 1);
            if (lastItem && lastItem->spacerItem()) insertIndex -= 1;
        }
        layout->insertWidget(qMax(0, insertIndex), footerHost);
    }
    if (!footerLabel) return;
    // 优先处理“无结果”提示
    const bool noResults = this->property("mallNoResults").toBool();
    const QString noResultsText = this->property("mallNoResultsText").toString();
    if (noResults) {
        if (spinner) spinner->setVisible(false);
        if (divider) divider->setVisible(true);
        footerLabel->setText(noResultsText.isEmpty() ? tr("没有找到相关商品") : noResultsText);
        footerHost->setVisible(true);
        return;
    }
    if (mallLoading) {
        if (spinner) spinner->setVisible(true);
        if (divider) divider->setVisible(false);
        footerLabel->setText(tr("加载中…"));
        footerHost->setVisible(true);
    } else if (!mallHasMore) {
        if (spinner) spinner->setVisible(false);
        if (divider) divider->setVisible(true);
        footerLabel->setText(tr("已到底啦"));
        footerHost->setVisible(true);
    } else {
        // 还有更多但当前不在加载中，可保持提示隐藏或清空
        if (spinner) spinner->setVisible(false);
        if (divider) divider->setVisible(false);
        footerLabel->clear();
        footerHost->setVisible(false);
    }
}

// ---- 视图模式切换 ----
void MainWindow::showHomeView()
{
    // 离开商城页，退出搜索态
    searchActive = false;
    currentSearchKeyword.clear();
    currentView = ViewMode::Home;
    lastNonCartView = ViewMode::Home;
    setTabActive("home");
    activeTabKey = QStringLiteral("home");
    setSearchBarVisible(true);
    // 切回首页时，仅隐藏全屏页（个人中心/订单/聊天），避免频繁销毁引发异步回调悬空
    if (auto p = findChild<QWidget*>("accountPage")) { p->hide(); }
    if (auto p = findChild<QWidget*>("ordersPage")) { p->hide(); }
    if (auto p = findChild<QWidget*>("chatPage")) { p->hide(); }
    // 回到首页前确保轮播计时器处于可控状态（若已存在，稍后 renderCarousel 会重建连接并启动）
    if (carouselTimer) { carouselTimer->stop(); }
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(true);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(true);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(true);
    if (promoTimer && !promoTimer->isActive()) promoTimer->start();
    updateGreeting();
    // 顶部头图已移除
    if (ui->carouselArea) ui->carouselArea->setVisible(true);
    // 隐藏促销区（按用户要求去掉“开学季/会员日”两行）
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    // 首页不使用商城滚动区，恢复到根布局
    attachRecommendationsToRoot();
    if (mallScrollArea) mallScrollArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(true);
    // 隐藏全局分页器（仅商城显示）
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 隐藏购物车页（如存在）
    if (cart) cart->hide();
    // 刷新首页数据
    // 确保本地轮播就绪
    if (!carouselImageLabel) {
        // 先尝试构建一次本地轮播，避免服务器暂未返回数据时显示文字占位
        // 具体内容在 renderCarousel 中也会再次确保
        // 初始化路径列表以便后续 renderCarousel 使用
        carouselLocalPaths = QStringList{
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/4fff3de19840e88d717fb372351454fe.png"),
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/888cc0d3b9d75bc12017b7f0c3393a38.png"),
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/1569583902c0ce638ebd23c952202400.png"),
            QString::fromUtf8("c:/Users/Edward/xwechat_files/wxid_zemit2lb8upw22_c69e/temp/RWTemp/2025-09/9e20f478899dc29eb19741386f9343c8/dd96060742b6578bd89299dbc31c46c4.png")
        };
    }
    loadCarousel();
    // 不再加载促销数据
    loadRecommendations();
}

void MainWindow::showMallView(bool preserveSearch)
{
    currentView = ViewMode::Mall;
    lastNonCartView = ViewMode::Mall;
    setTabActive("mall");
    activeTabKey = QStringLiteral("mall");
    setSearchBarVisible(true);
    // 进入商城页，仅隐藏全屏页
    if (auto p = findChild<QWidget*>("accountPage")) { p->hide(); }
    if (auto p = findChild<QWidget*>("ordersPage")) { p->hide(); }
    if (auto p = findChild<QWidget*>("chatPage")) { p->hide(); }
    // 离开首页，停止轮播
    if (carouselTimer) { carouselTimer->stop(); }
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(false);
    if (promoTimer) promoTimer->stop();
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(true);
    // 启用“滚动加载”模式：隐藏底部分页器控件
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 确保“发现好物”区域被 QScrollArea 包裹，并监听滚动事件
    ensureMallScrollArea();
    attachRecommendationsToScroll();
    if (mallScrollArea) mallScrollArea->setVisible(true);
    // 重置滚动加载状态
    mallLoading = false;
    mallHasMore = true;
    lastResults = QJsonArray{};
    updateMallFooter();
    // 隐藏购物车页
    if (cart) cart->hide();
    // 非搜索场景才自动拉取第一页；搜索场景下等待 search_products 返回
    if (!searchActive) {
        requestProductsPage(1);
    }
}

void MainWindow::showCartView()
{
    // 离开商城页，退出搜索态
    searchActive = false;
    currentSearchKeyword.clear();
    activeTabKey = QStringLiteral("cart");
    setTabActive("cart");
    setSearchBarVisible(false);
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    if (auto pf = findChild<QLabel*>("promoFloatingLabel")) pf->setVisible(false);
    if (promoTimer) promoTimer->stop();
    if (homeHeaderImage) homeHeaderImage->setVisible(false);
    // 进入购物车时，仅隐藏全屏页
    if (auto p = findChild<QWidget*>("accountPage")) { p->hide(); }
    if (auto p = findChild<QWidget*>("ordersPage")) { p->hide(); }
    if (auto p = findChild<QWidget*>("chatPage")) { p->hide(); }
    // 离开首页，停止轮播
    if (carouselTimer) { carouselTimer->stop(); }
    // 创建或重用购物车，并嵌入主布局区域
    if (!cart) {
        cart = new ShoppingCart(socket, this);
        if (!currentUsername.isEmpty()) cart->setUsername(currentUsername);
        // 嵌入到 rootLayout 的末尾（在底部分页之前或之后均可，这里放在分页下方同级，显示时隐藏其他区域）
        if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
            root->addWidget(cart);
        } else {
            // 回退：直接设置父子关系
            cart->setParent(this);
        }
        if (auto backBtn = cart->findChild<QPushButton*>("backButton")) {
            connect(backBtn, &QPushButton::clicked, this, &MainWindow::exitCartView);
        }
    }
    currentView = ViewMode::Cart;
    // 隐藏首页/商城区域和全局分页器
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (mallScrollArea) mallScrollArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(false);
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 显示购物车页面并尽量铺满主界面区域
    cart->setVisible(true);
    cart->raise();
}

void MainWindow::exitCartView()
{
    if (cart) cart->hide();
    // 返回之前的非购物车视图
    if (lastNonCartView == ViewMode::Mall) showMallView(); else showHomeView();
}

void MainWindow::clearToFullPage(QWidget *page)
{
    if (!page) return;
    // 隐藏首页/商城区域、全局分页器、问候标题与活动文案
    if (ui->carouselArea) ui->carouselArea->setVisible(false);
    if (ui->promotionsArea) ui->promotionsArea->setVisible(false);
    attachRecommendationsToRoot();
    if (mallScrollArea) mallScrollArea->setVisible(false);
    if (ui->recommendationsArea) ui->recommendationsArea->setVisible(false);
    if (auto row = findChild<QWidget*>("greetingRow")) row->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    if (cart) cart->hide();
    // 显示全屏页时停止轮播
    if (carouselTimer) { carouselTimer->stop(); }
    // 直接销毁其他全屏页，防止遗留信号/回调导致闪退
    if (auto oldOrders = findChild<QWidget*>("ordersPage")) { if (oldOrders != page) { oldOrders->hide(); } }
    if (auto oldAccount = findChild<QWidget*>("accountPage")) { if (oldAccount != page) { oldAccount->hide(); } }
    if (auto oldChat = findChild<QWidget*>("chatPage")) { if (oldChat != page) { oldChat->hide(); } }
    // 注意：聊天被内嵌在 chatPage 内部，这里不要额外隐藏 chat，否则会出现“只有页面没有内容”
    // 添加并显示当前页
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) {
        if (page->parent() != this && page->parent() != ui->centralwidget) page->setParent(this);
        // 避免重复添加：如果已经在 root 中，则不再 addWidget
        bool alreadyIn = false;
        for (int i=0;i<root->count();++i) {
            if (root->itemAt(i)->widget() == page) { alreadyIn = true; break; }
        }
        if (!alreadyIn) root->addWidget(page);
    }
    page->show();
    page->raise();
}

void MainWindow::updateGreeting()
{
    if (auto lbl = findChild<QLabel*>("greetingLabel")) {
        if (!currentUsername.isEmpty()) lbl->setText(tr("你好！%1").arg(currentUsername));
        else lbl->setText(tr("你好！"));
    }
}

void MainWindow::showAccountView()
{
    // 离开商城页，退出搜索态
    searchActive = false;
    currentSearchKeyword.clear();
    activeTabKey = QStringLiteral("account");
    setTabActive("account");
    setSearchBarVisible(false);
    static qint64 lastAccountReqMs = 0;
    // 若不存在，先创建一个占位的 accountPage，并使用 clearToFullPage 显示
    QWidget *page = findChild<QWidget*>("accountPage");
    if (!page) {
        page = new QWidget(this);
        page->setObjectName("accountPage");
        auto *v = new QVBoxLayout(page);
        auto *topBar = new QHBoxLayout();
    auto *title = new QLabel(tr("个人中心 ✨"), page); title->setStyleSheet("font-weight:600;font-size:16px;");
        auto *back = new QPushButton(tr("返回"), page);
        auto *saveTop = new QPushButton(tr("修改"), page); saveTop->setObjectName("saveTopButton");
        topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(saveTop); topBar->addWidget(back); v->addLayout(topBar);
        // 仅在尚未创建表单时显示占位
        auto *info = new QLabel(tr("正在加载账号信息…"), page);
        info->setObjectName("acc_loading");
        v->addWidget(info);
        if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(page);
            connect(back, &QPushButton::clicked, this, [this, p=QPointer<QWidget>(page)]{
                if (p) { p->hide(); }
                if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView();
            });
    }
    // 已有页面：若已存在表单，隐藏占位；否则确保占位显示
    if (page->findChild<QLineEdit*>("acc_user")) {
        if (auto info = page->findChild<QLabel*>("acc_loading", Qt::FindDirectChildrenOnly)) info->hide();
    } else {
        if (auto info = page->findChild<QLabel*>("acc_loading", Qt::FindDirectChildrenOnly)) { info->setText(tr("正在加载账号信息…")); info->show(); }
    }
    // 进入账号页时强制把顶部按钮文案设为“修改”
    if (auto saveToggle = page->findChild<QPushButton*>("saveTopButton")) saveToggle->setText(tr("修改"));
    clearToFullPage(page);
    // 请求账号信息，填充内容（重用已有请求），500ms 节流
    if (socket) {
        qint64 now = monotonic.elapsed();
    if (now - lastAccountReqMs > 500) {
            lastAccountReqMs = now;
            if (socket->state() == QAbstractSocket::ConnectedState) {
                QJsonObject req; req["type"] = "get_account_info"; if (!currentUsername.isEmpty()) req["username"] = currentUsername; QJsonDocument d(req); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n'); enqueueFrame(p);
            } else {
                qWarning() << "skip get_account_info due to socket state" << socket->state();
                QTimer::singleShot(300, this, [this]{ if (activeTabKey==QLatin1String("account")) showAccountView(); });
            }
        }
    }
}

void MainWindow::showOrdersView()
{
    // 离开商城页，退出搜索态
    searchActive = false;
    currentSearchKeyword.clear();
    activeTabKey = QStringLiteral("orders");
    setTabActive("orders");
    setSearchBarVisible(false);
    static qint64 lastOrdersReqMs = 0;
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    // 隐藏全局分页器，避免与订单分页重复
    if (auto pageInfo = findChild<QWidget*>("pageInfoLabel")) pageInfo->setVisible(false);
    if (auto totalInfo = findChild<QWidget*>("totalInfoLabel")) totalInfo->setVisible(false);
    if (auto gotoLbl = findChild<QWidget*>("gotoLabel")) gotoLbl->setVisible(false);
    if (auto gotoSpin = findChild<QWidget*>("gotoPageSpin")) gotoSpin->setVisible(false);
    if (auto gotoBtn = findChild<QWidget*>("gotoPageButton")) gotoBtn->setVisible(false);
    if (auto prevBtn = findChild<QWidget*>("prevPage")) prevBtn->setVisible(false);
    if (auto nextBtn = findChild<QWidget*>("nextPage")) nextBtn->setVisible(false);
    // 强防御：同步销毁旧的 ordersPage，避免异步回调访问已释放对象
    if (auto old = findChild<QWidget*>("ordersPage")) {
        QObject::disconnect(old, nullptr, nullptr, nullptr);
        old->blockSignals(true);
        old->deleteLater();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    // 显示占位页，避免空白并确保 clearToFullPage 有目标
    QWidget *loading = new QWidget(this);
    loading->setObjectName("ordersPage");
    auto *lv = new QVBoxLayout(loading);
    auto *top = new QHBoxLayout();
    auto *title = new QLabel(tr("历史订单"), loading); title->setStyleSheet("font-weight:600;font-size:16px;");
    auto *back = new QPushButton(tr("返回"), loading);
    top->addWidget(title); top->addStretch(1); top->addWidget(back); lv->addLayout(top);
    auto *info = new QLabel(tr("正在获取历史订单…"), loading); info->setObjectName("ordersLoadingLabel"); lv->addWidget(info);
    if (auto root = ui->centralwidget->findChild<QVBoxLayout*>("rootLayout")) root->addWidget(loading);
    QObject::disconnect(back, nullptr, nullptr, nullptr);
    connect(back, &QPushButton::clicked, this, [this, p=QPointer<QWidget>(loading)]{
        if (p) p->hide();
        if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView();
    }, Qt::UniqueConnection);
    clearToFullPage(loading);
    // 发送请求；在 orders_response 分支中构建/展示内嵌页面
    if (!socket) return;
    // 500ms 节流，避免快速切换造成请求风暴
    qint64 now = monotonic.elapsed();
    if (now - lastOrdersReqMs > 500) {
        lastOrdersReqMs = now;
        auto st = socket->state();
        if (st == QAbstractSocket::ConnectedState) {
            // 若刚刚触发了 chat_init，等其响应回来后再发 get_orders，避免紧邻并发
            if (this->property("awaitingChatInit").toBool()) {
                qInfo() << "delay get_orders due to awaitingChatInit";
                QTimer::singleShot(120, this, [this]{ if (activeTabKey==QLatin1String("orders")) showOrdersView(); });
                return;
            }
            QJsonObject req; req["type"] = "get_orders"; if (!currentUsername.isEmpty()) req["username"] = currentUsername; req["origin"] = QStringLiteral("orders"); QJsonDocument d(req); QByteArray p = d.toJson(QJsonDocument::Compact); p.append('\n');
            qInfo() << "send get_orders (orders tab)";
            enqueueFrame(p);
        } else {
            qWarning() << "skip get_orders due to socket state" << st;
            // 延迟一次再试，避免丢请求
            QTimer::singleShot(300, this, [this]{ if (activeTabKey==QLatin1String("orders")) showOrdersView(); });
        }
    }
    statusBar()->showMessage(tr("正在获取历史订单…"), 2000);
}

void MainWindow::showChatView()
{
    // 离开商城页，退出搜索态
    searchActive = false;
    currentSearchKeyword.clear();
    activeTabKey = QStringLiteral("chat");
    setTabActive("chat");
    setSearchBarVisible(false);
    static qint64 lastChatInitMs = 0;
    if (auto greet = findChild<QLabel*>("greetingLabel")) greet->setVisible(false);
    // 若已存在 chatPage，直接复用；ChatWindow 采用单例持有，避免 page 删除后异步回调访问已销毁控件
    if (!chat) chat = new ChatWindow(this);
    if (auto exist = findChild<QWidget*>("chatPage")) {
        // 已有页面：仅在首次嵌入时设置父子关系/窗口标志，避免频繁 reparent 引发不稳定
        if (chat->parent() != exist) {
            chat->setParent(exist);
        }
        if (!chat->property("embedded").toBool()) {
            chat->setWindowFlags(Qt::Widget);
            chat->setMinimumSize(0,300);
            chat->setProperty("embedded", true);
        }
        if (socket) chat->setSocket(socket);
    // 统一发送路径：让 ChatWindow 使用 MainWindow 的发送队列
    chat->setSendFunc([this](const QByteArray &f){ this->enqueueFrame(f); });
        if (!currentUsername.isEmpty()) chat->setUsername(currentUsername);
        // 串行化 chat_init 与后续请求：收到 chat_init_request 时立即发送，并在 chat_init_response 到达前，延迟 orders 等请求
        QObject::disconnect(chat, nullptr, this, nullptr);
        connect(chat, &ChatWindow::chatInitRequested, this, [this](const QByteArray &p){
            // 直接通过统一队列发送 chat_init，并标记等待 chat_init_response
            this->enqueueFrame(p);
            this->setProperty("awaitingChatInit", true);
            // 设置一个 500ms 的软超时，超时后允许继续请求（防止卡住）
            QTimer::singleShot(500, this, [this]{ this->setProperty("awaitingChatInit", false); });
        });
    // 500ms 节流初始化
    qint64 now = monotonic.elapsed();
    if (now - lastChatInitMs > 500) { lastChatInitMs = now; chat->initChat(); }
        clearToFullPage(exist);
        return;
    }
    // 注入网络依赖与当前用户名
    if (socket) chat->setSocket(socket);
    chat->setSendFunc([this](const QByteArray &f){ this->enqueueFrame(f); });
    QObject::disconnect(chat, nullptr, this, nullptr);
    connect(chat, &ChatWindow::chatInitRequested, this, [this](const QByteArray &p){
        this->enqueueFrame(p);
        this->setProperty("awaitingChatInit", true);
        QTimer::singleShot(500, this, [this]{ this->setProperty("awaitingChatInit", false); });
    });
    if (!currentUsername.isEmpty()) chat->setUsername(currentUsername);
    QWidget *page = new QWidget(this);
    page->setObjectName("chatPage");
    auto *v = new QVBoxLayout(page);
    auto *topBar = new QHBoxLayout();
    auto *title = new QLabel(tr("客服聊天"), page); title->setStyleSheet("font-weight:600;font-size:16px;");
    auto *back = new QPushButton(tr("返回"), page);
    topBar->addWidget(title); topBar->addStretch(1); topBar->addWidget(back); v->addLayout(topBar);
    chat->setParent(page);
    chat->setWindowFlags(Qt::Widget);
    chat->setMinimumSize(0,300);
    chat->setProperty("embedded", true);
    v->addWidget(chat);
    connect(back, &QPushButton::clicked, this, [this, p=QPointer<QWidget>(page)]{
        if (p) p->hide();
        // 保留 ChatWindow 以便下次快速进入
        if (lastNonCartView==ViewMode::Mall) showMallView(); else showHomeView();
    });
    clearToFullPage(page);
    // 进入聊天页时，拉取历史并刷新在线用户（首次必发，随后 400ms 节流）
    qint64 now = monotonic.elapsed();
    if (lastChatInitMs == 0 || (now - lastChatInitMs) > 400) { lastChatInitMs = now; chat->initChat(); }
}

// 顶部搜索栏显隐：仅在首页/发现好物显示
void MainWindow::setSearchBarVisible(bool visible)
{
    if (auto w = findChild<QWidget*>("searchInput")) w->setVisible(visible);
    if (auto w = findChild<QWidget*>("searchButton")) w->setVisible(visible);
}
// 加载网络图片（带简单内存缓存），按目标尺寸等比缩放后设置到 QLabel
void MainWindow::setImageFromUrl(const QString &url, QLabel *label, const QSize &targetSize)
{
    if (!label) return;
    // 归一化：将相对路径或以 /images 开头的路径补齐为完整 URL
    const QString fullUrl = resolveHttpUrl(url);
    // 命中缓存
    if (imageCache.contains(fullUrl)) {
        label->setPixmap(scaledAspect(imageCache.value(fullUrl), targetSize));
        return;
    }
    QUrl qurl(fullUrl);
    if (!qurl.isValid() || qurl.scheme().isEmpty()) return;
    QNetworkRequest req(qurl);
    auto *reply = http->get(req);
    // 以 label 作为上下文对象，若 label 已销毁则此回调不会被触发，避免悬空指针
    connect(reply, &QNetworkReply::finished, label, [this, reply, fullUrl, targetSize, label]{
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Image load failed" << fullUrl << ":" << reply->errorString();
            if (label) label->setText(tr("图片加载失败"));
            return;
        }
        const QByteArray bytes = reply->readAll();
        QPixmap pm; if (!pm.loadFromData(bytes)) return;
        imageCache.insert(fullUrl, pm);
        if (label) label->setPixmap(scaledAspect(pm, targetSize));
    });
}

QPixmap MainWindow::scaledAspect(const QPixmap &src, const QSize &target)
{
    if (src.isNull() || !target.isValid()) return src;
    return src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// 等比扩展并居中裁剪（类似 CSS cover）
QPixmap MainWindow::scaledCover(const QPixmap &src, const QSize &target)
{
    if (src.isNull() || !target.isValid()) return src;
    // 按目标高度等比缩放，保证纵向完整显示；若宽度超出则仅左右裁剪
    const int tgtH = target.height();
    if (tgtH <= 0) return src;
    const qreal scale = static_cast<qreal>(tgtH) / qMax(1, src.height());
    const int newW = qMax(1, static_cast<int>(qRound(src.width() * scale)));
    const int newH = qMax(1, tgtH);
    QPixmap scaled = src.scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (newW <= target.width()) {
        // 宽度不足目标宽，直接返回（由 QLabel 居中对齐留白）
        return scaled;
    }
    // 宽度大于目标宽，水平居中裁剪，纵向不裁剪
    const int x = (newW - target.width()) / 2;
    return scaled.copy(x, 0, target.width(), newH);
}

QString MainWindow::resolveHttpUrl(const QString &url) const
{
    QString u = url.trimmed();
    if (u.isEmpty()) return u;
    // 已经是 http/https，直接返回
    if (u.startsWith("http://", Qt::CaseInsensitive) || u.startsWith("https://", Qt::CaseInsensitive)) return u;
    // 以 // 开头的协议相对 URL，补 http:
    if (u.startsWith("//")) return QStringLiteral("http:%1").arg(u);
    // 以 /images 或 images 开头的相对路径，拼上 httpBase
    if (u.startsWith("/")) {
        return httpBase.isEmpty() ? (QStringLiteral("http://localhost:8081") + u) : (httpBase + u);
    }
    if (u.startsWith("images/", Qt::CaseInsensitive)) {
        return httpBase.isEmpty() ? (QStringLiteral("http://localhost:8081/") + u) : (httpBase + "/" + u);
    }
    // 其他相对路径，按 images/ 子目录处理
    return httpBase.isEmpty() ? (QStringLiteral("http://localhost:8081/images/") + u)
                              : (httpBase + "/images/" + u);
}

// 根据当前窗口宽度，等比缩放本地图片并显示在首页问候语下方
void MainWindow::applyHomeHeaderImage()
{
    if (!homeHeaderImage) return;
    if (homeHeaderImagePath.isEmpty()) { homeHeaderImage->clear(); return; }
    // 如果原始图未加载或上一次失败，尝试从本地路径读取
    if (homeHeaderOriginal.isNull()) {
        QPixmap pm(QString::fromUtf8(homeHeaderImagePath.toUtf8()));
        if (!pm.isNull()) {
            homeHeaderOriginal = pm;
        } else {
            homeHeaderImage->setText(tr("无法加载图片"));
            return;
        }
    }
    // 计算目标尺寸：尽量铺满右侧内容区宽度，限制最大高度以避免过高
    int contentW = width();
    if (auto central = ui->centralwidget) contentW = central->width();
    // 右侧内容区大致是整体宽度减去左侧栏宽度（约 160-220px）与边距
    int targetW = qMax(200, contentW - 220);
    const int maxH = 300; // 提高首页头图最大高度，减少留白
    QPixmap scaled = scaledAspect(homeHeaderOriginal, QSize(targetW, maxH));
    homeHeaderImage->setPixmap(scaled);
    homeHeaderImage->setFixedHeight(scaled.height());
}

// 将当前索引的原图按 carouselArea 宽度等比缩放并显示
void MainWindow::refreshCarouselPixmap()
{
    if (!carouselImageLabel) return;
    if (carouselOriginals.isEmpty()) { carouselImageLabel->clear(); return; }
    const QPixmap &orig = carouselOriginals.at(qBound(0, carouselIndex, carouselOriginals.size()-1));
    if (orig.isNull()) { carouselImageLabel->setText(tr("图片不可用")); return; }
    int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
    int contentH = ui->carouselArea ? ui->carouselArea->height() : height();
    // 改为“铺满并居中裁剪”，消除左右留白；目标宽度与布局权重（约80%）一致
    int targetW = qMax(320, (contentW - 40) * 80 / 100);
    int maxH = qMax(220, (contentH > 0 ? (contentH * 72 / 100) : 360));
    QPixmap covered = scaledCover(orig, QSize(targetW, maxH));
    carouselImageLabel->setPixmap(covered);
    carouselImageLabel->setFixedWidth(targetW);
    carouselImageLabel->setFixedHeight(covered.height());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 左右侧预览点击
    if ((obj == carouselPrevLabel || obj == carouselNextLabel) && event->type() == QEvent::MouseButtonRelease) {
        if (carouselOriginals.isEmpty()) return false;
        if (obj == carouselPrevLabel) {
            carouselIndex = (carouselIndex - 1 + carouselOriginals.size()) % carouselOriginals.size();
        } else {
            carouselIndex = (carouselIndex + 1) % carouselOriginals.size();
        }
        // 刷新
        refreshCarouselPixmap();
        if (carouselPrevLabel && carouselNextLabel && !carouselOriginals.isEmpty()) {
            int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
            int sideW = qMax(110, (contentW - 40) * 10 / 100);
            int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
            const int maxSideH = qMax(110, mainH * 75 / 100);
            auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
            const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
            const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
            carouselPrevLabel->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
            carouselNextLabel->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
            carouselPrevLabel->setFixedWidth(sideW);
            carouselNextLabel->setFixedWidth(sideW);
            carouselPrevLabel->setFixedHeight(carouselPrevLabel->pixmap(Qt::ReturnByValue).height());
            carouselNextLabel->setFixedHeight(carouselNextLabel->pixmap(Qt::ReturnByValue).height());
        }
        updateCarouselDots();
        if (carouselTimer) { carouselTimer->stop(); carouselTimer->start(); }
        return true;
    }
    // 底部小圆点点击
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto *lbl = qobject_cast<QLabel*>(obj)) {
            bool ok = false; int idx = lbl->property("dotIndex").toInt(&ok);
            if (ok && idx >= 0 && idx < carouselOriginals.size()) {
                carouselIndex = idx;
                refreshCarouselPixmap();
                if (carouselPrevLabel && carouselNextLabel && !carouselOriginals.isEmpty()) {
                    int contentW = ui->carouselArea ? ui->carouselArea->width() : width();
                    int sideW = qMax(110, (contentW - 40) * 10 / 100);
                    int mainH = carouselImageLabel->pixmap(Qt::ReturnByValue).height();
                    const int maxSideH = qMax(110, mainH * 75 / 100);
                    auto idxWrap = [this](int i){ int n = carouselOriginals.size(); return (n==0)?0:((i%n)+n)%n; };
                    const QPixmap &p = carouselOriginals.at(idxWrap(carouselIndex-1));
                    const QPixmap &n = carouselOriginals.at(idxWrap(carouselIndex+1));
                    carouselPrevLabel->setPixmap(scaledAspect(p, QSize(sideW, maxSideH)));
                    carouselNextLabel->setPixmap(scaledAspect(n, QSize(sideW, maxSideH)));
                    carouselPrevLabel->setFixedWidth(sideW);
                    carouselNextLabel->setFixedWidth(sideW);
                    carouselPrevLabel->setFixedHeight(carouselPrevLabel->pixmap(Qt::ReturnByValue).height());
                    carouselNextLabel->setFixedHeight(carouselNextLabel->pixmap(Qt::ReturnByValue).height());
                }
                updateCarouselDots();
                if (carouselTimer) { carouselTimer->stop(); carouselTimer->start(); }
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::updateCarouselDots()
{
    for (int i=0; i<carouselDots.size(); ++i) {
        QLabel *d = carouselDots[i];
        if (!d) continue;
        if (i == carouselIndex) d->setStyleSheet("border-radius:5px;background:#1677ff;");
        else d->setStyleSheet("border-radius:5px;background:#ddd;");
    }
}