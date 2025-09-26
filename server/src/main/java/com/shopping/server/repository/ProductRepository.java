package com.shopping.server.repository;

import com.shopping.server.model.Product;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.math.BigDecimal;
import java.util.List;

@Repository
public interface ProductRepository extends JpaRepository<Product, Long> {
    /**
     * 查找所有促销商品
     * @return 促销商品列表
     */
    List<Product> findByOnSaleTrue();
    
    /**
     * 根据价格范围查找商品
     * @param minPrice 最低价格
     * @param maxPrice 最高价格
     * @return 商品列表
     */
    List<Product> findByPriceBetween(BigDecimal minPrice, BigDecimal maxPrice);
    
    /**
     * 查找库存低于指定数量的商品
     * @param stockThreshold 库存阈值
     * @return 商品列表
     */
    List<Product> findByStockLessThan(Integer stockThreshold);
    
    /**
     * 模糊搜索商品名称
     * @param name 商品名称关键字
     * @return 商品列表
     */
    List<Product> findByNameContainingIgnoreCase(String name);
    
    /**
     * 查找指定价格范围内的促销商品
     * @param minPrice 最低价格
     * @param maxPrice 最高价格
     * @return 商品列表
     */
    List<Product> findByOnSaleTrueAndPriceBetween(BigDecimal minPrice, BigDecimal maxPrice);
    
    /**
     * 查找有库存的商品
     * @return 商品列表
     */
    List<Product> findByStockGreaterThan(Integer stock);
    
    /**
     * 自定义查询：查找打折力度最大的商品（按降序），使用分页限制条数。
     * 使用 Pageable 传入 PageRequest.of(0, limit) 来限制返回数量。
     * @param pageable 分页与排序（仅页大小与页码生效，排序在查询里已固定）
     * @return 商品列表
     */
    @Query("SELECT p FROM Product p " +
           "WHERE p.onSale = true " +
           "ORDER BY (p.price - p.discountPrice) / p.price DESC")
    List<Product> findTopDiscountProducts(Pageable pageable);
    
    /**
     * 自定义查询：搜索商品（支持名称和描述）
     * @param keyword 搜索关键词
     * @return 商品列表
     */
    @Query("SELECT p FROM Product p WHERE " +
           "LOWER(p.name) LIKE LOWER(CONCAT('%', :keyword, '%')) OR " +
           "LOWER(p.description) LIKE LOWER(CONCAT('%', :keyword, '%'))")
    List<Product> searchProducts(@Param("keyword") String keyword);
    
    /**
     * 统计促销商品数量
     * @return 促销商品数量
     */
    long countByOnSaleTrue();
    
    /**
     * 统计库存不足的商品数量
     * @param stockThreshold 库存阈值
     * @return 库存不足的商品数量
     */
    long countByStockLessThan(Integer stockThreshold);

       // 最新上架（按 productId 最大）
       Product findTopByOrderByProductIdDesc();
    
       @Query("SELECT MAX(p.productId) FROM Product p")
       Long findMaxProductId();

       // ========== 自定义分页排序查询 ==========
       // 销量倒序
       @Query("SELECT p FROM Product p ORDER BY p.sales DESC, p.price ASC")
       List<Product> findAllOrderBySalesDesc(Pageable pageable);

       // 价格升序（有效价格：onSale 且 discountPrice < price 时取 discountPrice，否则取 price）
       @Query("SELECT p FROM Product p ORDER BY CASE WHEN p.onSale = true AND p.discountPrice IS NOT NULL AND p.discountPrice < p.price THEN p.discountPrice ELSE p.price END ASC, p.sales DESC")
       List<Product> findAllOrderByEffectivePriceAsc(Pageable pageable);

       // 价格降序（有效价格）
       @Query("SELECT p FROM Product p ORDER BY CASE WHEN p.onSale = true AND p.discountPrice IS NOT NULL AND p.discountPrice < p.price THEN p.discountPrice ELSE p.price END DESC, p.sales DESC")
       List<Product> findAllOrderByEffectivePriceDesc(Pageable pageable);

       // 打折优先 -> 有折扣的在前，其次按有效价格升序，其次销量降序
       @Query("SELECT p FROM Product p ORDER BY CASE WHEN p.onSale = true AND p.discountPrice IS NOT NULL AND p.discountPrice < p.price THEN 0 ELSE 1 END ASC, CASE WHEN p.onSale = true AND p.discountPrice IS NOT NULL AND p.discountPrice < p.price THEN p.discountPrice ELSE p.price END ASC, p.sales DESC")
       List<Product> findAllDiscountFirst(Pageable pageable);
}