# 后端与数据库变更说明：商品销量与排序

本文档描述为支持客户端的“销量/价格/打折优先”排序与销量展示，需要在数据库和后端接口进行的改造。

---

## 1) 数据库变更

- 为商品表增加销量字段，并默认值为 0：

```sql
ALTER TABLE products
  ADD COLUMN sales INT NOT NULL DEFAULT 0 AFTER stock;
```

- 可选：从历史订单回填销量（以订单项表 `order_items` 为例，包含 `product_id`、`quantity` 字段；请按你的真实表名替换）：

```sql
-- 将历史订单累计销量写回 products.sales
UPDATE products p
JOIN (
  SELECT oi.product_id, COALESCE(SUM(oi.quantity), 0) AS total_sales
  FROM order_items oi
  JOIN orders o ON o.id = oi.order_id AND o.status = '已支付'  -- 仅统计已支付订单
  GROUP BY oi.product_id
) t ON t.product_id = p.id
SET p.sales = t.total_sales;
```

- 事务中扣减库存时，同时累加销量（以一次性购买 `n` 件为例）：

```sql
-- 同一个事务内完成扣减库存与累积销量
UPDATE products
SET stock = stock - :n,
    sales = sales + :n
WHERE id = :productId AND stock >= :n;
```

- 可选：为常用排序创建索引（注意：MySQL 对多列排序/分页的优化取决于查询模式，是否建立索引要结合你的慢查询观察）：

```sql
CREATE INDEX idx_products_sales ON products(sales DESC);
CREATE INDEX idx_products_price ON products(price);
```

---

## 2) 接口变更：商品分页与排序

客户端请求：

```json
{
  "type": "get_products",
  "page": 1,
  "size": 12,
  "sort": "sales_desc" | "price_asc" | "price_desc" | "discount_first"
}
```

- page：页码（从 1 开始）
- size：每页条数
- sort：排序模式
  - `sales_desc`：销量倒序（销量越高越靠前），价格作为次级排序可选
  - `price_asc`：按“有效价格”升序（见下），销量作为次级排序可选
  - `price_desc`：按“有效价格”降序
  - `discount_first`：打折商品优先；其次按有效价格升序

有效价格定义：有折扣时取 `discount_price`，否则取 `price`。

建议在 SQL 层处理排序（覆盖全量数据），客户端仅做 UI 即时反馈的本地重排，以保持与后端一致。

示例（MySQL 8+，请按你的表结构字段名替换）：

```sql
-- 基础分页
SET @offset := (:page - 1) * :size;

-- 有效价格计算：CASE WHEN on_sale AND discount_price BETWEEN 0 AND price THEN discount_price ELSE price END

-- sales_desc
SELECT *
FROM products
ORDER BY sales DESC, price ASC
LIMIT :size OFFSET @offset;

-- price_asc
SELECT *
FROM products
ORDER BY
  CASE WHEN on_sale = 1 AND discount_price > 0 AND discount_price < price THEN discount_price ELSE price END ASC,
  sales DESC
LIMIT :size OFFSET @offset;

-- price_desc
SELECT *
FROM products
ORDER BY
  CASE WHEN on_sale = 1 AND discount_price > 0 AND discount_price < price THEN discount_price ELSE price END DESC,
  sales DESC
LIMIT :size OFFSET @offset;

-- discount_first
SELECT *
FROM products
ORDER BY
  (CASE WHEN on_sale = 1 AND discount_price > 0 AND discount_price < price THEN 0 ELSE 1 END) ASC, -- 打折优先
  CASE WHEN on_sale = 1 AND discount_price > 0 AND discount_price < price THEN discount_price ELSE price END ASC,
  sales DESC
LIMIT :size OFFSET @offset;
```

> 如果你使用 JPA/QueryDSL，可在 Repository/Mapper 中按 `sort` 枚举拼接 `ORDER BY`；若用 MyBatis 则在 SQL 中 `CASE WHEN` 或 `<choose>` 处理。

---

## 3) 接口响应：请返回销量与促销字段

客户端期望的字段（示例）：

```json
{
  "type": "products_response",
  "total": 123,
  "products": [
    {
      "id": 1,
      "name": "xxx",
      "price": 199.00,
      "stock": 20,
      "sales": 56,
      "onSale": true,
      "discountPrice": 149.00,
      "imageUrl": "...",
      "description": "..."
    }
  ]
}
``;

其中 `sales`、`onSale`、`discountPrice` 均为可选，但若存在将用于排序与展示。

---

## 4) 搜索接口（可选增强）

若你也希望在“搜索结果”里支持同样的排序，可让 `search_products` 也接受可选 `sort` 参数，逻辑与 `get_products` 一致；客户端已做本地排序兜底。

---

## 5) 注意事项

- 库存与销量的变更务必在同一事务中更新，避免并发导致超卖或销量不一致。
- 大表分页排序注意加覆盖索引或限制扫描行数；必要时可引入搜索引擎或预聚合表。
- 历史数据回填后，记得对 `sales` 做一次全量校验（随机抽样比对订单聚合）。
