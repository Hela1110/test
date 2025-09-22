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
- 实时客服聊天
- 界面主题切换

### 服务端功能
- 用户认证和授权
- 商品信息管理
- 订单处理
- 库存管理
- 数据统计分析
- 多线程处理
- 实时通信支持

## 快速开始

### 环境要求
- Qt 5.15+ (客户端)
- JDK 11+ (服务端)
- MySQL 8.0+
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