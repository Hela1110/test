package com.shopping.server.service;

import com.shopping.server.model.Order;
import com.shopping.server.repository.OrderRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.List;

/**
 * 旧的基于单表 orders 的服务，已被新的 OrderProcessingService (头+行模型) 取代。
 * 后续可删除：暂时保留以兼容仍可能调用它的代码路径。
 */
@Deprecated
@Service
public class OrderService {
    @Autowired
    private OrderRepository orderRepository;
    
    @Autowired
    private ProductService productService;
    
    public List<Order> getClientOrders(Long clientId) {
        return orderRepository.findByClientClientId(clientId);
    }
    
    public List<Order> getClientCart(Long clientId) {
        return orderRepository.findByClientClientIdAndStatus(clientId, "CART");
    }
    
    @Transactional
    public Order addToCart(Order order) {
        order.setStatus("CART");
        return orderRepository.save(order);
    }
    
    @Transactional
    public boolean checkout(Long clientId) {
        List<Order> cartItems = getClientCart(clientId);
        boolean success = true;
        
        for (Order order : cartItems) {
            if (productService.updateStock(order.getProduct().getProductId(), order.getQuantity())) {
                order.setStatus("PAID");
                order.setOrderTime(LocalDateTime.now());
                orderRepository.save(order);
            } else {
                success = false;
                break;
            }
        }
        
        return success;
    }
    
    @Transactional
    public void removeFromCart(Long orderId) {
        orderRepository.deleteById(orderId);
    }
}