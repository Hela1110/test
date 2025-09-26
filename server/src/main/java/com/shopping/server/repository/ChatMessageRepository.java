package com.shopping.server.repository;

import com.shopping.server.model.ChatMessage;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.time.LocalDateTime;
import java.util.List;

public interface ChatMessageRepository extends JpaRepository<ChatMessage, Long> {
	// 最近的全局消息
	@Query("select c from ChatMessage c where c.toUser is null order by c.createdAt desc")
	List<ChatMessage> findLatestGlobal(Pageable pageable);

	// 两个用户之间（双向）的最近消息
	@Query("select c from ChatMessage c where ((c.fromUser = :u1 and c.toUser = :u2) or (c.fromUser = :u2 and c.toUser = :u1)) order by c.createdAt desc")
	List<ChatMessage> findConversation(@Param("u1") String u1, @Param("u2") String u2, Pageable pageable);

	// 指定用户收到的最近消息
	@Query("select c from ChatMessage c where c.toUser = :to order by c.createdAt desc")
	List<ChatMessage> findLatestForUser(@Param("to") String to, Pageable pageable);

	// 删除某两个用户之间的所有消息
	long deleteByFromUserAndToUser(String fromUser, String toUser);

	// 删除所有公共（群聊）消息（toUser 为 null）
	long deleteByToUserIsNull();

	// 清理早于某时间的消息
	long deleteByCreatedAtBefore(LocalDateTime threshold);
}
