package com.shopping.server.model;

/**
 * 订单 / 购物车状态统一枚举
 */
public enum OrderStatus {
    CART,            // 仍在购物车（未结算）
    PAID,            // 已支付 / 成交
    CANCELLED,       // 已取消
    DELETED_BY_CLIENT // 用户逻辑删除（仍保留数据）
}
