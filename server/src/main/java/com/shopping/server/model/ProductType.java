package com.shopping.server.model;

import lombok.Data;
import javax.persistence.*;

@Data
@Entity
@Table(name = "product_types")
public class ProductType {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "type_id")
    private Long typeId;

    @Column(name = "type_name", nullable = false)
    private String typeName;

    @Column(nullable = false)
    private String value; // 具体属性值，例如 8G / 黑色

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "product_id")
    private Product product;
}
