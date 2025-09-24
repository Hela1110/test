package com.shopping.server.repository;

import com.shopping.server.model.OrderHeader;
import com.shopping.server.model.Client;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;

public interface OrderHeaderRepository extends JpaRepository<OrderHeader, Long> {
    List<OrderHeader> findByClient(Client client);
}
