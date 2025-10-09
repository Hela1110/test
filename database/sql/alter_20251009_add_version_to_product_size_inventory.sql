-- 适用于 MySQL 5.6/5.7/8.0：通过 information_schema 判断列是否存在，避免使用 8.0 才支持的 IF NOT EXISTS 语法
-- 使用前确保客户端允许多语句执行；若不允许，可单条复制执行。

-- 1) 判断列是否存在
SET @colExists := (
	SELECT COUNT(*) FROM information_schema.columns
	WHERE table_schema = DATABASE()
		AND table_name = 'product_size_inventory'
		AND column_name = 'version'
);

-- 2) 动态生成 DDL（不存在时才添加）
SET @ddl := IF(@colExists = 0,
	'ALTER TABLE product_size_inventory ADD COLUMN version BIGINT NOT NULL DEFAULT 0;',
	'SELECT 1;'  -- 占位，无操作
);
PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 3) 统一初始化为 0（已有列但为 NULL 的情况）
UPDATE product_size_inventory SET version = 0 WHERE version IS NULL;

-- 可选：校验
-- SELECT product_id,size,stock,version FROM product_size_inventory LIMIT 10;