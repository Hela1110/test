-- 为库存流水 created_at 添加索引（如果还没有）
ALTER TABLE stock_movement ADD INDEX idx_sm_created_at (created_at);
-- 如果已存在会报重复，可忽略。