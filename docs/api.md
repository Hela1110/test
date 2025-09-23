# API 文档

## 客户端-服务器通信协议

### 消息格式
所有消息采用 JSON 格式，基本结构如下：
```json
{
    "type": "消息类型",
    "data": {
        // 具体数据
    }
}
```

### 认证相关 API

#### 1. 登录
- 请求：
```json
{
    "type": "login",
    "username": "用户名",
    "password": "密码"
}
```
- 响应：
```json
{
    "type": "login_response",
    "success": true/false,
    "message": "成功/错误信息"
}
```

#### 2. 注册
- 请求：
```json
{
    "type": "register",
    "username": "用户名",
    "password": "密码",
    "email": "邮箱",
    "phone": "电话"
}
```
- 响应：
```json
{
    "type": "register_response",
    "success": true/false,
    "message": "成功/错误信息"
}
```

### 商品相关 API

#### 1. 获取商品列表
- 请求：
```json
{
    "type": "get_products",
    "page": 页码,
    "size": 每页数量
}
```
- 响应：
```json
{
    "type": "products_response",
    "total": 总数量,
    "products": [
        {
            "id": "商品ID",
            "name": "商品名称",
            "price": 价格,
            "description": "描述",
            "imageUrl": "图片URL",
            "stock": 库存量
        }
    ]
}
```

#### 2. 搜索商品
- 请求：
```json
{
    "type": "search_products",
    "keyword": "搜索关键词"
}
```
- 响应：同获取商品列表

### 购物车相关 API

#### 1. 获取购物车
- 请求：
```json
{
    "type": "get_cart"
}
```
- 响应：
```json
{
    "type": "cart_response",
    "items": [
        {
            "productId": "商品ID",
            "name": "商品名称",
            "price": 价格,
            "quantity": 数量
        }
    ]
}
```

#### 2. 添加到购物车
- 请求：
```json
{
    "type": "add_to_cart",
    "productId": "商品ID",
    "quantity": 数量
}
```
- 响应：
```json
{
    "type": "add_to_cart_response",
    "success": true/false,
    "message": "成功/错误信息"
}
```

#### 3. 设置购物车数量
- 请求：
```json
{
    "type": "set_cart_quantity",
    "product_id": 商品ID,
    "quantity": 数量
}
```
- 响应：同获取购物车（`cart_response` / `cart_items`），数量为 0 等同于移除该商品

### 订单相关 API

#### 1. 创建订单
- 请求：
```json
{
    "type": "create_order",
    "items": [
        {
            "productId": "商品ID",
            "quantity": 数量
        }
    ]
}
```
- 响应：
```json
{
    "type": "order_response",
    "success": true/false,
    "orderId": "订单ID",
    "message": "成功/错误信息"
}
```

## 错误处理

所有API在发生错误时都会返回以下格式：
```json
{
    "type": "error",
    "code": 错误码,
    "message": "错误信息"
}
```

### 错误码说明
- 1001: 认证失败
- 1002: 参数无效
- 2001: 商品不存在
- 2002: 库存不足
- 3001: 购物车为空
- 3002: 订单创建失败

---

## 客户端字段映射与兼容说明

为平滑过渡，服务端在一段时间内同时支持“旧类型/字段”和“文档类型/字段”，客户端做如下映射与处理：

- 搜索/商品列表
    - 请求：优先使用 `search_products` 与 `get_products { page, size }`
    - 响应：`products_response { total, products[] }`
    - 客户端会将 `products[].id/name/price/[description]/[imageUrl]` 映射为内部使用的 `product_id/name/price/[description]/[image_url]`，以复用既有渲染逻辑

- 购物车
    - 请求：`get_cart`
    - 响应：同时可能收到：
        - 旧：`cart_items { items: [{ product_id, name, price, quantity }] }`
        - 新：`cart_response { items: [{ productId, name, price, quantity }] }`
    - 客户端在接收 `cart_response` 时会将 `productId` 转为 `product_id` 进行统一处理

- 添加到购物车
    - 请求：兼容 `product_id` 与 `productId`，支持 `quantity`
    - 响应：`add_to_cart_response { success, message? }`

- 下单/结算
    - 新流程：`create_order { items: [{ productId, quantity }] }` → `order_response { success, orderId?, message, code? }`
    - 旧流程：`checkout` → `checkout_response { success, message }`，同时服务端也会额外返回 `order_response` 便于过渡
    - 客户端默认走 `create_order`，避免旧新并行导致重复提示；接收到 `checkout_response` 时也能兼容处理但不重复弹窗

- 错误统一
    - 任意接口错误均可能返回 `{ type: "error", code, message }`
    - 客户端会根据 code 映射友好提示（见“错误码说明”），并在 UI 侧做弹窗去重，避免短时间重复提示