package com.shopping.server;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

// 仅扫描 socket 包，避免加载依赖数据库/JPA 的 service/repository
@SpringBootApplication(scanBasePackages = {"com.shopping.server.socket"})
public class ShoppingServerApplication {
    public static void main(String[] args) {
        SpringApplication.run(ShoppingServerApplication.class, args);
    }
}