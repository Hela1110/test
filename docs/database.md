# 数据库设计文档

## 数据库概述

本系统使用 MySQL 8.0+ 作为数据库管理系统，主要包含以下几个核心表：
- clients（用户表）
- products（商品表）
- orders（订单表）
- product_types（商品类型表）

## 表结构设计

### 1. clients 表（用户表）

| 字段名         | 类型         | 约束                     | 说明         |
|---------------|--------------|-------------------------|--------------|
| client_id     | BIGINT      | PRIMARY KEY, AUTO_INCREMENT | 用户ID       |
| username      | VARCHAR(50)  | NOT NULL, UNIQUE       | 用户名        |
| password      | VARCHAR(100) | NOT NULL              | 密码         |
| avatar        | VARCHAR(255) |                       | 头像URL      |
| email         | VARCHAR(100) |                       | 邮箱         |
| phone         | VARCHAR(20)  |                       | 电话         |
| purchase_count| INT         | DEFAULT 0             | 购买次数      |

### 2. products 表（商品表）

| 字段名         | 类型          | 约束                    | 说明         |
|---------------|---------------|------------------------|--------------|
| product_id    | BIGINT       | PRIMARY KEY, AUTO_INCREMENT | 商品ID      |
| name          | VARCHAR(100)  | NOT NULL              | 商品名称      |
| price         | DECIMAL(10,2) | NOT NULL              | 价格         |
| description   | TEXT         |                       | 商品描述      |
| image_url     | VARCHAR(255) |                       | 商品图片URL   |
| stock         | INT          | DEFAULT 0             | 库存量       |
| on_sale       | BOOLEAN      | DEFAULT FALSE         | 是否促销      |
| discount_price| DECIMAL(10,2)|                       | 促销价格      |

### 3. orders 表（订单表）

| 字段名         | 类型          | 约束                    | 说明         |
|---------------|---------------|------------------------|--------------|
| order_id      | BIGINT       | PRIMARY KEY, AUTO_INCREMENT | 订单ID      |
| client_id     | BIGINT       | FOREIGN KEY           | 用户ID       |
| product_id    | BIGINT       | FOREIGN KEY           | 商品ID       |
| quantity      | INT          | NOT NULL              | 购买数量      |
| total_price   | DECIMAL(10,2)| NOT NULL              | 总价         |
| order_time    | DATETIME     | DEFAULT CURRENT_TIMESTAMP | 下单时间   |
| status        | VARCHAR(20)  | DEFAULT 'CART'        | 订单状态      |

### 4. product_types 表（商品类型表）

| 字段名         | 类型         | 约束                    | 说明         |
|---------------|--------------|------------------------|--------------|
| type_id       | BIGINT      | PRIMARY KEY, AUTO_INCREMENT | 类型ID      |
| product_id    | BIGINT      | FOREIGN KEY           | 商品ID       |
| type_name     | VARCHAR(50) | NOT NULL              | 类型名称      |
| value         | VARCHAR(100)| NOT NULL              | 类型值        |

## 索引设计

### clients 表索引
- PRIMARY KEY (client_id)
- UNIQUE INDEX idx_client_username (username)

### products 表索引
- PRIMARY KEY (product_id)
- INDEX idx_product_name (name)

### orders 表索引
- PRIMARY KEY (order_id)
- INDEX idx_orders_status (status)
- INDEX idx_orders_client (client_id)

### product_types 表索引
- PRIMARY KEY (type_id)
- INDEX idx_product_types_product (product_id)

## 表关系

1. orders 表与 clients 表：多对一关系
   - orders.client_id 引用 clients.client_id
   - 级联操作：SET NULL (当用户被删除时，订单记录保留但客户ID置空)

2. orders 表与 products 表：多对一关系
   - orders.product_id 引用 products.product_id
   - 级联操作：SET NULL (当商品被删除时，订单记录保留但商品ID置空)

3. product_types 表与 products 表：多对一关系
   - product_types.product_id 引用 products.product_id
   - 级联操作：CASCADE (当商品被删除时，相关的类型记录也被删除)

## 数据库维护建议

1. 定期备份
   ```sql
   mysqldump -u username -p shopping_system > backup.sql
   ```

2. 优化建议
   - 定期更新统计信息
   - 适时清理过期数据
   - 监控索引使用情况

3. 安全建议
   - 定期修改数据库密码
   - 限制数据库远程访问
   - 实施最小权限原则