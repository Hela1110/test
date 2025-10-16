package com.shopping.server.repository;

import com.shopping.server.model.Product;
import com.shopping.server.model.ProductType;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface ProductTypeRepository extends JpaRepository<ProductType, Long> {
    List<ProductType> findByProduct(Product product);
    List<ProductType> findByTypeName(String typeName);
    List<ProductType> findByTypeNameAndProduct(String typeName, Product product);
}
