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
}