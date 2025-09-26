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

            // 可选：自动确保 chat_messages 表已存在（仅当 probe.db.ensureChat=true）
            ensureChatMessagesTable();
        } catch (Exception e) {
            long cost = System.currentTimeMillis() - start;
            System.err.println("[DB-PROBE] FAILURE cost=" + cost + "ms -> " + e.getClass().getName() + ": " + e.getMessage());
            e.printStackTrace(System.err);
        }
    }

    @Value("${probe.db.ensureChat:false}")
    private boolean ensureChat;

    private void ensureChatMessagesTable() {
        if (!ensureChat) {
            System.out.println("[DB-PROBE] ensureChat=false, skip ensuring chat_messages table");
            return;
        }
        try {
            // 检查是否存在 chat_messages 表
            Query exists = entityManager.createNativeQuery("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = 'chat_messages'");
            Number cnt = (Number) exists.getSingleResult();
            if (cnt != null && cnt.longValue() > 0) {
                System.out.println("[DB-PROBE] chat_messages table exists.");
                return;
            }
            System.out.println("[DB-PROBE] chat_messages table not found, creating...");
            String ddl = "CREATE TABLE IF NOT EXISTS chat_messages (" +
                    "id BIGINT NOT NULL AUTO_INCREMENT, " +
                    "from_user VARCHAR(64) NOT NULL, " +
                    "to_user VARCHAR(64) NULL, " +
                    "content VARCHAR(2000) NOT NULL, " +
                    "created_at DATETIME(6) NOT NULL, " +
                    "PRIMARY KEY (id), " +
                    "INDEX idx_chat_created_at (created_at), " +
                    "INDEX idx_chat_from_to_created (from_user, to_user, created_at)" +
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci";
            entityManager.createNativeQuery(ddl).executeUpdate();
            System.out.println("[DB-PROBE] chat_messages table created.");
        } catch (Exception ex) {
            System.err.println("[DB-PROBE] Failed to ensure chat_messages table: " + ex.getMessage());
        }
    }
}
