package com.shopping.server.repository;

import com.shopping.server.model.Client;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.Optional;

public interface ClientRepository extends JpaRepository<Client, Long> {
    Optional<Client> findByUsername(String username);
    boolean existsByUsername(String username);
    java.util.List<Client> findTop50ByOrderByClientIdDesc();
}
