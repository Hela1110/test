-- 创建数据库
CREATE DATABASE IF NOT EXISTS shopping_system;
USE shopping_system;

-- 创建客户表
CREATE TABLE IF NOT EXISTS clients (
    client_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(100) NOT NULL,
    avatar VARCHAR(255),
    email VARCHAR(100),
    phone VARCHAR(20),
    purchase_count INT DEFAULT 0
);

-- 创建商品表
CREATE TABLE IF NOT EXISTS products (
    product_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2) NOT NULL,
    description TEXT,
    image_url VARCHAR(255),
    stock INT DEFAULT 0,
    on_sale BOOLEAN DEFAULT FALSE,
    discount_price DECIMAL(10,2),
    sales INT NOT NULL DEFAULT 0
);

-- 创建商品类型表
CREATE TABLE IF NOT EXISTS product_types (
    type_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    product_id BIGINT,
    type_name VARCHAR(50) NOT NULL,
    value VARCHAR(100) NOT NULL,
    FOREIGN KEY (product_id) REFERENCES products(product_id) ON DELETE CASCADE
);

-- 创建订单表
CREATE TABLE IF NOT EXISTS orders (
    order_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    client_id BIGINT,
    product_id BIGINT,
    quantity INT NOT NULL,
    total_price DECIMAL(10,2) NOT NULL,
    order_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(20) DEFAULT 'CART',
    FOREIGN KEY (client_id) REFERENCES clients(client_id) ON DELETE SET NULL,
    FOREIGN KEY (product_id) REFERENCES products(product_id) ON DELETE SET NULL
);

-- 添加索引
CREATE INDEX idx_client_username ON clients(username);
CREATE INDEX idx_product_name ON products(name);
CREATE INDEX idx_product_sales ON products(sales);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_orders_client ON orders(client_id);
CREATE INDEX idx_product_types_product ON product_types(product_id);