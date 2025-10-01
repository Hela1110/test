package com.shopping.server.model;

import lombok.Data;

import javax.persistence.*;

@Data
@Entity
@Table(name = "product_size_inventory",
       uniqueConstraints = @UniqueConstraint(columnNames = {"product_id", "size"}))
public class ProductSizeInventory {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "product_id", nullable = false)
    private Product product;

    // 鞋码，整数 37-45
    @Column(name = "size", nullable = false)
    private Integer size;

    // 该尺码库存
    @Column(name = "stock", nullable = false)
    private Integer stock = 0;
}
