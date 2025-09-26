package com.shopping.server.repository;

import com.shopping.server.model.OrderHeader;
import com.shopping.server.model.Client;
import com.shopping.server.model.OrderStatus;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;

import java.util.List;
import java.util.Optional;

public interface OrderHeaderRepository extends JpaRepository<OrderHeader, Long> {
    List<OrderHeader> findByClient(Client client);
    Optional<OrderHeader> findFirstByClientAndStatus(Client client, OrderStatus status);
    Page<OrderHeader> findByClientAndStatus(Client client, OrderStatus status, Pageable pageable);

    // Fetch-join 购物车头与明细及商品，避免 LazyInitializationException（Netty 线程无事务）
    @Query("select distinct h from OrderHeader h left join fetch h.items i left join fetch i.product p where h.client = :client and h.status = :status")
    Optional<OrderHeader> fetchCartWithItems(@Param("client") Client client, @Param("status") OrderStatus status);

    // 获取某用户的所有订单并抓取明细和商品（不包含购物车过滤逻辑，调用方自行跳过 CART）
    @Query("select distinct h from OrderHeader h left join fetch h.items i left join fetch i.product p where h.client = :client")
    List<OrderHeader> findByClientWithItems(@Param("client") Client client);

    // ========== 统计相关（用于图表） ==========
    // 每月订单金额汇总（全体用户），限定时间范围与状态
    @Query("select year(h.createdAt) as y, month(h.createdAt) as m, sum(h.totalPrice) as total " +
        "from OrderHeader h where h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
        "group by year(h.createdAt), month(h.createdAt) order by y, m")
    List<Object[]> sumByMonth(@Param("start") java.time.LocalDateTime start,
                  @Param("end") java.time.LocalDateTime end);

    // 指定用户的每月订单金额
    @Query("select year(h.createdAt) as y, month(h.createdAt) as m, sum(h.totalPrice) as total " +
        "from OrderHeader h where h.client = :client and h.createdAt between :start and :end and h.status = com.shopping.server.model.OrderStatus.PAID " +
        "group by year(h.createdAt), month(h.createdAt) order by y, m")
    List<Object[]> sumByMonthForClient(@Param("client") Client client,
                        @Param("start") java.time.LocalDateTime start,
                        @Param("end") java.time.LocalDateTime end);
}
