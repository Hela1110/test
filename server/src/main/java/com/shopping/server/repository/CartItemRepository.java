package com.shopping.server.repository;

import com.shopping.server.model.CartItem;
import com.shopping.server.model.Client;
import com.shopping.server.model.Product;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;

public interface CartItemRepository extends JpaRepository<CartItem, Long> {
    List<CartItem> findByClient(Client client);
    Optional<CartItem> findByClientAndProduct(Client client, Product product);
    void deleteByClientAndProduct_ClientIdAndProduct_ProductId(Long clientId, Long productId);
    void deleteByClient(Client client);
}
