package com.shopping.server.socket;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import org.springframework.stereotype.Component;

import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.List;

@Component
@ChannelHandler.Sharable
public class SocketMessageHandler extends SimpleChannelInboundHandler<String> {
    private static final Map<String, ChannelHandlerContext> clientChannels = new ConcurrentHashMap<>();
    // 临时内存用户存储（演示用）：用户名 -> 明文密码
    // TODO: 替换为数据库存储，并对密码进行哈希（如 BCrypt）
    private static final Map<String, String> userStore = new ConcurrentHashMap<>();
    private final ObjectMapper objectMapper = new ObjectMapper();
    // 简易内存购物车：用户名 -> 购物车条目列表
    private static final Map<String, List<Map<String, Object>>> carts = new ConcurrentHashMap<>();
    // 简易商品库（内存静态数据）
    private static final List<Map<String, Object>> CATALOG = new CopyOnWriteArrayList<>();

    static {
        // 预置一个默认测试账户（仅用于开发调试）
        // TODO: 生产环境请移除，改为数据库初始化并使用密码哈希
        userStore.putIfAbsent("admin", "123456");
        // 预置一些商品
        CATALOG.add(product(1001, "苹果 iPhone", 5999.00));
        CATALOG.add(product(1002, "小米 手机", 1999.00));
        CATALOG.add(product(1003, "华为 Mate", 4999.00));
        CATALOG.add(product(1004, "联想 笔记本", 6999.00));
        CATALOG.add(product(1005, "罗技 鼠标", 199.00));
    }
    
    @Override
    protected void channelRead0(ChannelHandlerContext ctx, String msg) throws Exception {
        // 基本健壮性处理：忽略空行，打印原始消息便于排查
        if (msg == null) return;
        String raw = msg.trim();
        if (raw.isEmpty()) return;
        System.out.println("Received: " + raw);

        Map<String, Object> request;
        try {
            request = objectMapper.readValue(raw, new TypeReference<Map<String, Object>>() {});
        } catch (Exception parseEx) {
            System.out.println("Invalid JSON, error=" + parseEx.getMessage());
            Map<String, Object> resp = new HashMap<>();
            resp.put("type", "error");
            resp.put("message", "Invalid JSON");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return; // 不关闭连接，允许客户端继续发送
        }

        String type = (String) request.get("type");

        switch (type) {
            case "login":
                handleLogin(ctx, request);
                break;
            case "register":
                handleRegister(ctx, request);
                break;
            case "get_cart":
                handleGetCart(ctx, request);
                break;
            case "remove_from_cart":
                handleRemoveFromCart(ctx, request);
                break;
            case "checkout":
                handleCheckout(ctx, request);
                break;
            case "get_carousel":
                handleGetCarousel(ctx);
                break;
            case "get_recommendations":
                handleGetRecommendations(ctx);
                break;
            case "get_promotions":
                handleGetPromotions(ctx);
                break;
            case "search":
                handleSearch(ctx, request);
                break;
            case "get_product_detail":
                handleGetProductDetail(ctx, request);
                break;
            case "add_to_cart":
                handleAddToCart(ctx, request);
                break;
            // 添加更多消息处理类型
            default:
                System.out.println("Unknown message type: " + type);
                Map<String, Object> resp = new HashMap<>();
                resp.put("type", "error");
                resp.put("message", "Unknown message type: " + type);
                ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
        }
    }

    private static Map<String, Object> product(int id, String name, double price) {
        Map<String, Object> m = new HashMap<>();
        m.put("product_id", id);
        m.put("name", name);
        m.put("price", price);
        m.put("quantity", 1);
        return m;
    }
    
    private void handleLogin(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.get("username");
        String password = (String) request.get("password");

        Map<String, Object> response = new HashMap<>();
        response.put("type", "login_response");

        // 简单校验：如果已注册则校验密码；未注册则失败（演示用）
        if (username == null || username.isEmpty() || password == null || password.isEmpty()) {
            response.put("success", false);
            response.put("message", "用户名或密码不能为空");
        } else {
            String saved = userStore.get(username);
            if (saved != null && saved.equals(password)) {
                response.put("success", true);
                response.put("message", "登录成功");
                clientChannels.put(username, ctx);
            } else {
                response.put("success", false);
                response.put("message", saved == null ? "用户不存在，请先注册" : "密码错误");
            }
        }

    ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
    }

    private void handleGetCarousel(ChannelHandlerContext ctx) throws Exception {
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "carousel_data");
        List<Map<String, Object>> images = new CopyOnWriteArrayList<>();
        images.add(mapOf("title", "大促海报1", "url", "https://example.com/banner1.jpg"));
        images.add(mapOf("title", "大促海报2", "url", "https://example.com/banner2.jpg"));
        images.add(mapOf("title", "新品上市", "url", "https://example.com/banner3.jpg"));
        resp.put("images", images);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleGetRecommendations(ChannelHandlerContext ctx) throws Exception {
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "recommendations");
        // 简单取前 4 个
        resp.put("products", CATALOG.subList(0, Math.min(4, CATALOG.size())));
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleGetPromotions(ChannelHandlerContext ctx) throws Exception {
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "promotions");
        List<Map<String, Object>> promos = new CopyOnWriteArrayList<>();
        promos.add(mapOf("title", "开学季", "desc", "全场满 1000 减 100"));
        promos.add(mapOf("title", "会员日", "desc", "PLUS 额外 95 折"));
        resp.put("promotions", promos);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleSearch(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String keyword = (String) request.getOrDefault("keyword", "");
        String kw = keyword == null ? "" : keyword.trim().toLowerCase();
        List<Map<String, Object>> results = new CopyOnWriteArrayList<>();
        for (Map<String, Object> p : CATALOG) {
            String name = String.valueOf(p.get("name")).toLowerCase();
            if (kw.isEmpty() || name.contains(kw)) results.add(p);
        }
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "search_results");
        resp.put("results", results);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleGetProductDetail(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        Object pid = request.get("product_id");
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "product_detail");
        Map<String, Object> product = null;
        if (pid != null) {
            for (Map<String, Object> p : CATALOG) {
                if (pid.equals(p.get("product_id"))) { product = p; break; }
            }
        }
        if (product == null && !CATALOG.isEmpty()) product = CATALOG.get(0);
        resp.put("product", product);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleAddToCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object pid = request.get("product_id");
        if (pid == null) pid = 1001; // 默认一个
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        // 查找商品
        Map<String, Object> found = null;
        for (Map<String, Object> p : CATALOG) {
            if (pid.equals(p.get("product_id"))) { found = p; break; }
        }
        if (found != null) {
            // 如果已存在则数量+1
            boolean merged = false;
            for (Map<String, Object> it : list) {
                if (pid.equals(it.get("product_id"))) {
                    int q = ((Number)it.getOrDefault("quantity", 1)).intValue();
                    it.put("quantity", q+1);
                    merged = true; break;
                }
            }
            if (!merged) {
                Map<String, Object> item = new HashMap<>(found);
                item.put("quantity", 1);
                list.add(item);
            }
        }
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "add_to_cart_response");
        resp.put("success", found != null);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private Map<String, Object> mapOf(Object... kv) {
        Map<String, Object> m = new HashMap<>();
        for (int i=0;i+1<kv.length;i+=2) {
            m.put(String.valueOf(kv[i]), kv[i+1]);
        }
        return m;
    }

    private void handleRegister(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.get("username");
        String password = (String) request.get("password");

        Map<String, Object> response = new HashMap<>();
        response.put("type", "register_response");

        // 基本校验
        if (username == null || username.isEmpty() || password == null || password.isEmpty()) {
            response.put("success", false);
            response.put("message", "用户名和密码不能为空");
            ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
            return;
        }

        // 简单规则限制（可扩展为更复杂校验）
        if (username.length() < 3 || username.length() > 32) {
            response.put("success", false);
            response.put("message", "用户名长度需在 3-32 之间");
            ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
            return;
        }
        if (password.length() < 6 || password.length() > 64) {
            response.put("success", false);
            response.put("message", "密码长度需在 6-64 之间");
            ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
            return;
        }

        // 检查重复
        if (userStore.containsKey(username)) {
            response.put("success", false);
            response.put("message", "用户名已存在");
            ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
            return;
        }

        // TODO: 生产环境请对密码进行哈希存储
        userStore.put(username, password);

        response.put("success", true);
        response.put("message", "注册成功");
    ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
    }
    
    private void handleGetCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "cart_items");
        resp.put("items", list);
    ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleRemoveFromCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object pid = request.get("product_id");
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        if (pid != null) {
            list.removeIf(it -> pid.equals(it.get("product_id")));
        }
        // 返回最新购物车
        handleGetCart(ctx, request);
    }
    
    private void handleCheckout(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        boolean ok = !list.isEmpty();
        if (ok) list.clear();
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "checkout_response");
        resp.put("success", ok);
        resp.put("message", ok ? "结算成功" : "购物车为空");
    ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private String findUsernameByCtx(ChannelHandlerContext ctx) {
        for (Map.Entry<String, ChannelHandlerContext> e : clientChannels.entrySet()) {
            if (e.getValue() == ctx) return e.getKey();
        }
        return null;
    }
    
    @Override
    public void channelActive(ChannelHandlerContext ctx) {
        System.out.println("Client connected: " + ctx.channel().remoteAddress());
    }
    
    @Override
    public void channelInactive(ChannelHandlerContext ctx) {
        System.out.println("Client disconnected: " + ctx.channel().remoteAddress());
        // 清理连接对应的用户映射
        clientChannels.entrySet().removeIf(e -> e.getValue() == ctx);
    }
    
    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
        cause.printStackTrace();
        ctx.close();
    }
}