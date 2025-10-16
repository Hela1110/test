package com.shopping.server.model;

import lombok.Data;

import javax.persistence.*;
import java.time.LocalDateTime;

@Data
@Entity
@Table(name = "chat_messages")
public class ChatMessage {
	@Id
	@GeneratedValue(strategy = GenerationType.IDENTITY)
	private Long id;

	// 发送方用户名
	@Column(nullable = false, length = 64)
	private String fromUser;

	// 接收方用户名；为空表示群发/客服群
	@Column(length = 64)
	private String toUser;

	@Column(nullable = false, length = 2000)
	private String content;

	@Column(nullable = false)
	private LocalDateTime createdAt;
}
