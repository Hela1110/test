package com.shopping.server.bootstrap;

import com.shopping.server.model.OrderHeader;
import com.shopping.server.model.OrderItem;
import com.shopping.server.model.StockMovement;
import com.shopping.server.repository.OrderHeaderRepository;
import com.shopping.server.repository.StockMovementRepository;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.List;

/**
 * 一次性历史数据回填：把已有订单（在引入 StockMovement 之前产生）转换为 ORDER_DEDUCT 流水。
 * 仅在满足条件时执行一次：
 *  1. 配置项 shopping.backfill.stock-movement.enabled=true （默认 true 可自动执行一次）；
 *  2. 当前 stock_movement 表为空（防止重复）；
 *  3. 存在至少一条订单记录。
 * 填充逻辑：对每个订单的每个 OrderItem 生成一条负数 delta 的流水（不改变当前库存，仅补齐审计轨迹）。
 * 注意：历史库存的真实调整（比如管理员手动改过）无法完全恢复，只能记录“推断的扣减”。
 */
@Component
public class StockMovementBackfill {

    private final OrderHeaderRepository orderHeaderRepository;
    private final StockMovementRepository stockMovementRepository;

    @Value("${shopping.backfill.stock-movement.enabled:true}")
    private boolean enabled;

    private volatile boolean done = false; // 防止多次执行

    public StockMovementBackfill(OrderHeaderRepository orderHeaderRepository,
                                 StockMovementRepository stockMovementRepository) {
        this.orderHeaderRepository = orderHeaderRepository;
        this.stockMovementRepository = stockMovementRepository;
    }

    @EventListener(ApplicationReadyEvent.class)
    @Transactional
    public void backfillIfNeeded() {
        if (!enabled || done) return;
        long existing = stockMovementRepository.count();
        if (existing > 0) { done = true; return; }
        long orderCount = orderHeaderRepository.count();
        if (orderCount == 0) { done = true; return; }
        List<OrderHeader> orders = orderHeaderRepository.findAll();
        if (orders.isEmpty()) { done = true; return; }

        // 简单防重复：如果订单里产品ID+尺码+数量组合完全重复过多次，有可能已经有流水，但因为表为空可以直接生成
        int created = 0;
        for (OrderHeader oh : orders) {
            if (oh.getItems()==null) continue;
            for (OrderItem item : oh.getItems()) {
                if (item.getProduct()==null || item.getQuantity()==null) continue;
                StockMovement sm = new StockMovement();
                sm.setProduct(item.getProduct());
                sm.setSize(item.getSize()==null?0:item.getSize());
                sm.setDelta(-Math.abs(item.getQuantity()));
                sm.setAction("ORDER_DEDUCT");
                sm.setOrderId(oh.getId());
                sm.setOperator("backfill");
                // createdAt 设为订单创建时间，若为空用当前时间
                sm.setCreatedAt(oh.getCreatedAt()==null? LocalDateTime.now(): oh.getCreatedAt());
                stockMovementRepository.save(sm);
                created++;
            }
        }
        done = true;
        System.out.println("[BACKFILL] StockMovement backfill finished. created=" + created);
    }
}
