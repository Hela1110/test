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

    // 新增：按商品统计（指定尺码，仅 PAID）
    @Query("select i.product.productId as productId, i.product.name as name, sum(i.quantity) as qty, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where i.size = :size and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by i.product.productId, i.product.name order by sum(i.quantity) desc")
    List<Object[]> sumSalesByProductForSize(@Param("start") java.time.LocalDateTime start,
                                            @Param("end") java.time.LocalDateTime end,
                                            @Param("size") Integer size);

    // 新增：指定用户、指定尺码的按商品统计
    @Query("select i.product.productId as productId, i.product.name as name, sum(i.quantity) as qty, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where i.size = :size and h.client = :client and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by i.product.productId, i.product.name order by sum(i.quantity) desc")
    List<Object[]> sumSalesByProductForClientAndSize(@Param("client") Client client,
                                                     @Param("start") java.time.LocalDateTime start,
                                                     @Param("end") java.time.LocalDateTime end,
                                                     @Param("size") Integer size);

    // 新增：按尺码统计销量与销售额（仅 PAID 订单），限定时间范围；忽略 size 为空的记录
    @Query("select i.size as size, sum(i.quantity) as qty, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where i.size is not null and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by i.size order by i.size asc")
    List<Object[]> sumSalesBySize(@Param("start") java.time.LocalDateTime start,
                                  @Param("end") java.time.LocalDateTime end);

    // 新增：月度金额统计（仅 PAID，指定尺码）。与 OrderHeaderRepository.sumByMonth 同形状：year, month, amount
    @Query("select year(h.createdAt) as y, month(h.createdAt) as m, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where i.size = :size and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by year(h.createdAt), month(h.createdAt) order by y, m")
    List<Object[]> sumAmountByMonthForSize(@Param("start") java.time.LocalDateTime start,
                                           @Param("end") java.time.LocalDateTime end,
                                           @Param("size") Integer size);

    // 新增：按用户 + 尺码 的月度金额统计
    @Query("select year(h.createdAt) as y, month(h.createdAt) as m, sum(i.price * i.quantity) as amount " +
           "from OrderItem i join i.order h " +
           "where i.size = :size and h.client = :client and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
           "group by year(h.createdAt), month(h.createdAt) order by y, m")
    List<Object[]> sumAmountByMonthForClientAndSize(@Param("client") Client client,
                                                    @Param("start") java.time.LocalDateTime start,
                                                    @Param("end") java.time.LocalDateTime end,
                                                    @Param("size") Integer size);
}
