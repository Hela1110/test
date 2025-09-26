package com.shopping.server.repository;

import com.shopping.server.model.OrderItem;
import com.shopping.server.model.OrderHeader;
import com.shopping.server.model.Client;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface OrderItemRepository extends JpaRepository<OrderItem, Long> {
    List<OrderItem> findByOrder(OrderHeader header);

    // 按商品统计销量与销售额（仅 PAID 订单），限定时间范围
    @Query("select i.product.productId as productId, i.product.name as name, sum(i.quantity) as qty, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by i.product.productId, i.product.name order by sum(i.quantity) desc")
    List<Object[]> sumSalesByProduct(@Param("start") java.time.LocalDateTime start,
                                     @Param("end") java.time.LocalDateTime end);

    // 指定用户的按商品统计
    @Query("select i.product.productId as productId, i.product.name as name, sum(i.quantity) as qty, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where h.client = :client and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by i.product.productId, i.product.name order by sum(i.quantity) desc")
    List<Object[]> sumSalesByProductForClient(@Param("client") Client client,
                                              @Param("start") java.time.LocalDateTime start,
                                              @Param("end") java.time.LocalDateTime end);
}
