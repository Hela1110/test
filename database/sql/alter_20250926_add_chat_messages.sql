-- 创建聊天消息表（如果不存在）
-- 目的：配合后端 ChatMessage 实体（表名 chat_messages）
-- 列与约束与 JPA 对齐（字段名使用 snake_case）

CREATE TABLE IF NOT EXISTS `chat_messages` (
  `id` BIGINT NOT NULL AUTO_INCREMENT,
  `from_user` VARCHAR(64) NOT NULL,
  `to_user` VARCHAR(64) NULL,
  `content` VARCHAR(2000) NOT NULL,
  `created_at` DATETIME(6) NOT NULL,
  PRIMARY KEY (`id`),
  INDEX `idx_chat_created_at` (`created_at`),
  INDEX `idx_chat_from_to_created` (`from_user`, `to_user`, `created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
