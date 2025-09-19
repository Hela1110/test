# 部署指南

## 环境准备

### 1. 系统要求
- 操作系统：Windows/Linux/MacOS
- CPU：双核及以上
- 内存：4GB及以上
- 硬盘空间：10GB及以上

### 2. 软件要求
- JDK 11+
- MySQL 8.0+
- Maven 3.6+
- Qt 5.15+
- CMake 3.14+

## 服务端部署

### 1. 配置数据库
```bash
# 登录MySQL
mysql -u root -p

# 创建数据库
CREATE DATABASE shopping_system;

# 导入数据库脚本
mysql -u root -p shopping_system < database/sql/schema.sql
```

### 2. 修改配置文件
修改 `server/src/main/resources/application.properties`:
```properties
# 数据库配置
spring.datasource.url=jdbc:mysql://localhost:3306/shopping_system
spring.datasource.username=your_username
spring.datasource.password=your_password

# 服务器配置
server.port=8080
socket.port=8081

# 日志配置
logging.level.root=INFO
logging.file.name=logs/shopping-server.log
```

### 3. 编译打包
```bash
cd server
mvn clean package
```

### 4. 运行服务器
```bash
java -jar target/shopping-server-1.0-SNAPSHOT.jar
```

## 客户端部署

### 1. 编译
```bash
# Windows
cd client
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make

# Linux/MacOS
cd client
mkdir build && cd build
cmake ..
make
```

### 2. 配置
修改 `client/config.ini`:
```ini
[Server]
host=localhost
port=8080
socket_port=8081

[UI]
theme=default
language=zh_CN
```

### 3. 打包发布
```bash
# Windows
windeployqt bin/shopping-client.exe

# Linux
# 使用 linuxdeployqt 或手动复制依赖库
```

## 启动顺序

1. 启动 MySQL 服务
2. 启动服务端应用
3. 启动客户端程序

## 性能优化

### 服务端优化
1. JVM参数调优
```bash
java -Xms2g -Xmx4g -XX:+UseG1GC -jar shopping-server.jar
```

2. 数据库优化
```sql
-- 添加必要的索引
CREATE INDEX idx_product_name ON products(name);
CREATE INDEX idx_order_time ON orders(order_time);
```

### 客户端优化
1. 启用缓存
2. 使用延迟加载
3. 图片资源压缩

## 监控和维护

### 1. 日志监控
- 检查服务器日志：`tail -f logs/shopping-server.log`
- 检查系统日志：`journalctl -u shopping-server.service`

### 2. 性能监控
```bash
# 监控JVM
jstat -gcutil <pid> 1000

# 监控系统资源
top -p <pid>
```

### 3. 备份策略
```bash
# 数据库备份
mysqldump -u root -p shopping_system > backup/shopping_$(date +%Y%m%d).sql

# 配置文件备份
cp -r config/* backup/config_$(date +%Y%m%d)/
```

## 故障处理

### 1. 服务器无响应
1. 检查进程状态：`ps aux | grep java`
2. 检查端口占用：`netstat -tulpn | grep 8080`
3. 检查日志文件
4. 重启服务：`systemctl restart shopping-server`

### 2. 数据库连接问题
1. 检查MySQL服务状态
2. 验证连接参数
3. 检查防火墙设置

### 3. 客户端崩溃
1. 检查日志文件
2. 验证配置文件
3. 检查Qt依赖