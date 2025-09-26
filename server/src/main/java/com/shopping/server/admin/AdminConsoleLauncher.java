package com.shopping.server.admin;

import com.shopping.server.repository.ProductRepository;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;
import org.springframework.boot.context.event.ApplicationReadyEvent;

import javax.swing.*;

/**
 * 在 Spring Boot 完全启动后，启动服务端本地管理控制台（无需登录）。
 */
@Component
public class AdminConsoleLauncher {
    private final ProductRepository productRepository;

    public AdminConsoleLauncher(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    @EventListener(ApplicationReadyEvent.class)
    public void onReady() {
        SwingUtilities.invokeLater(() -> {
            try {
                UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
            } catch (Exception ignore) {}
            AdminFrame frame = new AdminFrame(productRepository);
            frame.setVisible(true);
        });
    }
}
