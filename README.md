# 购物系统 - 基于Qt和Spring Boot的电商平台

[![Qt](https://img.shields.io/badge/Qt-6.8.3-green.svg)](https://www.qt.io/)
[![Spring Boot](https://img.shields.io/badge/Spring%20Boot-2.7-brightgreen.svg)](https://spring.io/projects/spring-boot)
[![Java](https://img.shields.io/badge/Java-11+-orange.svg)](https://www.oracle.com/java/)
[![MySQL](https://img.shields.io/badge/MySQL-8.0-blue.svg)](https://www.mysql.com/)

## 📌 项目简介

本项目是一个功能完善的**桌面端电商系统**,采用现代化的C/S架构设计,实现了商品浏览、购物车管理、订单处理、数据统计分析等完整的电商业务流程。系统具有良好的用户体验和稳定的性能表现。

**技术亮点**:
- ✅ **跨语言架构**: C++客户端 + Java服务器,充分发挥各语言优势
- ✅ **智能推荐系统**: 销量最高+新品+折扣商品智能组合
- ✅ **实时通信**: 基于Netty的高性能Socket长连接
- ✅ **数据可视化**: QtCharts饼状图、柱状图展示销售数据
- ✅ **事务一致性**: Spring事务管理保证订单和库存的原子操作
- ✅ **响应式UI**: 现代化界面设计,支持emoji图标美化

## 🏗️ 系统架构

### 整体架构图
```
┌──────────────────┐      Socket(9090)      ┌───────────────────┐
│   Qt客户端        │◄─────JSON消息─────────►│  Spring Boot      │
│   (C++/Qt6)      │                         │  服务器(Java)     │
│                  │                         │                   │
│ • 商品浏览        │                         │ • 业务处理        │
│ • 购物车管理      │                         │ • Spring Data JPA │
│ • 订单管理        │                         │ • Netty Server    │
│ • 数据统计(Charts)│                         │ • 智能推荐算法    │
└──────────────────┘                         └─────────┬─────────┘
                                                       │ JDBC
                                              ┌────────▼──────────┐
                                              │   MySQL 8.0       │
                                              │   (InnoDB引擎)    │
                                              └───────────────────┘
```

### 技术栈详解
| 层次 | 技术组件 | 版本 | 用途说明 |
|-----|---------|------|---------|
| **前端** | Qt Framework | 6.8.3 | GUI框架,提供丰富UI组件 |
| | C++ | C++17 | 主要开发语言 |
| | CMake | 3.16+ | 跨平台构建系统 |
| | Qt Charts | - | 数据可视化(饼状图/柱状图) |
| | QTcpSocket | Qt Network | TCP通信,与服务器长连接 |
| **后端** | Spring Boot | 2.7.x | 简化Spring开发,自动配置 |
| | Spring Data JPA | 2.7.x | ORM框架,简化数据库操作 |
| | Hibernate | 5.6.x | JPA实现,对象关系映射 |
| | Netty | 4.1.x | 高性能异步网络框架 |
| | Jackson | 2.13.x | JSON序列化/反序列化 |
| | Maven | 3.6+ | 项目构建和依赖管理 |
| **数据库** | MySQL | 8.0+ | 关系型数据库 |
| | InnoDB | - | 支持事务、外键、行级锁 |
| **通信** | Socket端口 | 9090 | 主要通信协议(JSON格式) |
| | HTTP端口 | 8081 | RESTful API(备用) |

## ⚡ 主要功能

### 用户端功能
- ✅ **用户认证**: 注册、登录、密码加密、权限区分(普通用户/管理员)
- ✅ **商品浏览**: 
  - 首页推荐(热卖🔥/新品/折扣商品智能推荐)
  - 商品列表分页展示(20条/页,上一页/下一页按钮)
  - 多维度排序(销量优先/价格升序/价格降序/打折优先)
  - 关键词搜索(名称模糊匹配)
  - 商品详情展示(图片/价格/库存/描述)
- ✅ **购物车管理**: 
  - 添加商品(支持选择数量)
  - 修改数量(直接编辑或删除)
  - 删除商品
  - 实时价格计算(保留2位小数)
  - 购物车持久化(数据库存储)
- ✅ **订单管理**: 
  - 一键下单
  - 订单列表分页展示
  - 多状态筛选(待支付/待发货/已完成/已取消)
  - 订单详情查看(订单号/时间/商品明细/总价)
  - 订单搜索(订单号/商品名)
- ✅ **个人中心**: 
  - 账户信息展示(ID/用户名/手机号/邮箱)
  - 信息修改(用户名/手机号/邮箱/密码)
  - 实时更新保存
- ✅ **客服聊天**: 
  - 实时消息推送
  - 历史消息加载
  - 消息时间戳
  - 未读消息提示

### 管理员端功能
- ✅ **数据统计**: 
  - 销售额统计(柱状图,按月份展示)
  - 商品销售占比(饼状图)
  - 尺码销量统计
  - 点击图表查看商品详情(新增: 价格/库存/销量字段对齐)
  - 日期范围筛选
- ✅ **用户管理**: 
  - 用户列表分页浏览
  - 搜索用户(用户名/ID)
  - 启用/禁用用户(禁用后无法登录)
  - 查看用户详情
- ✅ **库存管理**: 
  - 商品库存查询
  - 低库存预警
  - 库存变动日志(销售/入库/调整)
- ✅ **数据导出**: 
  - CSV格式导出(UTF-8 BOM编码)
  - 适配Windows Excel直接打开

### 最新特性 (2025-10)
- 🔥 **热卖商品智能标记**: 首页自动展示销量最高商品,醒目"热卖🔥"橙色角标
- 📊 **饼状图商品详情增强**: 点击图表扇区显示完整商品信息(price/discountPrice/stock/imageUrl/sales)
- 🎯 **三级优先级角标**: 售罄(红色) > 热卖(橙色) > 新品(绿色)
- 🛡️ **用户禁用功能**: 管理员可禁用用户,被禁用用户无法通过Socket/REST登录
- 📷 **图片加载优化**: 支持HTTP/HTTPS/CDN图片URL,自动下载显示
- 💾 **购物车持久化**: 购物车数据存储到数据库(order_headers.status=CART)
- 🔄 **智能推荐算法**: 4商品组合(销量最高+新品+2个打折商品+兜底策略)

## 🚀 快速开始

### 环境要求
- **操作系统**: Windows 10/11
- **客户端**: Qt 6.8.3 + MinGW 13.1.0
- **服务端**: JDK 11+ + Maven 3.6+
- **数据库**: MySQL 8.0+

### 数据库初始化
```bash
# 1. 创建数据库
CREATE DATABASE shopping CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

# 2. 导入表结构
USE shopping;
SOURCE database/sql/schema.sql;

# 3. 导入测试数据
SOURCE database/sql/insert_test_data.sql;

# 4. 执行增量脚本(如需要)
SOURCE database/sql/alter_20250926_add_chat_messages.sql;
SOURCE database/sql/alter_20250927_add_enabled_to_clients.sql;
```

### 一键启动 (推荐)
```bash
# 在项目根目录执行
StartAll.bat 8081
```
此脚本会自动:
1. 启动MySQL服务(如未运行)
2. 编译并启动Spring Boot服务器(端口9090)
3. 编译并启动Qt客户端

### 手动启动

#### 启动服务端
```bash
cd server
mvn clean package -DskipTests
java -jar target/shopping-server-1.0-SNAPSHOT.jar --socket.port=9090
```

#### 启动客户端
```bash
cd client
cmake --preset qt-mingw1310-release
cmake --build build-qt1310 --target shopping_client --config Release -j
.\build-qt1310\shopping_client.exe
```

## 📖 使用文档

### 用户使用流程
1. **注册登录**: 使用用户名和密码注册账号,登录进入主界面
2. **浏览商品**: 查看首页推荐商品(热卖/新品/折扣),或切换到商城浏览全部商品
3. **搜索排序**: 使用搜索框搜索商品,使用排序按钮按销量/价格排序
4. **加入购物车**: 点击"加入购物车"按钮,选择数量,确认添加
5. **查看购物车**: 点击购物车图标,查看已添加商品,可修改数量或删除
6. **下单结算**: 点击"结算"按钮,确认订单信息,完成下单
7. **查看订单**: 切换到订单页面,查看历史订单和订单详情
8. **客服咨询**: 点击聊天图标,与客服实时沟通

### 管理员功能
- 使用管理员账号登录(默认: admin/admin123)
- 查看数据统计图表(销售额/商品销售占比)
- 管理用户(启用/禁用/搜索)
- 查看库存和库存变动日志

## 🔧 配置说明

### 服务端配置 (application.yml)
```yaml
server:
  port: 8081  # HTTP端口(备用)

spring:
  datasource:
    url: jdbc:mysql://localhost:3306/shopping?characterEncoding=utf8mb4
    username: root
    password: your_password
  jpa:
    hibernate:
      ddl-auto: none  # 生产环境设置为none
    show-sql: false   # 是否显示SQL日志

socket:
  port: 9090  # Socket端口
```

### 客户端配置
客户端无需配置文件,首次启动会自动连接到 `localhost:9090`

## 🛠️ 开发指南

### 项目结构
```
shopping-system/
├── client/                 # Qt客户端
│   ├── include/           # 头文件
│   │   ├── chat/
│   │   ├── login/
│   │   ├── mainwindow/
│   │   ├── register/
│   │   └── shopping/
│   ├── src/              # 源代码
│   ├── ui/               # Qt Designer UI文件
│   ├── resources/        # 资源文件(样式表/图片)
│   └── CMakeLists.txt
├── server/                # Spring Boot服务端
│   ├── src/main/java/
│   │   └── com/shopping/server/
│   │       ├── entity/   # JPA实体类
│   │       ├── repository/ # Spring Data Repository
│   │       ├── service/  # 业务逻辑层
│   │       ├── socket/   # Netty Socket服务器
│   │       └── ShoppingServerApplication.java
│   └── pom.xml
├── database/              # 数据库脚本
│   └── sql/
│       ├── schema.sql
│       └── alter_*.sql
├── docs/                  # 文档
│   ├── 项目答辩文档.md
│   ├── 架构.md
│   ├── api.md
│   └── database.md
├── images/                # 商品图片
├── StartAll.bat          # 一键启动脚本
└── README.md
```

### 核心技术实现

#### 前端网络通信 (QTcpSocket)
```cpp
// 发送JSON消息到服务器
void MainWindow::sendRequest(const QJsonObject &request) {
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');  // 消息分隔符
    socket->write(data);
}

// 接收服务器响应
void MainWindow::onReadyRead() {
    while (socket->canReadLine()) {
        QByteArray line = socket->readLine();
        QJsonDocument doc = QJsonDocument::fromJson(line);
        QJsonObject response = doc.object();
        handleResponse(response);
    }
}
```

#### 后端消息处理 (Netty)
```java
@Component
public class SocketMessageHandler extends ChannelInboundHandlerAdapter {
    
    @Autowired
    private ProductRepository productRepository;
    
    @Override
    public void channelRead(ChannelHandlerContext ctx, Object msg) {
        String message = (String) msg;
        JsonNode json = objectMapper.readTree(message);
        String type = json.get("type").asText();
        
        // 根据消息类型分发处理
        switch (type) {
            case "get_products": handleGetProducts(ctx, json); break;
            case "get_recommendations": handleGetRecommendations(ctx); break;
            case "checkout": handleCheckout(ctx, json); break;
            // ...更多消息类型
        }
    }
}
```

#### JPA数据访问 (Spring Data)
```java
@Repository
public interface ProductRepository extends JpaRepository<Product, Long> {
    // 方法命名查询 - 查找销量最高的上架商品
    Product findTopByOnSaleTrueOrderBySalesDesc();
    
    // 自定义JPQL查询 - 搜索商品
    @Query("SELECT p FROM Product p WHERE " +
           "LOWER(p.name) LIKE LOWER(CONCAT('%', :keyword, '%'))")
    List<Product> searchProducts(@Param("keyword") String keyword);
}
```

#### 事务管理 (订单下单)
```java
@Service
@Transactional  // 声明式事务,失败自动回滚
public class OrderService {
    
    public OrderHeader createOrder(Long clientId, List<OrderItemDTO> items) {
        // 1. 校验库存
        validateStock(items);
        
        // 2. 创建订单头
        OrderHeader order = createOrderHeader(clientId);
        
        // 3. 创建订单明细并扣减库存
        for (OrderItemDTO item : items) {
            createOrderItem(order, item);
            deductStock(item.getProductId(), item.getQuantity());
        }
        
        // 4. 更新订单总价
        updateTotalAmount(order);
        
        return order;
    }
}
```

### 通信协议

#### 消息格式
**请求消息**:
```json
{
  "type": "消息类型",
  "param1": "值1",
  "param2": "值2"
}
```

**响应消息**:
```json
{
  "type": "响应类型",
  "code": 状态码,
  "message": "提示信息",
  "data": { /* 业务数据 */ }
}
```

#### 常用消息类型
| 消息类型 | 说明 | 参数 |
|---------|------|------|
| login | 用户登录 | username, password |
| register | 用户注册 | username, password, phone, email |
| get_products | 获取商品列表 | page, size |
| get_recommendations | 获取推荐商品 | - |
| search_products | 搜索商品 | keyword |
| add_to_cart | 添加到购物车 | productId, quantity |
| get_cart | 获取购物车 | - |
| create_order | 创建订单 | items[] |
| get_orders | 获取订单列表 | page, size |
| get_stats | 获取统计数据 | startDate, endDate |
| send_chat_message | 发送聊天消息 | to, content |

#### 错误码
| 错误码 | 说明 |
|-------|------|
| 1001 | 认证失败(用户名或密码错误) |
| 1002 | 参数无效 |
| 1003 | 用户已被禁用 |
| 2001 | 商品不存在 |
| 2002 | 库存不足 |
| 3001 | 购物车为空 |
| 3002 | 订单创建失败 |

## 🔍 故障排查

### 常见问题

#### 1. 客户端无法连接服务器
**症状**: 客户端启动后显示"连接失败"  
**解决方案**:
- 检查服务器是否启动: `netstat -ano | findstr 9090`
- 检查防火墙设置,确保端口9090未被阻止
- 查看服务器日志确认Netty服务器是否成功启动

#### 2. 数据库连接失败
**症状**: 服务器启动报错"Unable to create initial connections of pool"  
**解决方案**:
- 检查MySQL服务是否运行: `net start mysql`
- 确认数据库账号密码正确(application.yml)
- 检查数据库是否已创建: `SHOW DATABASES;`

#### 3. 中文乱码问题
**症状**: 界面或数据库中文显示为乱码  
**解决方案**:
- 数据库使用utf8mb4字符集
- 源码文件保存为UTF-8编码
- MySQL连接URL添加: `characterEncoding=utf8mb4`

#### 4. 购物车数据丢失
**症状**: 重启客户端后购物车为空  
**解决方案**:
- 检查是否使用最新版本(购物车已持久化到数据库)
- 确认用户已登录
- 查看数据库表: `SELECT * FROM order_headers WHERE status='CART';`

#### 5. 图片无法显示
**症状**: 商品图片显示为空白  
**解决方案**:
- 确认imageUrl字段是否为有效URL(http/https开头)
- 检查网络连接
- 查看客户端日志确认下载是否成功

## 📚 相关文档

- [架构设计文档](docs/架构.md)
- [API接口文档](docs/api.md)
- [数据库设计文档](docs/database.md)
- [部署文档](docs/deployment.md)
- [课程设计答辩文档](docs/项目答辩文档.md)

## 👥 开发团队

- **开发者**: 
- **联系方式**: 

## 📄 许可证

MIT License

## 🙏 致谢

感谢以下开源项目:
- [Qt](https://www.qt.io/) - 强大的跨平台GUI框架
- [Spring Boot](https://spring.io/projects/spring-boot) - 简化Spring应用开发
- [Netty](https://netty.io/) - 高性能网络应用框架
- [MySQL](https://www.mysql.com/) - 开源关系型数据库

---

**最后更新**: 2025年10月15日  
**版本**: 1.0
