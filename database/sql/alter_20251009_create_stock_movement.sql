-- 说明：某些环境 product 表可能尚未创建或名称差异，直接声明外键会报 1824。
-- 处理策略：先创建表（不带外键），再检测 product 表是否存在且未有外键时再添加。

CREATE TABLE IF NOT EXISTS stock_movement (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    product_id BIGINT NOT NULL,
    size INT NOT NULL,
    delta INT NOT NULL,
    action VARCHAR(40) NOT NULL,
    order_id BIGINT NULL,
    operator VARCHAR(64) NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 索引与外键请逐句复制判断执行（旧版 MySQL 的裸 IF 只能在存储过程里，脚本里直接写会 1064）。
-- 查看是否已有索引/外键：
--   SELECT index_name FROM information_schema.statistics WHERE table_schema=DATABASE() AND table_name='stock_movement';
--   SELECT constraint_name FROM information_schema.table_constraints WHERE table_schema=DATABASE() AND table_name='stock_movement' AND constraint_type='FOREIGN KEY';
-- 若无则手动执行：
--   CREATE INDEX idx_sm_product_size ON stock_movement(product_id,size);
--   CREATE INDEX idx_sm_order ON stock_movement(order_id);
-- 如果需要外键且 product 表已经存在：
--   ALTER TABLE stock_movement ADD CONSTRAINT fk_sm_product FOREIGN KEY (product_id) REFERENCES product(product_id);

-- 可选：历史数据迁移逻辑可放在这里