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
import java.nio.file.*;
import java.time.Instant;
import java.util.concurrent.atomic.AtomicLong;

@Component
@ChannelHandler.Sharable
public class SocketMessageHandler extends SimpleChannelInboundHandler<String> {
    private static final Map<String, ChannelHandlerContext> clientChannels = new ConcurrentHashMap<>();
    // 临时内存用户存储（演示用）：用户名 -> 明文密码
    // TODO: 替换为数据库存储，并对密码进行哈希（如 BCrypt）
    private static final Map<String, String> userStore = new ConcurrentHashMap<>();
    // 用户资料：username -> { phone: "", ... }
    private static final Map<String, Map<String,Object>> userProfiles = new ConcurrentHashMap<>();
    private final ObjectMapper objectMapper = new ObjectMapper();
    private static final ObjectMapper PERSIST = new ObjectMapper();
    // 简易内存购物车：用户名 -> 购物车条目列表
    private static final Map<String, List<Map<String, Object>>> carts = new ConcurrentHashMap<>();
    // 简易订单存储（演示用）：订单ID 自增 + 持久化文件
    private static final List<Map<String, Object>> orders = new CopyOnWriteArrayList<>();
    private static final AtomicLong ORDER_SEQ = new AtomicLong(1);
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
    CATALOG.add(product(1006, "机械键盘", 399.00));
    CATALOG.add(product(1007, "显示器 27寸", 1299.00));
    CATALOG.add(product(1008, "移动电源", 159.00));
    CATALOG.add(product(1009, "蓝牙耳机", 299.00));
    CATALOG.add(product(1010, "智能手表", 999.00));
    CATALOG.add(product(1011, "平板电脑", 2499.00));
    CATALOG.add(product(1012, "游戏手柄", 259.00));
    CATALOG.add(product(1013, "U 盘 128G", 89.00));
    CATALOG.add(product(1014, "移动硬盘 1T", 359.00));
    CATALOG.add(product(1015, "打印机", 699.00));
        // 尝试从本地文件恢复用户与购物车
        try { loadFromDisk(); } catch (Exception ignore) {}
    }

    // 简易重复请求抑制：每个连接，若在窗口期内收到完全相同的 raw 字符串，则忽略
    private static final long DEDUP_WINDOW_MS = 400; // 400ms 时间窗口
    private static final Map<ChannelHandlerContext, Map<String, Long>> recentByCtx = new ConcurrentHashMap<>();

    // 简易持久化（JSON 文件）
    private static final Path DATA_DIR = Paths.get("server-data");
    private static final Path USERS_FILE = DATA_DIR.resolve("users.json");
    private static final Path USERS_PROFILE_FILE = DATA_DIR.resolve("users_profile.json");
    private static final Path CARTS_FILE = DATA_DIR.resolve("carts.json");
    private static final Path ORDERS_FILE = DATA_DIR.resolve("orders.json");
    private static void ensureDataDir() throws Exception { if (!Files.exists(DATA_DIR)) Files.createDirectories(DATA_DIR); }
    private static synchronized void saveUsers() throws Exception { ensureDataDir(); PERSIST.writeValue(USERS_FILE.toFile(), userStore); }
    private static synchronized void saveProfiles() throws Exception { ensureDataDir(); PERSIST.writeValue(USERS_PROFILE_FILE.toFile(), userProfiles); }
    private static synchronized void saveCarts() throws Exception { ensureDataDir(); PERSIST.writeValue(CARTS_FILE.toFile(), carts); }
    private static synchronized void saveOrders() throws Exception { ensureDataDir(); PERSIST.writeValue(ORDERS_FILE.toFile(), orders); }
    private static synchronized void loadFromDisk() throws Exception {
        ensureDataDir();
        if (Files.exists(USERS_FILE)) {
            Map<String,String> m = PERSIST.readValue(USERS_FILE.toFile(), new TypeReference<Map<String,String>>(){});
            if (m != null) userStore.putAll(m);
        }
        if (Files.exists(USERS_PROFILE_FILE)) {
            Map<String, Map<String,Object>> m = PERSIST.readValue(USERS_PROFILE_FILE.toFile(), new TypeReference<Map<String, Map<String,Object>>>(){});
            if (m != null) userProfiles.putAll(m);
        }
        if (Files.exists(CARTS_FILE)) {
            Map<String, List<Map<String,Object>>> m = PERSIST.readValue(CARTS_FILE.toFile(), new TypeReference<Map<String, List<Map<String,Object>>>>(){});
            if (m != null) carts.putAll(m);
        }
        if (Files.exists(ORDERS_FILE)) {
            List<Map<String,Object>> m = PERSIST.readValue(ORDERS_FILE.toFile(), new TypeReference<List<Map<String,Object>>>(){});
            if (m != null) {
                orders.addAll(m);
                // 更新 ORDER_SEQ
                long maxId = 0;
                for (Map<String,Object> o : orders) {
                    Object oid = o.get("orderId");
                    if (oid instanceof Number) maxId = Math.max(maxId, ((Number) oid).longValue());
                }
                ORDER_SEQ.set(Math.max(ORDER_SEQ.get(), maxId + 1));
            }
        }
    }
    
    @Override
    protected void channelRead0(ChannelHandlerContext ctx, String msg) throws Exception {
        // 基本健壮性处理：忽略空行，打印原始消息便于排查
        if (msg == null) return;
        String raw = msg.trim();
        if (raw.isEmpty()) return;
        // 去重：相同 raw 在短时间窗口内仅处理一次
        long now = System.currentTimeMillis();
        Map<String, Long> recent = recentByCtx.computeIfAbsent(ctx, k -> new ConcurrentHashMap<>());
        Long lastTs = recent.get(raw);
        if (lastTs != null && (now - lastTs) < DEDUP_WINDOW_MS) {
            // 忽略重复消息，不再处理也不再打印
            return;
        }
        recent.put(raw, now);
        System.out.println("Received: " + raw);

        Map<String, Object> request;
        try {
            request = objectMapper.readValue(raw, new TypeReference<Map<String, Object>>() {});
        } catch (Exception parseEx) {
            System.out.println("Invalid JSON, error=" + parseEx.getMessage());
            Map<String, Object> resp = new HashMap<>();
            resp.put("type", "error");
            resp.put("code", 1002);
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
            case "get_products":
                handleGetProducts(ctx, request);
                break;
            case "search_products":
                handleSearchProducts(ctx, request);
                break;
            case "get_cart":
                handleGetCart(ctx, request);
                break;
            case "remove_from_cart":
                handleRemoveFromCart(ctx, request);
                break;
            case "set_cart_quantity":
                handleSetCartQuantity(ctx, request);
                break;
            case "checkout":
                handleCheckout(ctx, request);
                break;
            case "create_order":
                handleCreateOrder(ctx, request);
                break;
            case "get_orders":
                handleGetOrders(ctx, request);
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
            case "clear_cart":
                handleClearCart(ctx, request);
                break;
            case "get_account_info":
                handleGetAccountInfo(ctx, request);
                break;
            case "update_account_info":
                handleUpdateAccountInfo(ctx, request);
                break;
            // 添加更多消息处理类型
            default:
                System.out.println("Unknown message type: " + type);
                Map<String, Object> resp = new HashMap<>();
                resp.put("type", "error");
                resp.put("code", 1002);
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
        m.put("description", name + " 描述");
        m.put("image_url", "https://example.com/product/" + id + ".jpg");
        m.put("stock", 100);
        m.put("on_sale", false);
        return m;
    }

    private void handleGetProducts(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        int page = ((Number)request.getOrDefault("page", 1)).intValue();
        int size = ((Number)request.getOrDefault("size", 10)).intValue();
        if (page < 1) page = 1; if (size < 1) size = 10;
        int total = CATALOG.size();
        int from = Math.min((page - 1) * size, total);
        int to = Math.min(from + size, total);
        List<Map<String,Object>> slice = new CopyOnWriteArrayList<>(CATALOG.subList(from, to));
        // 映射到文档字段
        List<Map<String,Object>> products = new CopyOnWriteArrayList<>();
        for (Map<String,Object> p : slice) {
            Map<String,Object> it = new HashMap<>();
            it.put("id", p.get("product_id"));
            it.put("name", p.get("name"));
            it.put("price", p.get("price"));
            it.put("description", p.get("description"));
            it.put("imageUrl", p.get("image_url"));
            it.put("stock", p.get("stock"));
            products.add(it);
        }
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "products_response");
        resp.put("total", total);
        resp.put("products", products);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleSearchProducts(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String keyword = String.valueOf(request.getOrDefault("keyword", "")).trim().toLowerCase();
        List<Map<String,Object>> results = new CopyOnWriteArrayList<>();
        for (Map<String,Object> p : CATALOG) {
            String name = String.valueOf(p.get("name")).toLowerCase();
            if (keyword.isEmpty() || name.contains(keyword)) results.add(p);
        }
        List<Map<String,Object>> products = new CopyOnWriteArrayList<>();
        for (Map<String,Object> p : results) {
            Map<String,Object> it = new HashMap<>();
            it.put("id", p.get("product_id"));
            it.put("name", p.get("name"));
            it.put("price", p.get("price"));
            it.put("description", p.get("description"));
            it.put("imageUrl", p.get("image_url"));
            it.put("stock", p.get("stock"));
            products.add(it);
        }
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "products_response");
        resp.put("total", products.size());
        resp.put("products", products);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
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
        if (pid == null) pid = request.get("productId");
        if (pid == null) pid = 1001; // 默认一个
        int quantity = ((Number)request.getOrDefault("quantity", 1)).intValue();
        if (quantity < 1) quantity = 1;
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        // 查找商品
        Map<String, Object> found = null;
        for (Map<String, Object> p : CATALOG) {
            if (pid.equals(p.get("product_id"))) { found = p; break; }
        }
        boolean success = false;
        String message = null;
        int code = 0;
        if (found != null) {
            int stock = ((Number)found.getOrDefault("stock", 0)).intValue();
            if (stock <= 0) {
                // 无库存
                success = false;
                message = "库存不足";
                code = 2002;
            } else {
                boolean merged = false;
                for (Map<String, Object> it : list) {
                    if (pid.equals(it.get("product_id"))) {
                        int q = ((Number)it.getOrDefault("quantity", 1)).intValue();
                        int newQ = q + quantity;
                        if (newQ > stock) newQ = stock;
                        if (newQ == q) {
                            // 已达库存上限
                            success = false;
                            message = "库存不足";
                            code = 2002;
                        } else {
                            it.put("quantity", newQ);
                            success = true;
                        }
                        merged = true; break;
                    }
                }
                if (!merged) {
                    int addQ = Math.min(quantity, stock);
                    if (addQ <= 0) {
                        success = false;
                        message = "库存不足";
                        code = 2002;
                    } else {
                        Map<String, Object> item = new HashMap<>(found);
                        item.put("quantity", addQ);
                        list.add(item);
                        success = true;
                    }
                }
            }
        } else {
            message = "商品不存在";
        }
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "add_to_cart_response");
        resp.put("success", success && found != null);
        if (message != null) resp.put("message", message);
        if (code != 0) resp.put("code", code);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
        try { saveCarts(); } catch (Exception ignore) {}
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
        String phone = (String) request.getOrDefault("phone", "");

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
        // 初始化或更新用户资料
        Map<String,Object> prof = userProfiles.computeIfAbsent(username, k -> new ConcurrentHashMap<>());
        if (phone != null) prof.put("phone", phone);
        try { saveUsers(); } catch (Exception ignore) {}
        try { saveProfiles(); } catch (Exception ignore) {}

        response.put("success", true);
        response.put("message", "注册成功");
    ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
    }

    private void handleClearCart(ChannelHandlerContext ctx, Map<String,Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        list.clear();
        try { saveCarts(); } catch (Exception ignore) {}
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "clear_cart_response");
        resp.put("success", true);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
        // 回发最新购物车
        handleGetCart(ctx, request);
    }

    private void handleGetOrders(ChannelHandlerContext ctx, Map<String,Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        List<Map<String,Object>> myOrders = new CopyOnWriteArrayList<>();
        for (Map<String,Object> o : orders) {
            if (username != null && username.equals(o.get("username"))) myOrders.add(o);
        }
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "orders_response");
        resp.put("orders", myOrders);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleGetAccountInfo(ChannelHandlerContext ctx, Map<String,Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Map<String,Object> prof = (username!=null) ? userProfiles.getOrDefault(username, new HashMap<>()) : new HashMap<>();
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "account_info");
        resp.put("success", username != null);
        if (username != null) {
            resp.put("username", username);
            resp.put("phone", String.valueOf(prof.getOrDefault("phone", "")));
        } else {
            resp.put("message", "未登录");
        }
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleUpdateAccountInfo(ChannelHandlerContext ctx, Map<String,Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "update_account_response");
        if (username == null) {
            resp.put("success", false);
            resp.put("message", "未登录");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        String newPhone = (String) request.get("phone");
        String newPassword = (String) request.get("password");
        String newUsername = (String) request.get("new_username");
        // 更新手机号
        Map<String,Object> prof = userProfiles.computeIfAbsent(username, k -> new ConcurrentHashMap<>());
        if (newPhone != null) prof.put("phone", newPhone);
        // 更新密码
        if (newPassword != null && !newPassword.isEmpty()) {
            userStore.put(username, newPassword);
        }
        // 更新用户名（演示：直接改 key；实际生产中需迁移所有相关数据）
        if (newUsername != null && !newUsername.isEmpty() && !newUsername.equals(username)) {
            if (userStore.containsKey(newUsername)) {
                resp.put("success", false);
                resp.put("message", "新用户名已存在");
                ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
                return;
            }
            // 迁移用户存储
            String pwd = userStore.remove(username);
            if (pwd != null) userStore.put(newUsername, pwd);
            Map<String,Object> moved = userProfiles.remove(username);
            if (moved != null) userProfiles.put(newUsername, moved);
            List<Map<String,Object>> cart = carts.remove(username);
            if (cart != null) carts.put(newUsername, cart);
            // 订单中的 username 字段也迁移（演示用途）
            for (Map<String,Object> o : orders) if (username.equals(o.get("username"))) o.put("username", newUsername);
            username = newUsername;
        }
        try { saveUsers(); } catch (Exception ignore) {}
        try { saveProfiles(); } catch (Exception ignore) {}
        try { saveCarts(); } catch (Exception ignore) {}
        try { saveOrders(); } catch (Exception ignore) {}
        resp.put("success", true);
        resp.put("message", "账户信息已更新");
        resp.put("username", username);
        resp.put("phone", String.valueOf(userProfiles.getOrDefault(username, new HashMap<>()).getOrDefault("phone", "")));
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }
    
    private void handleGetCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        // 基于当前目录库存，构造带实时库存的 items
        List<Map<String,Object>> itemsWithStock = new CopyOnWriteArrayList<>();
        for (Map<String,Object> it : list) {
            Map<String,Object> m = new HashMap<>(it);
            Object pid = it.get("product_id");
            if (pid != null) {
                for (Map<String,Object> p : CATALOG) if (pid.equals(p.get("product_id"))) {
                    m.put("stock", p.getOrDefault("stock", 0));
                    break;
                }
            }
            itemsWithStock.add(m);
        }
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "cart_items");
        resp.put("items", itemsWithStock);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
        // 兼容文档：再发送一条 cart_response（字段名按文档）
        Map<String,Object> resp2 = new HashMap<>();
        resp2.put("type", "cart_response");
        List<Map<String,Object>> items = new CopyOnWriteArrayList<>();
        for (Map<String,Object> it : list) {
            Map<String,Object> m = new HashMap<>();
            Object pid = it.get("product_id");
            m.put("productId", pid);
            m.put("name", it.get("name"));
            m.put("price", it.get("price"));
            m.put("quantity", it.get("quantity"));
            // 附带当前库存
            int stock = 0;
            if (pid != null) {
                for (Map<String,Object> p : CATALOG) if (pid.equals(p.get("product_id"))) { stock = ((Number)p.getOrDefault("stock", 0)).intValue(); break; }
            }
            m.put("stock", stock);
            items.add(m);
        }
        resp2.put("items", items);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp2) + "\n");
    }

    private void handleRemoveFromCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object pid = request.get("product_id");
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        if (pid != null) {
            list.removeIf(it -> pid.equals(it.get("product_id")));
        }
        // 返回最新购物车
        try { saveCarts(); } catch (Exception ignore) {}
        handleGetCart(ctx, request);
    }

    private void handleSetCartQuantity(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object pid = request.get("product_id");
        if (pid == null) pid = request.get("productId");
        int quantity = ((Number)request.getOrDefault("quantity", 1)).intValue();
        if (quantity < 0) quantity = 0;
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        boolean found = false;
        for (Map<String,Object> it : list) {
            if (pid != null && pid.equals(it.get("product_id"))) {
                found = true;
                // 依据当前库存进行夹取
                int stock = 0;
                for (Map<String,Object> p : CATALOG) if (pid.equals(p.get("product_id"))) { stock = ((Number)p.getOrDefault("stock", 0)).intValue(); break; }
                if (stock > 0) quantity = Math.min(quantity, stock); else quantity = 0;
                if (quantity == 0) {
                    // 等价于删除
                    list.remove(it);
                } else {
                    it.put("quantity", quantity);
                }
                break;
            }
        }
        // 若未找到且数量>0，可选地新增（这里选择：若未找到则按当前目录添加一条）
        if (!found && quantity > 0 && pid != null) {
            for (Map<String,Object> p : CATALOG) {
                if (pid.equals(p.get("product_id"))) {
                    int stock = ((Number)p.getOrDefault("stock", 0)).intValue();
                    int addQ = Math.min(quantity, stock);
                    if (addQ > 0) {
                        Map<String,Object> item = new HashMap<>(p);
                        item.put("quantity", addQ);
                        list.add(item);
                    }
                    break;
                }
            }
        }
        try { saveCarts(); } catch (Exception ignore) {}
        handleGetCart(ctx, request);
    }
    
    private void handleCheckout(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        List<Map<String, Object>> list = carts.computeIfAbsent(username == null ? "__anon__" : username, k -> new CopyOnWriteArrayList<>());
        boolean ok = !list.isEmpty();
        long orderId = -1;
        double totalPrice = 0.0;
        if (ok) {
            // 校验库存
            for (Map<String,Object> it : list) {
                int pid = ((Number)it.get("product_id")).intValue();
                int qty = ((Number)it.getOrDefault("quantity",1)).intValue();
                Map<String,Object> found = null;
                for (Map<String,Object> p : CATALOG) if (((Number)p.get("product_id")).intValue() == pid) { found = p; break; }
                if (found == null) { ok = false; break; }
                int stock = ((Number)found.getOrDefault("stock", 0)).intValue();
                if (qty > stock) { ok = false; break; }
            }
        }
        if (ok) {
            // 扣减库存并创建订单，随后清空购物车
            List<Map<String,Object>> items = new CopyOnWriteArrayList<>();
            for (Map<String,Object> it : list) {
                int pid = ((Number)it.get("product_id")).intValue();
                int qty = ((Number)it.getOrDefault("quantity",1)).intValue();
                Map<String,Object> found = null;
                for (Map<String,Object> p : CATALOG) if (((Number)p.get("product_id")).intValue() == pid) { found = p; break; }
                if (found == null) continue;
                int stock = ((Number)found.getOrDefault("stock", 0)).intValue();
                found.put("stock", Math.max(0, stock - qty));
                Map<String,Object> m = new HashMap<>();
                m.put("productId", pid);
                m.put("quantity", qty);
                items.add(m);
                totalPrice += ((Number)it.get("price")).doubleValue() * qty;
            }
            orderId = ORDER_SEQ.getAndIncrement();
            Map<String,Object> order = new HashMap<>();
            order.put("orderId", orderId);
            order.put("username", username);
            order.put("items", items);
            order.put("total_price", totalPrice);
            order.put("status", "CREATED");
            order.put("order_time", Instant.now().toString());
            orders.add(order);
            list.clear();
            try { saveOrders(); } catch (Exception ignore) {}
        }
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "checkout_response");
        resp.put("success", ok);
        resp.put("message", ok ? "结算成功" : (list.isEmpty()? "购物车为空" : "库存不足"));
        if (!ok && !list.isEmpty()) { resp.put("code", 2002); }
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
        // 兼容文档：发送 order_response
        Map<String,Object> resp2 = new HashMap<>();
        resp2.put("type", "order_response");
        resp2.put("success", ok);
        if (ok) resp2.put("orderId", orderId);
        resp2.put("message", ok ? "订单创建成功" : (list.isEmpty()? "购物车为空" : "库存不足"));
        if (!ok) { resp2.put("code", list.isEmpty()? 3001 : 2002); }
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp2) + "\n");
        try { saveCarts(); } catch (Exception ignore) {}
    }

    private void handleCreateOrder(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object rawItems = request.get("items");
        List<Map<String,Object>> reqItems = new CopyOnWriteArrayList<>();
        if (rawItems instanceof List<?>) {
            for (Object o : (List<?>) rawItems) {
                if (o instanceof Map<?,?>) {
                    @SuppressWarnings("unchecked")
                    Map<String,Object> m = new HashMap<>((Map<String,Object>) o);
                    reqItems.add(m);
                }
            }
        }
        boolean ok = !reqItems.isEmpty();
        long orderId = -1; double total = 0.0;
        if (ok) {
            // 校验并预检查库存
            for (Map<String,Object> it : reqItems) {
                Object pid = it.get("productId");
                int qty = ((Number)it.getOrDefault("quantity", 1)).intValue();
                Map<String,Object> found = null;
                for (Map<String,Object> p : CATALOG) if (pid != null && pid.equals(p.get("product_id"))) { found = p; break; }
                if (found == null || qty < 1) { ok = false; break; }
                int stock = ((Number)found.getOrDefault("stock", 0)).intValue();
                if (qty > stock) { ok = false; break; }
                total += ((Number)found.get("price")).doubleValue() * qty;
            }
        }
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "order_response");
        resp.put("success", ok);
        if (ok) {
            orderId = ORDER_SEQ.getAndIncrement();
            Map<String,Object> order = new HashMap<>();
            order.put("orderId", orderId);
            order.put("username", username);
            order.put("items", reqItems);
            order.put("total_price", total);
            order.put("status", "CREATED");
            order.put("order_time", Instant.now().toString());
            orders.add(order);
            try { saveOrders(); } catch (Exception ignore) {}
            // 扣减库存
            for (Map<String,Object> it : reqItems) {
                Object pid = it.get("productId");
                int qty = ((Number)it.getOrDefault("quantity", 1)).intValue();
                for (Map<String,Object> p : CATALOG) if (pid != null && pid.equals(p.get("product_id"))) {
                    int stock = ((Number)p.getOrDefault("stock", 0)).intValue();
                    p.put("stock", Math.max(0, stock - qty));
                    break;
                }
            }
            resp.put("orderId", orderId);
            resp.put("message", "订单创建成功");
        } else {
            // 失败原因：为空(3002通用)或库存不足(2002)
            boolean empty = reqItems.isEmpty();
            resp.put("code", empty ? 3002 : 2002);
            resp.put("message", empty ? "订单创建失败" : "库存不足");
        }
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
        recentByCtx.remove(ctx);
    }
    
    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
        cause.printStackTrace();
        ctx.close();
    }
}