-- 批量为现有“鞋类”商品（或指定范围商品）补齐 37–45 尺码库存行
-- 注意：下面 WHERE 条件请按你的分类/命名规则调整。若没有分类字段，可改为 product_id IN (...)

START TRANSACTION;

-- 基于产品范围：为所有商品补齐 37-45 的尺码（兼容 MySQL 5.7：使用内联派生表而非 WITH CTE）
INSERT INTO product_size_inventory(product_id, size, stock)
SELECT p.product_id, s.size, 0
FROM products p
CROSS JOIN (
    SELECT 37 AS size UNION ALL SELECT 38 UNION ALL SELECT 39 UNION ALL SELECT 40 UNION ALL
    SELECT 41 UNION ALL SELECT 42 UNION ALL SELECT 43 UNION ALL SELECT 44 UNION ALL SELECT 45
) AS s
LEFT JOIN product_size_inventory psi
       ON psi.product_id = p.product_id AND psi.size = s.size
WHERE psi.id IS NULL;

-- 将总库存与尺码库存对齐：用各尺码库存之和回填 products.stock
UPDATE products p
LEFT JOIN (
    SELECT product_id, SUM(stock) AS sum_stock
    FROM product_size_inventory
    GROUP BY product_id
) t ON t.product_id = p.product_id
SET p.stock = COALESCE(t.sum_stock, 0);

-- 如果你有明确的鞋类分类字段，比如 products.category = 'shoes'，请改为：
-- WHERE p.category = 'shoes' AND psi.id IS NULL;

COMMIT;

-- 可选：为某些爆款默认给 42 尺码 10 双库存（示例）
-- UPDATE product_size_inventory psi
-- JOIN products p ON p.product_id = psi.product_id
-- SET psi.stock = 10
-- WHERE psi.size = 42 AND (p.name LIKE '%爆款%' OR p.description LIKE '%爆款%');
