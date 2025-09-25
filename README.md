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

### 一键启动（推荐）

在根目录直接双击 `StartAll.bat`，或在命令行手动传参数：

```
StartAll.bat [httpPort] [socketPort] [--skip-build]
```

示例：

```
StartAll.bat 8081 9090
```

参数说明：

| 参数 | 含义 | 默认 | 备注 |
|------|------|------|------|
| httpPort | Spring Boot `server.port` | 8081 | 避免与本地其它 8080 冲突 |
| socketPort | Netty `socket.port` | 9090 | 客户端应连接此端口做业务通信 |
| --skip-build | 跳过 mvn package | 无 | 仅在确认无源码改动时用于加速 |

脚本流程：
1. (可选) 构建服务端并启动：`java -jar ... --server.port=HTTP --socket.port=SOCKET`
2. 等待两个端口（HTTP + Socket）全部可用（最多 90 秒）
3. 构建并启动客户端（优先非 dist 目录最新构建）
4. 监测服务端早期退出，若窗口关闭将立即提示

> 旧版本仅监听 8080；现在请区分 HTTP 与 Socket 两个端口，客户端网络层要连 `socket.port`。

### 手动启动（开发调试）

1. 修改 `server/src/main/resources/application.yml` 中：
	- `spring.datasource.password: 你的密码` 替换为真实数据库密码
	- 如需修改端口：`server.port` / `socket.port`
2. 确认数据库已存在并建表（`ddl-auto: none` 不会自动建表）
3. 服务端：
	```bash
	mvn -q -DskipTests clean package
	java -jar target/shopping-server-1.0-SNAPSHOT.jar --server.port=8081 --socket.port=9090
	```
4. 客户端：
	```bash
	cd client
	mkdir build && cd build
	cmake .. && cmake --build . --config Release
	./shopping_client.exe
	```
5. 确保客户端配置的连接端口与上述 `--socket.port` 一致。

### 常见启动故障排查

| 现象 | 可能原因 | 解决 | 查看位置 |
|------|----------|------|----------|
| OrderHeaderRepository bean not found | 实际是上下文在创建仓库前因数据库连接失败退出 | 检查数据库账号/密码/库是否存在；加 `--debug` 观察 | 服务端窗口 |
| 90 秒仍未就绪 | 某端口被占用或进程已崩溃 | 换端口 / 释放占用；确认 server 窗口仍在且无异常 | StartAll 输出 + server 日志 |
| 客户端请求无响应 | 误连到 HTTP 端口 | 改连 `socket.port` (默认 9090) | 客户端 `client.log` |
| 下单库存异常 | 人工修改表或并发测试 | 校验库存字段并重试；观察事务日志 | 服务端日志 |
| 中文编码错乱 | 终端编码/数据库字符集 | 确保 `characterEncoding=utf8` & 控制台 UTF-8 | application.yml + 控制台 |

> 仍无法定位时，请提供服务端启动完整日志 (首尾 200 行) 与应用版本号。

## 日志位置

- 客户端：同目录 `client.log`（交互、协议、错误）
- 服务端：控制台；必要时可在 `application.yml` 调整 `logging.level` 或启动参数添加 `--debug`

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

	## 数据库重构后的 Socket 协议要点（2025 重构）

	本轮重构将原先内存级 CATALOG / carts / orders 全部迁移到 MySQL，核心变化如下：

	1. 购物车持久化
		- 以 `order_headers(status=CART)` + `order_items` 作为单一活动购物车；同一用户仅存在一个 CART header。
		- 响应中新增 `headerId` 字段，客户端可在后续操作（checkout 等）中携带，减少用户名耦合。

	2. 双消息兼容策略保持
		- `get_cart` / 修改类指令仍发送 `cart_items` 与 `cart_response` 两条，新增字段 `headerId`。
		- `checkout` 返回 `checkout_response` 与 `order_response`；推荐客户端仅消费 `order_response`。

	3. 商品列表/搜索分页
		- 使用数据库分页：`get_products { page, size }` 基于 Spring Data Page，从 1 开始的 page 转为内部 0-based。
		- `search_products` 使用名称模糊匹配（忽略大小写）。

	4. 库存与下单
		- 库存校验 & 扣减在事务中完成；库存不足抛出并映射为 `code=2002`。
		- 空购物车结算返回 `code=3001`。

	5. 已弃用的内存结构
		- 代码中残留的 `@Deprecated` CATALOG / carts / orders 仅为兼容旧方法引用，不再写入真实业务数据。

	6. 协议新增/调整字段
		- 所有购物车相关响应附带 `headerId`。
		- add_to_cart_response 增加 `headerId`。

	7. 迁移影响
		- 客户端不需要修改已有解析即可兼容（忽略新字段安全）。
		- 若要利用精确结算，可在 `checkout` 前显示 `headerId` 绑定确认。

	> 若后续计划加入 Token 鉴权，可使用登录后发放的 token 替代每条消息包含 username。


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

## 构建环境要求

- JDK 11+
- Maven 3.6+
- Qt 6 + CMake 3.14+
- MySQL 8.x

## 初始化数据库

1. 创建库：
	```sql
	CREATE DATABASE shopping_system CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
	```
2. 导入结构 / 基础数据脚本（若有）：
	```bash
	mysql -u root -p shopping_system < database/sql/schema.sql
	```
3. 修改 `application.yml` 中的账号密码；首次启动请确认表结构已存在（`ddl-auto: none`）。

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