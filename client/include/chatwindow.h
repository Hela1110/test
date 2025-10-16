#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QElapsedTimer>
#include <QVector>
#include <QByteArray>
#include <functional>

// 前置声明，避免在头文件中包含大量 Qt 头
class QTcpSocket;
class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTimer;
class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;

namespace Ui {
class ChatWindow;
}

class ChatWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();
    void setSocket(QTcpSocket *s) { socket = s; }
    void setUsername(const QString &u) { username = u; }
    // 将发送入口统一到外部（例如 MainWindow）的批处理队列
    void setSendFunc(std::function<void(const QByteArray&)> fn) { sendFn = std::move(fn); }
    void initChat();
    void handleMessage(const QJsonObject &msg);
    void handleMessageUi(const QJsonObject &msg);
    // 是否正在等待订单详情（用于 MainWindow 将 orders_response 兜底路由回聊天窗口）
    bool isWaitingOrderDetail() const;

signals:
    // 用于将 chat_init 的初始化请求交给外部统一串行化发送
    void chatInitRequested(const QByteArray &payload);
    // 窗口大小变化事件（用于自适应控制图标/文本）
    void resizeEventOccurred(const QSize &size);

private slots:
    void on_sendButton_clicked();
    void on_messageInput_returnPressed();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::ChatWindow *ui;
    QTcpSocket *socket = nullptr;
    QString username;
    QLineEdit *peerEdit = nullptr;
    QLabel *onlineLabel = nullptr; // 保留但不再作为主显示
    QComboBox *onlineCombo = nullptr; // 在线用户下拉
    QPushButton *undoDeleteBtn = nullptr; // 撤销删除按钮（短暂显示）
    QTimer *deleteTimer = nullptr;       // 延时删除定时器
    QString pendingDeletePeer;           // 待删除的对话对象
    // 用于图片下载的共享网络管理器
    QNetworkAccessManager *http = nullptr;
    void sendMessage();
    // 如果 message 文本是本地图片路径，则先上传后再发送URL，返回是否已处理
    bool trySendLocalImagePath(const QString &message);
    void sendDelete();
    void appendLine(const QString &text);
    void appendBubble(const QString &from, const QString &to, const QString &content, const QString &ts, bool isSelf);
    void appendSystem(const QString &text);
    // 上传并发送图片（通过 HTTP 上传获取 URL，再经 chat_send 发送）
    void sendImage();
    // 直接从剪贴板读取图片并发送（Ctrl+V 粘贴）
    bool pasteImageFromClipboard();
    // 辅助：将 /images/... 或相对路径转为可访问的绝对 URL（默认 8081）
    QString toAbsoluteUrl(const QString &u) const;
    // 专门的图片气泡渲染（下载 → 缩略 → 嵌入到列表项）
    void appendImageBubble(const QString &from, const QString &to, const QString &url, const QString &ts, bool isSelf);
    // 订单卡片：以卡片样式展示，并支持双击查看详情
    void appendOrderBubble(const QString &from, const QString &to, const QJsonObject &orderObj, const QString &ts, bool isSelf);
    QString buildOrderDetailHtml(const QJsonObject &orderObj) const;
    void showOrderDetailFromJson(const QString &json);
    // 售后申请卡片 & 详情按订单号拉取
    void appendRefundBubble(const QString &from, const QString &to, const QJsonObject &refundObj, const QString &ts, bool isSelf);
    void showOrderDetailById(qlonglong orderId);

    // 等待详情查询时的订单号
    qlonglong pendingDetailOrderId = -1;

    // 轻量节流用单调时钟
    QElapsedTimer monotonic;
    qint64 lastInitChatMs = 0;

    // 发送侧微批处理（聊天窗口内部的发送合并）
    QVector<QByteArray> pendingFrames;
    QTimer *sendFlushTimer = nullptr;
    void enqueueFrame(const QByteArray &frame);
    std::function<void(const QByteArray&)> sendFn;
};

#endif // CHATWINDOW_H