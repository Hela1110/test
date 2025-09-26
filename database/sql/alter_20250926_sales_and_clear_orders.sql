-- 增加商品销量字段（兼容不支持 IF NOT EXISTS 的 MySQL 版本）
SET @col_exists := (
  SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS 
  WHERE TABLE_SCHEMA = DATABASE() 
    AND TABLE_NAME = 'products' 
    AND COLUMN_NAME = 'sales'
);
SET @ddl := IF(@col_exists = 0,
  'ALTER TABLE `products` ADD COLUMN `sales` INT NOT NULL DEFAULT 0',
  'SELECT 1'
);
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 清空订单记录（先删从表再删主表；兼容旧表 orders）
SET FOREIGN_KEY_CHECKS = 0;
DELETE FROM order_items;
DELETE FROM order_headers;
DELETE FROM orders;
-- 可选：重置自增
ALTER TABLE order_items AUTO_INCREMENT = 1;
ALTER TABLE order_headers AUTO_INCREMENT = 1;
ALTER TABLE orders AUTO_INCREMENT = 1;
SET FOREIGN_KEY_CHECKS = 1;
