package com.shopping.server.model;

import javax.persistence.*;
import lombok.Data;
import java.math.BigDecimal;

@Data
@Entity
@Table(name = "products")
public class Product {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long productId;
    
    @Column(nullable = false)
    private String name;
    
    @Column(nullable = false)
    private BigDecimal price;
    
    private String description;
    private String imageUrl;
    private Integer stock;
    private Boolean onSale;
    private BigDecimal discountPrice;
    // 商品累计销量（非空），默认为 0
    @Column(nullable = false)
    private Integer sales = 0;

    @PrePersist
    public void prePersist() {
        if (sales == null) sales = 0;
    }

    @PreUpdate
    public void preUpdate() {
        if (sales == null) sales = 0;
    }
}