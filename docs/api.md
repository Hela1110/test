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