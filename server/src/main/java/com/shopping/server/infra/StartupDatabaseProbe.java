package com.shopping.server.infra;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

import javax.persistence.EntityManager;
import javax.persistence.PersistenceContext;
import javax.persistence.Query;

/**
 * 在应用启动后主动探测数据库可用性，便于快速区分：
 *  - 是数据库连接/权限问题 还是 JPA 自动配置缺失
 *  - 打印当前数据库与简单查询耗时
 * 可通过 application.yml: probe.db=false 或启动参数 --probe.db=false 关闭。
 */
@Component
public class StartupDatabaseProbe implements CommandLineRunner {

    @Value("${probe.db:true}")
    private boolean enabled;

    @PersistenceContext
    private EntityManager entityManager;

    @Override
    public void run(String... args) {
        if (!enabled) {
            System.out.println("[DB-PROBE] Disabled by configuration (probe.db=false)");
            return;
        }
        long start = System.currentTimeMillis();
        try {
            // 简单执行一条最轻量 SQL；对 MySQL 来说 "SELECT 1" 即可
            Query q = entityManager.createNativeQuery("SELECT 1");
            Object single = q.getSingleResult();
            long cost = System.currentTimeMillis() - start;
            System.out.println("[DB-PROBE] SUCCESS result=" + single + " cost=" + cost + "ms dialect=" + entityManager.getEntityManagerFactory().getProperties().get("hibernate.dialect"));
        } catch (Exception e) {
            long cost = System.currentTimeMillis() - start;
            System.err.println("[DB-PROBE] FAILURE cost=" + cost + "ms -> " + e.getClass().getName() + ": " + e.getMessage());
            e.printStackTrace(System.err);
        }
    }
}
