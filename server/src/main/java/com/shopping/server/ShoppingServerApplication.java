package com.shopping.server;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

// 扩展扫描至整个 com.shopping.server，加载 repository/service 以启用 JPA
@SpringBootApplication(scanBasePackages = {"com.shopping.server"})
public class ShoppingServerApplication {
    public static void main(String[] args) {
        SpringApplication.run(ShoppingServerApplication.class, args);
    }
}