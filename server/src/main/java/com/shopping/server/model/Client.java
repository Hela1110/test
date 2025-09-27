package com.shopping.server.model;

import javax.persistence.*;
import lombok.Data;

@Data
@Entity
@Table(name = "clients")
public class Client {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long clientId;
    
    @Column(nullable = false)
    private String username;
    
    @Column(nullable = false)
    private String password;
    
    private String avatar;
    @Column(unique = true)
    private String email;
    @Column(unique = true)
    private String phone;
    private Integer purchaseCount;
}