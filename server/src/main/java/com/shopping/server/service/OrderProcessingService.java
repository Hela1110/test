package com.shopping.server.service;

import com.shopping.server.model.*;
import com.shopping.server.repository.*;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.time.LocalDateTime;
import java.util.Optional;

@Service
@RequiredArgsConstructor
public class OrderProcessingService {
    private final OrderHeaderRepository orderHeaderRepository;
    private final OrderItemRepository orderItemRepository;
    private final ProductRepository productRepository;
    private final ClientRepository clientRepository;

    /**
     * 将商品加入购物车（OrderHeader.status = CART）。
     */
    @Transactional
    public OrderHeader addToCart(Long clientId, Long productId, int quantity) {
        Client client = clientRepository.findById(clientId)
                .orElseThrow(() -> new IllegalArgumentException("Client not found: " + clientId));
        Product product = productRepository.findById(productId)
                .orElseThrow(() -> new IllegalArgumentException("Product not found: " + productId));

        if (quantity <= 0) throw new IllegalArgumentException("Quantity must be > 0");

        // 查找用户当前 CART 头
        OrderHeader cartHeader = orderHeaderRepository
                .findFirstByClientAndStatus(client, OrderStatus.CART)
                .orElseGet(() -> {
                    OrderHeader h = new OrderHeader();
                    h.setClient(client);
                    h.setCreatedAt(LocalDateTime.now());
                    h.setStatus(OrderStatus.CART);
                    h.setTotalPrice(BigDecimal.ZERO);
                    return h;
                });

        // 尝试合并已有行（同一个 product）
        Optional<OrderItem> existing = cartHeader.getItems().stream()
                .filter(i -> i.getProduct().getProductId().equals(productId))
                .findFirst();
        if (existing.isPresent()) {
            existing.get().setQuantity(existing.get().getQuantity() + quantity);
        } else {
            OrderItem item = new OrderItem();
            item.setProduct(product);
            item.setQuantity(quantity);
            item.setPrice(product.getPrice()); // 快照价格
            cartHeader.addItem(item);
        }
        cartHeader.setTotalPrice(cartHeader.calcTotal());
        return orderHeaderRepository.save(cartHeader);
    }

    /**
     * 结算购物车：改变状态为 PAID，扣减库存。
     */
    @Transactional
    public OrderHeader checkout(Long orderHeaderId) {
        OrderHeader header = orderHeaderRepository.findById(orderHeaderId)
                .orElseThrow(() -> new IllegalArgumentException("OrderHeader not found: " + orderHeaderId));
        if (header.getStatus() != OrderStatus.CART) {
            throw new IllegalStateException("OrderHeader not in CART state");
        }
        // 库存校验 + 扣减
        header.getItems().forEach(item -> {
            Product p = item.getProduct();
            if (p.getStock() != null) {
                int remain = p.getStock() - item.getQuantity();
                if (remain < 0) {
                    throw new IllegalStateException("Stock not enough for product " + p.getProductId());
                }
                p.setStock(remain);
            }
        });
        header.setStatus(OrderStatus.PAID);
        header.setTotalPrice(header.calcTotal());
        return header; // 由于持久化上下文, 修改会自动 flush
    }

    /**
     * 逻辑删除：标记头为 DELETED_BY_CLIENT。
     */
    @Transactional
    public void logicalDelete(Long orderHeaderId) {
        OrderHeader header = orderHeaderRepository.findById(orderHeaderId)
                .orElseThrow(() -> new IllegalArgumentException("OrderHeader not found: " + orderHeaderId));
        header.setStatus(OrderStatus.DELETED_BY_CLIENT);
    }

    /**
     * 从购物车移除某个商品行。
     */
    @Transactional
    public OrderHeader removeItem(Long orderHeaderId, Long orderItemId) {
        OrderHeader header = orderHeaderRepository.findById(orderHeaderId)
                .orElseThrow(() -> new IllegalArgumentException("OrderHeader not found: " + orderHeaderId));
        header.getItems().removeIf(i -> i.getId().equals(orderItemId));
        header.setTotalPrice(header.calcTotal());
        return header;
    }
}
