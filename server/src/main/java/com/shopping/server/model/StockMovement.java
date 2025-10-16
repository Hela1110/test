package com.shopping.server.model;

import lombok.Data;
import org.hibernate.annotations.CreationTimestamp;

import javax.persistence.*;
import java.time.LocalDateTime;

@Data
@Entity
@Table(name = "stock_movement", indexes = {
        @Index(name = "idx_sm_product_size", columnList = "product_id,size"),
        @Index(name = "idx_sm_order", columnList = "order_id")
})
public class StockMovement {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "product_id", nullable = false)
    private Product product;

    @Column(name = "size", nullable = false)
    private Integer size; // 37-45

    // 变化数量，扣减为负，增加为正
    @Column(name = "delta", nullable = false)
    private Integer delta;

    @Column(name = "action", length = 40, nullable = false)
    private String action; // ORDER_DEDUCT, REFUND_ROLLBACK, ADMIN_ADJUST

    @Column(name = "order_id")
    private Long orderId;

    @Column(name = "operator", length = 64)
    private String operator; // 触发者(用户名/系统)

    @CreationTimestamp
    @Column(name = "created_at", updatable = false)
    private LocalDateTime createdAt;
}
