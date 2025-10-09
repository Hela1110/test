package com.shopping.server.admin;

import com.shopping.server.repository.ProductRepository;
import com.shopping.server.repository.ChatMessageRepository;
import com.shopping.server.repository.OrderHeaderRepository;
import com.shopping.server.repository.OrderItemRepository;
import com.shopping.server.repository.ProductTypeRepository;
import com.shopping.server.repository.ProductSizeInventoryRepository;
import com.shopping.server.repository.StockMovementRepository;
import com.shopping.server.service.InventoryService;
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
    private final ProductSizeInventoryRepository psiRepository;
    private final InventoryService inventoryService;
    private final StockMovementRepository stockMovementRepository;

    public AdminConsoleLauncher(ProductRepository productRepository,
                               ChatMessageRepository chatMessageRepository,
                               OrderHeaderRepository orderHeaderRepository,
                               OrderItemRepository orderItemRepository,
                               ClientRepository clientRepository,
                               ProductTypeRepository productTypeRepository,
                               ProductSizeInventoryRepository psiRepository,
                               InventoryService inventoryService,
                               StockMovementRepository stockMovementRepository) {
        this.productRepository = productRepository;
        this.chatMessageRepository = chatMessageRepository;
        this.orderHeaderRepository = orderHeaderRepository;
        this.orderItemRepository = orderItemRepository;
        this.clientRepository = clientRepository;
        this.productTypeRepository = productTypeRepository;
        this.psiRepository = psiRepository;
        this.inventoryService = inventoryService;
        this.stockMovementRepository = stockMovementRepository;
    }

    @EventListener(ApplicationReadyEvent.class)
    public void onReady() {
        // 在无图形（headless）环境下跳过 AdminFrame，防止 HeadlessException
        if (java.awt.GraphicsEnvironment.isHeadless()) {
            System.out.println("[INFO] Detected headless environment, skip launching AdminFrame GUI.");
            return;
        }
        SwingUtilities.invokeLater(() -> {
            try {
                UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
            } catch (Exception ignore) {
                System.out.println("[WARN] Failed to set look and feel: " + ignore.getMessage());
            }
            try {
                AdminFrame frame = new AdminFrame(productRepository, chatMessageRepository, orderHeaderRepository, orderItemRepository, clientRepository, productTypeRepository, psiRepository, inventoryService, stockMovementRepository);
                frame.setVisible(true);
                System.out.println("[INFO] AdminFrame launched successfully.");
            } catch (java.awt.HeadlessException he) {
                System.out.println("[WARN] HeadlessException launching AdminFrame, skipping GUI. " + he.getMessage());
            } catch (Throwable t) {
                System.out.println("[ERROR] Unexpected error launching AdminFrame: " + t.getMessage());
            }
        });
    }
}
