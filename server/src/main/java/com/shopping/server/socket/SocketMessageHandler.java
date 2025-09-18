package com.shopping.server.socket;

import com.fasterxml.jackson.databind.ObjectMapper;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import org.springframework.stereotype.Component;

import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;

@Component
@ChannelHandler.Sharable
public class SocketMessageHandler extends SimpleChannelInboundHandler<String> {
    private static final Map<String, ChannelHandlerContext> clientChannels = new ConcurrentHashMap<>();
    private final ObjectMapper objectMapper = new ObjectMapper();
    
    @Override
    protected void channelRead0(ChannelHandlerContext ctx, String msg) throws Exception {
        Map<String, Object> request = objectMapper.readValue(msg, Map.class);
        String type = (String) request.get("type");
        
        switch (type) {
            case "login":
                handleLogin(ctx, request);
                break;
            case "get_cart":
                handleGetCart(ctx, request);
                break;
            case "checkout":
                handleCheckout(ctx, request);
                break;
            // 添加更多消息处理类型
            default:
                System.out.println("Unknown message type: " + type);
        }
    }
    
    private void handleLogin(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.get("username");
        String password = (String) request.get("password");
        
        // TODO: 实现登录验证逻辑
        
        Map<String, Object> response = new HashMap<>();
        response.put("type", "login_response");
        response.put("success", true);
        
        ctx.writeAndFlush(objectMapper.writeValueAsString(response));
    }
    
    private void handleGetCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        // TODO: 实现获取购物车逻辑
    }
    
    private void handleCheckout(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        // TODO: 实现结算逻辑
    }
    
    @Override
    public void channelActive(ChannelHandlerContext ctx) {
        System.out.println("Client connected: " + ctx.channel().remoteAddress());
    }
    
    @Override
    public void channelInactive(ChannelHandlerContext ctx) {
        System.out.println("Client disconnected: " + ctx.channel().remoteAddress());
    }
    
    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
        cause.printStackTrace();
        ctx.close();
    }
}