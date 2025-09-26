package com.shopping.server.admin;

import com.shopping.server.repository.ProductRepository;
import com.shopping.server.repository.ChatMessageRepository;
import com.shopping.server.repository.OrderHeaderRepository;
import com.shopping.server.repository.OrderItemRepository;
import com.shopping.server.repository.ProductTypeRepository;
import com.shopping.server.repository.ClientRepository;
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
    private final ChatMessageRepository chatMessageRepository;
    private final OrderHeaderRepository orderHeaderRepository;
    private final OrderItemRepository orderItemRepository;
    private final ProductTypeRepository productTypeRepository;
    private final ClientRepository clientRepository;

    public AdminConsoleLauncher(ProductRepository productRepository,
                               ChatMessageRepository chatMessageRepository,
                               OrderHeaderRepository orderHeaderRepository,
                               OrderItemRepository orderItemRepository,
                               ClientRepository clientRepository,
                               ProductTypeRepository productTypeRepository) {
        this.productRepository = productRepository;
        this.chatMessageRepository = chatMessageRepository;
        this.orderHeaderRepository = orderHeaderRepository;
        this.orderItemRepository = orderItemRepository;
        this.clientRepository = clientRepository;
        this.productTypeRepository = productTypeRepository;
    }

    @EventListener(ApplicationReadyEvent.class)
    public void onReady() {
        SwingUtilities.invokeLater(() -> {
            try {
                UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
            } catch (Exception ignore) {}
            AdminFrame frame = new AdminFrame(productRepository, chatMessageRepository, orderHeaderRepository, orderItemRepository, clientRepository, productTypeRepository);
            frame.setVisible(true);
        });
    }
}
