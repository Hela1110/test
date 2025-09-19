# C++ 代码规范

## 1. 命名规范

### 1.1 文件命名
- 头文件：`.h`，小写，下划线分隔
- 源文件：`.cpp`，与头文件名称对应
- 例如：`login_window.h`, `login_window.cpp`

### 1.2 类名
- 使用 PascalCase（大驼峰）
- 名词或名词短语
- 例如：`LoginWindow`, `ShoppingCart`

### 1.3 方法名
- 使用 camelCase（小驼峰）
- 动词或动词短语
- 例如：`connectToServer()`, `processResponse()`

### 1.4 变量名
- 成员变量：使用 m_ 前缀
- 静态变量：使用 s_ 前缀
- 普通变量：camelCase
- 例如：
```cpp
class Example {
private:
    int m_count;
    static int s_instanceCount;
    int localVariable;
};
```

### 1.5 常量
- 使用全大写，下划线分隔
- 例如：`const int MAX_RETRY_COUNT = 3;`

## 2. 代码格式

### 2.1 头文件结构
```cpp
#ifndef CLASS_NAME_H
#define CLASS_NAME_H

// 包含必要的头文件
#include <QString>

// 前向声明
class DependencyClass;

class ClassName {
public:
    // 构造函数和析构函数
    ClassName();
    virtual ~ClassName();

    // 公共方法
    void publicMethod();

protected:
    // 保护成员
    virtual void protectedMethod();

private:
    // 私有方法
    void privateMethod();

    // 私有成员变量
    int m_variable;
};

#endif // CLASS_NAME_H
```

### 2.2 源文件结构
```cpp
#include "class_name.h"
#include <QDebug>

ClassName::ClassName() {
    // 初始化
}

ClassName::~ClassName() {
    // 清理
}

void ClassName::publicMethod() {
    // 实现
}
```

### 2.3 缩进和格式
```cpp
if (condition) {
    // 使用4个空格缩进
    doSomething();
} else {
    doSomethingElse();
}

// 长行换行
void longFunctionName(const QString& param1,
                     const QString& param2,
                     const QString& param3);
```

## 3. Qt 特定规范

### 3.1 信号和槽
```cpp
class Widget : public QWidget {
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);

signals:
    void dataChanged(const QString& data);

private slots:
    void onButtonClicked();
    void onDataReceived(const QByteArray& data);

private:
    QPushButton* m_button;
};
```

### 3.2 属性声明
```cpp
class Example : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

public:
    QString name() const { return m_name; }
    void setName(const QString& name);

signals:
    void nameChanged(const QString& name);

private:
    QString m_name;
};
```

### 3.3 资源使用
```cpp
// 在 .qrc 文件中定义资源
<RCC>
    <qresource prefix="/images">
        <file>background.png</file>
    </qresource>
</RCC>

// 在代码中使用
QPixmap pixmap(":/images/background.png");
```

## 4. 最佳实践

### 4.1 内存管理
```cpp
// 使用智能指针
#include <memory>
std::unique_ptr<Class> ptr(new Class());

// Qt 父子对象管理
QWidget* widget = new QWidget(this); // 自动删除
```

### 4.2 错误处理
```cpp
// 使用异常处理
try {
    riskyOperation();
} catch (const std::exception& e) {
    qDebug() << "Error:" << e.what();
}

// 使用返回值检查
bool result = operation();
if (!result) {
    handleError();
    return;
}
```

### 4.3 字符串处理
```cpp
// 使用 QString
QString str = "Hello";
str.append(" World");

// 字符串格式化
QString result = QString("Count: %1").arg(count);
```

### 4.4 事件处理
```cpp
class Widget : public QWidget {
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
};
```

## 5. 项目结构
```
client/
├── include/
│   ├── login/
│   │   └── login_window.h
│   ├── mainwindow/
│   │   └── main_window.h
│   └── util/
│       └── constants.h
├── src/
│   ├── login/
│   │   └── login_window.cpp
│   ├── mainwindow/
│   │   └── main_window.cpp
│   └── main.cpp
├── resources/
│   ├── images/
│   ├── styles/
│   └── resources.qrc
└── ui/
    ├── login_window.ui
    └── main_window.ui
```

## 6. 注释规范

### 6.1 文件头注释
```cpp
/**
 * @file login_window.h
 * @brief 登录窗口类的声明
 * @author 作者名
 * @date 2025-09-18
 */
```

### 6.2 类注释
```cpp
/**
 * @brief 登录窗口类
 * 
 * 处理用户登录相关的UI和业务逻辑
 */
class LoginWindow : public QMainWindow {
    // ...
};
```

### 6.3 方法注释
```cpp
/**
 * @brief 处理登录请求
 * @param username 用户名
 * @param password 密码
 * @return 登录是否成功
 */
bool processLogin(const QString& username, const QString& password);
```