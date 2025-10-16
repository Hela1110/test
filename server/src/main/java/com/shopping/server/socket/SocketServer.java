package com.shopping.server.socket;

import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.LineBasedFrameDecoder;
import io.netty.handler.codec.string.StringDecoder;
import io.netty.handler.codec.string.StringEncoder;
import java.nio.charset.StandardCharsets;
import org.springframework.stereotype.Component;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;

import javax.annotation.PostConstruct;
import javax.annotation.PreDestroy;

@Component
public class SocketServer {
    @Value("${socket.port:9090}")
    private int socketPort; // 可配置独立于 HTTP server.port 的 Netty 端口
    private EventLoopGroup bossGroup;
    private EventLoopGroup workerGroup;
    private final SocketMessageHandler socketMessageHandler;
    
    @Autowired
    public SocketServer(SocketMessageHandler socketMessageHandler) {
        this.socketMessageHandler = socketMessageHandler;
    }
    
    @PostConstruct
    public void start() throws Exception {
        bossGroup = new NioEventLoopGroup();
        workerGroup = new NioEventLoopGroup();
        
        try {
            ServerBootstrap b = new ServerBootstrap();
            b.group(bossGroup, workerGroup)
                .channel(NioServerSocketChannel.class)
                .childHandler(new ChannelInitializer<SocketChannel>() {
                    @Override
                    protected void initChannel(SocketChannel ch) {
                        // 按行分帧（客户端以'\n'作为消息分隔），最大单帧 1MB，避免 JSON 粘包/半包带来的解码干扰
                        ch.pipeline().addLast(
                            new LineBasedFrameDecoder(1024 * 1024),
                            new StringDecoder(StandardCharsets.UTF_8),
                            new StringEncoder(StandardCharsets.UTF_8),
                            socketMessageHandler
                        );
                    }
                })
                .option(ChannelOption.SO_BACKLOG, 128)
                .childOption(ChannelOption.SO_KEEPALIVE, true);
            
            b.bind(socketPort).sync();
            System.out.println("Socket server started on port " + socketPort);
        } catch (Exception e) {
            e.printStackTrace();
            shutdown();
        }
    }
    
    @PreDestroy
    public void shutdown() {
        if (bossGroup != null) {
            bossGroup.shutdownGracefully();
        }
        if (workerGroup != null) {
            workerGroup.shutdownGracefully();
        }
    }
}