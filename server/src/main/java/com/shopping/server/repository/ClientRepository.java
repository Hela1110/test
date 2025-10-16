package com.shopping.server.repository;

import com.shopping.server.model.Client;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.Query;

import java.util.Optional;

public interface ClientRepository extends JpaRepository<Client, Long> {
    Optional<Client> findByUsername(String username);
    boolean existsByUsername(String username);
    boolean existsByPhone(String phone);
    boolean existsByEmail(String email);
    java.util.List<Client> findTop50ByOrderByClientIdDesc();

    @Query("select c from Client c where lower(c.username) like lower(concat('%', ?1, '%')) or lower(c.email) like lower(concat('%', ?1, '%')) or lower(c.phone) like lower(concat('%', ?1, '%'))")
    Page<Client> searchByKeyword(String keyword, Pageable pageable);

    @Query("select count(c) from Client c where lower(c.username) like lower(concat('%', ?1, '%')) or lower(c.email) like lower(concat('%', ?1, '%')) or lower(c.phone) like lower(concat('%', ?1, '%'))")
    long countByKeyword(String keyword);
}
