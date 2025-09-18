package com.shopping.server.repository;

import com.shopping.server.model.Order;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.time.LocalDateTime;

@Repository
public interface OrderRepository extends JpaRepository<Order, Long> {
    /**
     * 根据客户ID查找所有订单
     * @param clientId 客户ID
     * @return 订单列表
     */
    List<Order> findByClientClientId(Long clientId);
    
    /**
     * 根据客户ID和订单状态查找订单
     * @param clientId 客户ID
     * @param status 订单状态
     * @return 订单列表
     */
    List<Order> findByClientClientIdAndStatus(Long clientId, String status);
    
    /**
     * 根据订单状态查找订单
     * @param status 订单状态
     * @return 订单列表
     */
    List<Order> findByStatus(String status);
    
    /**
     * 根据时间范围查找订单
     * @param startTime 开始时间
     * @param endTime 结束时间
     * @return 订单列表
     */
    List<Order> findByOrderTimeBetween(LocalDateTime startTime, LocalDateTime endTime);
    
    /**
     * 根据客户ID和时间范围查找订单
     * @param clientId 客户ID
     * @param startTime 开始时间
     * @param endTime 结束时间
     * @return 订单列表
     */
    List<Order> findByClientClientIdAndOrderTimeBetween(Long clientId, LocalDateTime startTime, LocalDateTime endTime);
    
    /**
     * 根据客户ID和状态删除订单
     * @param clientId 客户ID
     * @param status 订单状态
     */
    void deleteByClientClientIdAndStatus(Long clientId, String status);
    
    /**
     * 统计客户的订单数量
     * @param clientId 客户ID
     * @return 订单数量
     */
    long countByClientClientId(Long clientId);
    
    /**
     * 查找客户特定状态的订单数量
     * @param clientId 客户ID
     * @param status 订单状态
     * @return 订单数量
     */
    long countByClientClientIdAndStatus(Long clientId, String status);
}