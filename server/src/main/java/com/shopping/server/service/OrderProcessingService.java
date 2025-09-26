package com.shopping.server.service;

import com.shopping.server.model.*;
import com.shopping.server.repository.*;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.time.LocalDateTime;
import java.util.Optional;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

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
     * 按所选条目“部分结算”：
     * - 从当前购物车(CART)中挑选 items 对应的明细，校验库存并扣减；
     * - 生成一个新的订单头（PAID）承载这些条目；
     * - 从购物车中仅移除被结算的条目，保留未勾选；
     * - 返回新订单头。
     *
     * items: List<Map>，每项需包含 productId/product_id 和 quantity。
     */
    @Transactional
    public OrderHeader createOrderForItems(Long clientId, List<Map<String, Object>> items) {
        if (items == null || items.isEmpty()) {
            throw new IllegalArgumentException("No items to create order");
        }
        Client client = clientRepository.findById(clientId)
                .orElseThrow(() -> new IllegalArgumentException("Client not found: " + clientId));

        // 抓取当前购物车（含明细与商品）
        OrderHeader cart = orderHeaderRepository
                .findFirstByClientAndStatus(client, OrderStatus.CART)
                .orElseThrow(() -> new IllegalStateException("Cart is empty"));

        // 构建选择表：productId -> quantity（客户端已是合并后的数量，这里再次合并一遍以防重复）
        Map<Long, Integer> chosen = new HashMap<>();
        for (Map<String, Object> it : items) {
            Object pidObj = it.get("productId");
            if (pidObj == null) pidObj = it.get("product_id");
            Object qObj = it.getOrDefault("quantity", 1);
            if (pidObj == null) continue;
            long pid = (pidObj instanceof Number) ? ((Number) pidObj).longValue() : Long.parseLong(String.valueOf(pidObj));
            int qty = (qObj instanceof Number) ? ((Number) qObj).intValue() : Integer.parseInt(String.valueOf(qObj));
            if (qty <= 0) continue;
            chosen.merge(pid, qty, Integer::sum);
        }
        if (chosen.isEmpty()) {
            throw new IllegalArgumentException("No valid items");
        }

        // 新订单头
        OrderHeader order = new OrderHeader();
        order.setClient(client);
        order.setCreatedAt(LocalDateTime.now());
        order.setStatus(OrderStatus.CART); // 先置 CART，扣减库存后再标记 PAID
        order.setTotalPrice(BigDecimal.ZERO);

        // 遍历购物车条目，挑出被选中的，校验库存并转移到新订单
        List<OrderItem> remaining = new java.util.ArrayList<>();
        for (OrderItem ci : cart.getItems()) {
            Long pid = ci.getProduct().getProductId();
            Integer pickQty = chosen.get(pid);
            if (pickQty == null) {
                remaining.add(ci); // 未选择，保留在购物车
                continue;
            }
            int useQty = Math.min(ci.getQuantity(), Math.max(1, pickQty));
            Product p = ci.getProduct();
            if (p.getStock() != null) {
                int remain = p.getStock() - useQty;
                if (remain < 0) {
                    throw new IllegalStateException("Stock not enough for product " + p.getProductId());
                }
                p.setStock(remain);
            }
            // 放入新订单
            OrderItem oi = new OrderItem();
            oi.setProduct(p);
            oi.setQuantity(useQty);
            oi.setPrice(ci.getPrice());
            order.addItem(oi);

            // 如果购物车该行数量大于选中数量，则减少剩余数量后保留；否则移除
            int left = ci.getQuantity() - useQty;
            if (left > 0) {
                ci.setQuantity(left);
                remaining.add(ci);
            } // else: 不加入 remaining，即移除该行
        }

        // 更新新订单与购物车状态
        order.setTotalPrice(order.calcTotal());
        order.setStatus(OrderStatus.PAID);
        // 购物车替换剩余条目
        cart.getItems().clear();
        remaining.forEach(cart::addItem);
        cart.setTotalPrice(cart.calcTotal());

        // 持久化：由于关系为 cascade = ALL，保存 header 即可级联保存明细
        orderHeaderRepository.save(cart);
        return orderHeaderRepository.save(order);
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
