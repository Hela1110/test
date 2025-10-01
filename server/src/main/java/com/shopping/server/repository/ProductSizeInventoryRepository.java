package com.shopping.server.repository;

import com.shopping.server.model.Product;
import com.shopping.server.model.ProductSizeInventory;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;

public interface ProductSizeInventoryRepository extends JpaRepository<ProductSizeInventory, Long> {
    List<ProductSizeInventory> findByProduct(Product product);
    Optional<ProductSizeInventory> findByProductAndSize(Product product, Integer size);
}
