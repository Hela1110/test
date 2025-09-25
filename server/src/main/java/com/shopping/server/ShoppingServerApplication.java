package com.shopping.server;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.autoconfigure.domain.EntityScan;
import org.springframework.data.jpa.repository.config.EnableJpaRepositories;
import org.springframework.transaction.annotation.EnableTransactionManagement;

// 扫描整个 com.shopping.server 包，使 JPA 实体与仓库生效
@SpringBootApplication
@EnableJpaRepositories(basePackages = "com.shopping.server.repository")
@EntityScan(basePackages = "com.shopping.server.model")
@EnableTransactionManagement
public class ShoppingServerApplication {
    public static void main(String[] args) {
        SpringApplication.run(ShoppingServerApplication.class, args);
    }
}