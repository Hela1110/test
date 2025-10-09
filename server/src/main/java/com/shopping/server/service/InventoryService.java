package com.shopping.server.service;

import com.shopping.server.model.Product;
import com.shopping.server.model.ProductSizeInventory;
import com.shopping.server.model.StockMovement;
import com.shopping.server.repository.ProductRepository;
import com.shopping.server.repository.ProductSizeInventoryRepository;
import com.shopping.server.repository.StockMovementRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Slf4j
@Service
@RequiredArgsConstructor
public class InventoryService {

    private final ProductRepository productRepository;
    private final ProductSizeInventoryRepository psiRepository;
    private final StockMovementRepository movementRepository;

    private static final int MAX_RETRY = 5;

    @Transactional
    public void deduct(Product product, int size, int quantity, Long orderId, String operator) {
        applyChange(product, size, -quantity, "ORDER_DEDUCT", orderId, operator);
    }

    @Transactional
    public void rollback(Product product, int size, int quantity, Long orderId, String operator) {
        applyChange(product, size, quantity, "REFUND_ROLLBACK", orderId, operator);
    }

    @Transactional
    public void adjust(Product product, int size, int delta, String operator) {
        applyChange(product, size, delta, "ADMIN_ADJUST", null, operator);
    }

    private void applyChange(Product product, int size, int delta, String action, Long orderId, String operator) {
        int attempt = 0;
        while (true) {
            attempt++;
            ProductSizeInventory psi = psiRepository.findByProductAndSize(product, size)
                    .orElseGet(() -> {
                        ProductSizeInventory created = new ProductSizeInventory();
                        created.setProduct(product);
                        created.setSize(size);
                        created.setStock(0);
                        return psiRepository.save(created);
                    });
            int newStock = psi.getStock() + delta;
            if (newStock < 0) {
                throw new IllegalStateException("库存不足: product=" + product.getProductId() + " size=" + size);
            }
            psi.setStock(newStock);
            try {
                psiRepository.saveAndFlush(psi); // 利用 @Version 实现并发控制
                // 更新总库存 = 各尺码之和
                recomputeTotalStock(product);
                recordMovement(product, size, delta, action, orderId, operator);
                return;
            } catch (org.springframework.orm.ObjectOptimisticLockingFailureException e) {
                log.warn("库存并发冲突重试 attempt={} product={} size={}", attempt, product.getProductId(), size);
                if (attempt >= MAX_RETRY) {
                    throw e;
                }
            }
        }
    }

    private void recomputeTotalStock(Product product) {
        List<ProductSizeInventory> all = psiRepository.findByProduct(product);
        int sum = all.stream().mapToInt(ProductSizeInventory::getStock).sum();
        product.setStock(sum);
        productRepository.save(product);
    }

    private void recordMovement(Product product, int size, int delta, String action, Long orderId, String operator) {
        StockMovement m = new StockMovement();
        m.setProduct(product);
        m.setSize(size);
        m.setDelta(delta);
        m.setAction(action);
        m.setOrderId(orderId);
        m.setOperator(operator == null ? "system" : operator);
        movementRepository.save(m);
    }
}
