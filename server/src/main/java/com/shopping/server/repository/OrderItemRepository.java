package com.shopping.server.repository;

import com.shopping.server.model.OrderItem;
import com.shopping.server.model.OrderHeader;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface OrderItemRepository extends JpaRepository<OrderItem, Long> {
    List<OrderItem> findByOrder(OrderHeader header);
}
