-- 创建/迁移：为鞋类商品提供尺码库存，以及购物车/订单明细携带尺码字段
-- 兼容 MySQL 5.7+/8.0，尽量幂等

START TRANSACTION;

-- 1) product_size_inventory 表（product_id + size 唯一）
CREATE TABLE IF NOT EXISTS `product_size_inventory` (
  `id` BIGINT NOT NULL AUTO_INCREMENT,
  `product_id` BIGINT NOT NULL,
  `size` INT NOT NULL,
  `stock` INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_product_size` (`product_id`,`size`),
  KEY `idx_psi_product` (`product_id`),
  CONSTRAINT `fk_psi_product` FOREIGN KEY (`product_id`) REFERENCES `products` (`product_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2) cart_items 增加 size 列，并更新唯一约束为 (client_id, product_id, size)
-- 2.1 若无 size 列则添加（可为空，表示无尺码商品）
SET @has_size := (
  SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS 
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cart_items' AND COLUMN_NAME = 'size'
);
SET @sql := IF(@has_size = 0, 'ALTER TABLE `cart_items` ADD COLUMN `size` INT NULL AFTER `product_id`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 2.2 删除旧的唯一约束（如果存在），再添加新唯一约束
-- 不同环境唯一约束名称可能不同，先尝试删除几个常见名称
SET @drop1 := (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cart_items' AND CONSTRAINT_TYPE = 'UNIQUE' AND CONSTRAINT_NAME = 'UK_CLIENT_PRODUCT');
SET @sql := IF(@drop1>0, 'ALTER TABLE `cart_items` DROP INDEX `UK_CLIENT_PRODUCT`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @drop2 := (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cart_items' AND INDEX_NAME = 'client_id');
-- 一些环境可能人为创建了 client_id/product_id 组合唯一索引
-- 尝试判断是否就是唯一且仅包含这两列；若是则删除
-- 这里简化处理：若存在名为 uk_cart_items 或 client_product 唯一索引则删除
SET @drop3 := (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cart_items' AND CONSTRAINT_TYPE = 'UNIQUE' AND CONSTRAINT_NAME = 'uk_cart_items');
SET @sql := IF(@drop3>0, 'ALTER TABLE `cart_items` DROP INDEX `uk_cart_items`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @drop4 := (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cart_items' AND CONSTRAINT_TYPE = 'UNIQUE' AND CONSTRAINT_NAME = 'client_product');
SET @sql := IF(@drop4>0, 'ALTER TABLE `cart_items` DROP INDEX `client_product`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 添加新的唯一索引（若不存在）
SET @has_new := (
  SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cart_items' AND INDEX_NAME = 'uk_client_product_size'
);
SET @sql := IF(@has_new=0, 'ALTER TABLE `cart_items` ADD UNIQUE KEY `uk_client_product_size` (`client_id`,`product_id`,`size`)', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 3) order_items 增加 size 列（可空，表示无尺码）
SET @has_oisize := (
  SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS 
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'order_items' AND COLUMN_NAME = 'size'
);
SET @sql := IF(@has_oisize = 0, 'ALTER TABLE `order_items` ADD COLUMN `size` INT NULL AFTER `product_id`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

COMMIT;

-- 可选：初始化每个鞋类商品的尺码库存（37~45），默认 0；实际库存由后台填充
-- INSERT 示例（按需要启用并填入具体 product_id 与初始库存）：
-- INSERT INTO product_size_inventory(product_id, size, stock)
-- SELECT p.product_id, s.size, 0 FROM products p CROSS JOIN (
--   SELECT 37 AS size UNION ALL SELECT 38 UNION ALL SELECT 39 UNION ALL SELECT 40 UNION ALL SELECT 41 UNION ALL SELECT 42 UNION ALL SELECT 43 UNION ALL SELECT 44 UNION ALL SELECT 45
-- ) s WHERE p.category = 'shoes';
