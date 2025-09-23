# 客户端-服务器购物系统

基于Qt和Spring Boot的购物系统，实现了完整的购物流程、用户管理、商品管理等功能。

## 项目架构

- 客户端：基于Qt 6开发的C++桌面应用
- 服务端：基于Spring Boot的Java服务器应用
- 数据库：MySQL关系型数据库

## 主要功能

### 客户端功能
- 用户注册和登录
- 商品浏览和搜索
- 购物车管理
- 订单管理
- 用户信息管理
- 库存管理
- 数据统计分析
- JDK 11+ (服务端)
- MySQL 8.0+

## 启动方式

- 一键启动（推荐）：双击工作区根目录下的 `StartAll.bat`
	- 脚本会：
		- 启动服务端（Spring Boot + Netty），等待 Socket 端口 8080 准备
		- 启动客户端（Qt6 Widgets）
		- 打印“Launching client: …\shopping_client.exe”，方便确认启动的是最新构建

## 日志位置

- 客户端会在可执行文件同目录写入 `client.log`
- 遇到弹窗/点击/网络响应等关键节点会写日志，便于排查是否发生重复点击或重复弹窗

## API 对齐与兼容说明

- 本项目按 `docs/api.md` 对齐了以下消息：
	- 获取商品列表：`get_products` → 响应 `products_response { total, products[] }`
	- 搜索商品：`search_products` → 响应同 `products_response`
	- 创建订单：`create_order` → 响应 `order_response { success, orderId, message, code? }`
- 为兼容旧客户端，服务端会同时返回历史消息：
	- 购物车：`get_cart` 会发送 `cart_items`（旧）与 `cart_response`（新）两条
	- 结算：`checkout` 会发送 `checkout_response`（旧）与 `order_response`（新）两条
- 客户端默认使用新协议：
	- 搜索改用 `search_products`
	- 结算走 `create_order`（避免旧的 `checkout` 与 `order_response` 并存导致双提示）

## 分页与下单

- 分页
	- 顶部增加了“上一页/下一页”按钮，对应 `get_products { page, size }`
	- 状态栏会显示“第 X/Y 页（共 N 条）”
- 下单
	- 购物车“结算”使用 `create_order`，将购物车中的 `productId/quantity` 作为 `items` 提交
	- 成功只提示一次“下单成功（含订单号）”；失败根据 `code/message` 提示

## 数量选择与购物车编辑

- 加入购物车前可选择数量：在商品列表/详情点击“加入购物车”时，会弹出数量输入框（默认 1），请求中会携带 `quantity`
- 在购物车中直接编辑数量：数量列可直接双击编辑，修改后会自动发送 `set_cart_quantity { product_id, quantity }` 到服务端；数量为 0 表示从购物车移除
- 结算/下单弹窗会显示本次订单的总金额，金额保留两位小数

## 错误码

- 服务端错误以 `type=error` 返回：`{ type: "error", code, message }`
- 常见错误码：
	- 1001: 认证失败
	- 1002: 参数无效
	- 2001: 商品不存在
	- 2002: 库存不足
	- 3001: 购物车为空
	- 3002: 订单创建失败

客户端会对错误码进行友好提示，并做去重（避免短时间内重复弹窗）。
- Maven 3.6+
- CMake 3.14+

### 编译运行

#### 客户端
```bash
cd client
mkdir build && cd build
cmake ..
make
```

#### 服务端
```bash
cd server
mvn clean install
java -jar target/shopping-server-1.0-SNAPSHOT.jar
```

### 配置数据库
1. 创建数据库：
```sql
CREATE DATABASE shopping_system;
```

2. 执行数据库脚本：
```bash
mysql -u your_username -p shopping_system < database/sql/schema.sql
```

## 开发指南

详细的开发文档请查看：
- [架构设计](docs/架构.md)
- [API文档](docs/api.md)
- [数据库设计](docs/database.md)
- [部署指南](docs/deployment.md)

## 代码规范
- [C++代码规范](docs/cpp-style-guide.md)
- [Java代码规范](docs/java-style-guide.md)

## 贡献指南
1. Fork 本仓库
2. 创建特性分支
3. 提交变更
4. 发起 Pull Request

## 许可证
MIT License