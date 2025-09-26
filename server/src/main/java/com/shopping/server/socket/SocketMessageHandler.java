package com.shopping.server.socket;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import org.springframework.stereotype.Component;
import org.springframework.beans.factory.annotation.Autowired;

import com.shopping.server.repository.ClientRepository;
import com.shopping.server.repository.ProductRepository;
import com.shopping.server.repository.OrderHeaderRepository;
import com.shopping.server.service.OrderProcessingService;
import com.shopping.server.model.*;

import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.List;
import java.nio.file.*;
import java.util.concurrent.atomic.AtomicLong;

@Component
@ChannelHandler.Sharable
public class SocketMessageHandler extends SimpleChannelInboundHandler<String> {
    // === 注入的持久化层 / 服务层依赖 ===
    private final ClientRepository clientRepository;
    private final ProductRepository productRepository;
    private final OrderHeaderRepository orderHeaderRepository;
    private final OrderProcessingService orderProcessingService;

    @Autowired
    public SocketMessageHandler(ClientRepository clientRepository,
                                ProductRepository productRepository,
                                OrderHeaderRepository orderHeaderRepository,
                                OrderProcessingService orderProcessingService) {
        this.clientRepository = clientRepository;
        this.productRepository = productRepository;
        this.orderHeaderRepository = orderHeaderRepository;
        this.orderProcessingService = orderProcessingService;
    }
    private static final Map<String, ChannelHandlerContext> clientChannels = new ConcurrentHashMap<>();
    // 临时内存用户存储（演示用）：用户名 -> 明文密码
    // TODO: 替换为数据库存储，并对密码进行哈希（如 BCrypt）
    private static final Map<String, String> userStore = new ConcurrentHashMap<>();
    // 用户资料：username -> { phone: "", ... }
    private static final Map<String, Map<String,Object>> userProfiles = new ConcurrentHashMap<>();
    private final ObjectMapper objectMapper = new ObjectMapper();
    private static final ObjectMapper PERSIST = new ObjectMapper();
    /**
     * 下列内存结构已被数据库 (JPA) 替代，仅保留空壳防止旧方法引用编译错误。
     */
    @Deprecated private static final Map<String, List<Map<String, Object>>> carts = new ConcurrentHashMap<>();
    @Deprecated private static final List<Map<String, Object>> orders = new CopyOnWriteArrayList<>();
    @Deprecated private static final AtomicLong ORDER_SEQ = new AtomicLong(1);

    static {
        // 预置一个默认测试账户（仅用于开发调试）
        // TODO: 生产环境请移除，改为数据库初始化并使用密码哈希
        userStore.putIfAbsent("admin", "123456");
        // 内存商品已废弃：改由数据库 products 表驱动
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

    /**
     * 提供当前在线用户名列表（基于活跃的 Netty 连接）。
     */
    public static java.util.Set<String> getOnlineUsernames() {
        return java.util.Collections.unmodifiableSet(clientChannels.keySet());
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

    // 内存 product() 方法已删除（改用数据库实体 Product）

    private void handleGetProducts(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        int page = ((Number)request.getOrDefault("page", 1)).intValue();
        int size = ((Number)request.getOrDefault("size", 10)).intValue();
        if (page < 1) page = 1; if (size < 1) size = 10;
        // Spring Data page 从 0 开始
        var pageable = org.springframework.data.domain.PageRequest.of(page-1, size);
        String sort = String.valueOf(request.getOrDefault("sort", "discount_first"));
        java.util.List<com.shopping.server.model.Product> list;
        long total;
    // 总量直接使用 count()
    total = productRepository.count();
        switch (sort) {
            case "sales_desc":
                list = productRepository.findAllOrderBySalesDesc(pageable);
                break;
            case "price_asc":
                list = productRepository.findAllOrderByEffectivePriceAsc(pageable);
                break;
            case "price_desc":
                list = productRepository.findAllOrderByEffectivePriceDesc(pageable);
                break;
            case "discount_first":
            default:
                list = productRepository.findAllDiscountFirst(pageable);
                break;
        }
    Long _tmpMaxPid = null; try { _tmpMaxPid = productRepository.findMaxProductId(); } catch (Exception ignore) {}
    final Long maxPid = _tmpMaxPid;
    List<Map<String,Object>> products = new CopyOnWriteArrayList<>();
        list.forEach(p -> {
            Map<String,Object> it = new HashMap<>();
            // 同时返回多种 id 命名，兼容旧客户端
            it.put("id", p.getProductId());
            it.put("product_id", p.getProductId());
            it.put("productId", p.getProductId());
            it.put("name", p.getName());
            it.put("title", p.getName());
            it.put("price", p.getPrice());
            it.put("description", p.getDescription());
            it.put("imageUrl", p.getImageUrl());
            it.put("image", p.getImageUrl());
            it.put("stock", p.getStock());
            it.put("sales", p.getSales());
            if (maxPid != null) it.put("isNew", java.util.Objects.equals(maxPid, p.getProductId()));
                it.put("onSale", p.getOnSale());
                it.put("discountPrice", p.getDiscountPrice());
            products.add(it);
        });
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "products_response");
        resp.put("total", total);
        resp.put("products", products);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleSearchProducts(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String keyword = String.valueOf(request.getOrDefault("keyword", "")).trim();
        List<com.shopping.server.model.Product> list;
        if (keyword.isEmpty()) {
            list = productRepository.findAll();
        } else {
            list = productRepository.findByNameContainingIgnoreCase(keyword);
        }
        Long _tmpMaxPid = null; try { _tmpMaxPid = productRepository.findMaxProductId(); } catch (Exception ignore) {}
        final Long maxPid = _tmpMaxPid;
        List<Map<String,Object>> products = new CopyOnWriteArrayList<>();
        list.forEach(p -> {
            Map<String,Object> it = new HashMap<>();
            it.put("id", p.getProductId());
            it.put("product_id", p.getProductId());
            it.put("productId", p.getProductId());
            it.put("name", p.getName());
            it.put("title", p.getName());
            it.put("price", p.getPrice());
            it.put("description", p.getDescription());
            it.put("imageUrl", p.getImageUrl());
            it.put("image", p.getImageUrl());
            it.put("stock", p.getStock());
            it.put("sales", p.getSales());
            if (maxPid != null) it.put("isNew", java.util.Objects.equals(maxPid, p.getProductId()));
                it.put("onSale", p.getOnSale());
                it.put("discountPrice", p.getDiscountPrice());
            products.add(it);
        });
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
            boolean ok = (saved != null && saved.equals(password));
            // 若内存中未找到，则回退到数据库查询
            if (!ok) {
                var dbOpt = clientRepository.findByUsername(username);
                if (dbOpt.isPresent()) {
                    // 注意：当前为明文密码对比，生产应使用哈希（如 BCrypt）
                    ok = password.equals(dbOpt.get().getPassword());
                    if (ok && saved == null) {
                        // 为了兼容现有内存登录流程，将成功的 DB 账号写回内存
                        userStore.put(username, password);
                    }
                }
            }
            if (ok) {
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
        // 使用数据库：优先按折扣力度排序的前 4 个（若仓库方法可用），否则取 onSale 前 4 个
    List<Map<String,Object>> list = new CopyOnWriteArrayList<>();
    Long _tmpMaxPid = null; try { _tmpMaxPid = productRepository.findMaxProductId(); } catch (Exception ignore) {}
    final Long maxPid = _tmpMaxPid;
        try {
            var top = productRepository.findTopDiscountProducts(org.springframework.data.domain.PageRequest.of(0, 4));
            top.forEach(p -> list.add(mapOf(
                "product_id", p.getProductId(),
                "id", p.getProductId(),
                "productId", p.getProductId(),
                "name", p.getName(),
                "price", p.getPrice(),
                "description", p.getDescription(),
                "imageUrl", p.getImageUrl(),
                "stock", p.getStock(),
                "sales", p.getSales(),
                "onSale", p.getOnSale(),
                "discountPrice", p.getDiscountPrice(),
                "isNew", (maxPid != null && java.util.Objects.equals(maxPid, p.getProductId()))
            )));
        } catch (Exception ignore) {
            // 兜底：取所有上架商品前 4 个
            productRepository.findByOnSaleTrue().stream().limit(4).forEach(p -> list.add(mapOf(
                "product_id", p.getProductId(),
                "id", p.getProductId(),
                "productId", p.getProductId(),
                "name", p.getName(),
                "price", p.getPrice(),
                "description", p.getDescription(),
                "imageUrl", p.getImageUrl(),
                "stock", p.getStock(),
                "sales", p.getSales(),
                "onSale", p.getOnSale(),
                "discountPrice", p.getDiscountPrice(),
                "isNew", (maxPid != null && java.util.Objects.equals(maxPid, p.getProductId()))
            )));
        }
        // 如果依然为空（比如没有标记 on_sale 的商品），再做一次强兜底：返回任意商品前 4 个
        if (list.isEmpty()) {
            var page = productRepository.findAll(org.springframework.data.domain.PageRequest.of(0, 4));
            page.getContent().forEach(p -> list.add(mapOf(
                "product_id", p.getProductId(),
                "id", p.getProductId(),
                "productId", p.getProductId(),
                "name", p.getName(),
                "price", p.getPrice(),
                "description", p.getDescription(),
                "imageUrl", p.getImageUrl(),
                "stock", p.getStock(),
                "sales", p.getSales(),
                "onSale", p.getOnSale(),
                "discountPrice", p.getDiscountPrice(),
                "isNew", (maxPid != null && java.util.Objects.equals(maxPid, p.getProductId()))
            )));
        }
        // 强制包含最新商品
        if (maxPid != null) {
            try {
                var newest = productRepository.findTopByOrderByProductIdDesc();
                if (newest != null) {
                    boolean exists = list.stream().anyMatch(m -> {
                        Object pidObj = m.get("product_id");
                        return (pidObj instanceof Number) && java.util.Objects.equals(((Number)pidObj).longValue(), maxPid);
                    });
                    if (!exists) {
                        list.add(0, mapOf(
                            "product_id", newest.getProductId(),
                            "id", newest.getProductId(),
                            "productId", newest.getProductId(),
                            "name", newest.getName(),
                            "price", newest.getPrice(),
                            "description", newest.getDescription(),
                            "imageUrl", newest.getImageUrl(),
                            "stock", newest.getStock(),
                            "sales", newest.getSales(),
                            "onSale", newest.getOnSale(),
                            "discountPrice", newest.getDiscountPrice(),
                            "isNew", true
                        ));
                    } else {
                        // 已在列表，确保 isNew 为 true
                        for (var m : list) {
                            Object pidObj = m.get("product_id");
                            if (pidObj instanceof Number && java.util.Objects.equals(((Number)pidObj).longValue(), maxPid)) {
                                m.put("isNew", true);
                                break;
                            }
                        }
                    }
                }
            } catch (Exception ignore) {}
        }
        List<Map<String,Object>> out = list.size() > 4 ? new java.util.ArrayList<>(list.subList(0, 4)) : list;
        resp.put("products", out);
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
        String keyword = String.valueOf(request.getOrDefault("keyword", "")).trim();
        List<Map<String, Object>> results = new CopyOnWriteArrayList<>();
        List<com.shopping.server.model.Product> list;
        if (keyword.isEmpty()) {
            list = productRepository.findAll();
        } else {
            list = productRepository.findByNameContainingIgnoreCase(keyword);
        }
    Long maxPid = null; try { maxPid = productRepository.findMaxProductId(); } catch (Exception ignore) {}
    for (var p : list) {
            results.add(mapOf(
                    "product_id", p.getProductId(),
                    "name", p.getName(),
                    "price", p.getPrice(),
                    "description", p.getDescription(),
                    "imageUrl", p.getImageUrl(),
                    "stock", p.getStock(),
            "onSale", p.getOnSale(),
            "discountPrice", p.getDiscountPrice(),
            "isNew", (maxPid != null && java.util.Objects.equals(maxPid, p.getProductId()))
            ));
        }
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "search_results");
        resp.put("results", results);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleGetProductDetail(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        Object pid = request.get("product_id");
        if (pid == null) pid = request.get("productId");
        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "product_detail");
        Map<String, Object> productMap = null;
    if (pid != null) {
            long id = ((Number)pid).longValue();
            var opt = productRepository.findById(id);
            if (opt.isPresent()) {
                var p = opt.get();
        Long maxPid = null; try { maxPid = productRepository.findMaxProductId(); } catch (Exception ignore) {}
        productMap = mapOf(
                        "product_id", p.getProductId(),
                        "name", p.getName(),
                        "price", p.getPrice(),
                        "description", p.getDescription(),
                        "imageUrl", p.getImageUrl(),
                        "stock", p.getStock(),
            "sales", p.getSales(),
            "onSale", p.getOnSale(),
            "discountPrice", p.getDiscountPrice(),
            "isNew", (maxPid != null && java.util.Objects.equals(maxPid, p.getProductId()))
                );
            }
        }
        resp.put("product", productMap);
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }

    private void handleAddToCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object pidObj = request.get("product_id");
        if (pidObj == null) pidObj = request.get("productId");
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "add_to_cart_response");

        if (pidObj == null) {
            resp.put("success", false);
            resp.put("message", "缺少 productId");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        Long productId;
        try {
            if (pidObj instanceof Number) productId = ((Number) pidObj).longValue();
            else productId = Long.parseLong(String.valueOf(pidObj));
        } catch (Exception e) {
            resp.put("success", false);
            resp.put("message", "非法的 productId");
            resp.put("code", 2001);
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        int quantity;
        try {
            Object qObj = request.getOrDefault("quantity", 1);
            if (qObj instanceof Number) quantity = ((Number) qObj).intValue();
            else quantity = Integer.parseInt(String.valueOf(qObj));
        } catch (Exception e) {
            quantity = 1;
        }
        if (quantity < 1) quantity = 1;

        if (username == null) {
            resp.put("success", false);
            resp.put("message", "未登录");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) {
            resp.put("success", false);
            resp.put("message", "用户不存在");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        try {
            OrderHeader saved = orderProcessingService.addToCart(clientOpt.get().getClientId(), productId, quantity);
            resp.put("success", true);
            resp.put("headerId", saved.getId());
            resp.put("message", "加入购物车成功");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");

            // 关键：用 fetch-join 重载购物车，避免 LazyInitializationException
            var headerOpt = orderHeaderRepository.fetchCartWithItems(clientOpt.get(), OrderStatus.CART);
            if (headerOpt.isPresent()) {
                try {
                    sendCartDualResponses(ctx, headerOpt.get());
                } catch (Exception e) {
                    e.printStackTrace();
                }
            } else {
                // 回退：发送空购物车
                Map<String,Object> empty = new HashMap<>();
                empty.put("type", "cart_response");
                empty.put("items", java.util.List.of());
                empty.put("headerId", null);
                ctx.writeAndFlush(objectMapper.writeValueAsString(empty) + "\n");
            }
        } catch (Exception ex) {
            // 捕获所有异常，避免异常冒泡触发 Netty 关闭连接
            resp.put("success", false);
            resp.put("message", ex.getMessage() != null ? ex.getMessage() : "加入购物车失败");
            resp.put("code", 2002);
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
        }
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

        // 检查重复（内存）
        if (userStore.containsKey(username)) {
            response.put("success", false);
            response.put("message", "用户名已存在");
            ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
            return;
        }
        // 检查重复（数据库）
        if (clientRepository.existsByUsername(username)) {
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

        // 将用户写入数据库（与内存并存，便于后续统一迁移到 DB）
        try {
            Client client = new Client();
            client.setUsername(username);
            client.setPassword(password); // 明文存储仅用于演示，生产环境请使用哈希
            client.setPhone(phone);
            client.setPurchaseCount(0);
            clientRepository.save(client);
        } catch (Exception dbEx) {
            // 数据库写入失败时，返回提示（内存仍然保留，登录不受影响）
            response.put("success", false);
            response.put("message", "注册失败：数据库写入异常" + (dbEx.getMessage() != null ? (" - " + dbEx.getMessage()) : ""));
            ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
            return;
        }

        response.put("success", true);
        response.put("message", "注册成功");
    ctx.writeAndFlush(objectMapper.writeValueAsString(response) + "\n");
    }

    private void handleClearCart(ChannelHandlerContext ctx, Map<String,Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "clear_cart_response");
        if (username == null) {
            resp.put("success", false);
            resp.put("message", "未登录");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) {
            resp.put("success", false);
            resp.put("message", "用户不存在");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
    var headerOpt = orderHeaderRepository.fetchCartWithItems(clientOpt.get(), OrderStatus.CART);
        if (headerOpt.isPresent()) { 
            OrderHeader header = headerOpt.get();
            header.getItems().clear();
            header.setTotalPrice(header.calcTotal());
            orderHeaderRepository.save(header);
            resp.put("success", true);
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            sendCartDualResponses(ctx, header);
        } else {
            resp.put("success", true);
            resp.put("message", "购物车为空");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            handleGetCart(ctx, request);
        }
    }

    private void handleGetOrders(ChannelHandlerContext ctx, Map<String,Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "orders_response");
        if (username == null) {
            resp.put("orders", List.of());
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) {
            resp.put("orders", List.of());
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
    List<Map<String,Object>> myOrders = new CopyOnWriteArrayList<>();
    for (OrderHeader h : orderHeaderRepository.findByClientWithItems(clientOpt.get())) {
            if (h.getStatus() == OrderStatus.CART) continue; // 不返回购物车
            Map<String,Object> om = new HashMap<>();
            om.put("orderId", h.getId());
            om.put("status", h.getStatus().name());
            om.put("total_price", h.calcTotal());
            om.put("order_time", h.getCreatedAt() != null ? h.getCreatedAt().toString() : null);
            List<Map<String,Object>> items = new CopyOnWriteArrayList<>();
            for (OrderItem it : h.getItems()) {
                items.add(mapOf(
                        "productId", it.getProduct().getProductId(),
                        "name", it.getProduct().getName(),
                        "price", it.getPrice(),
                        "quantity", it.getQuantity()
                ));
            }
            om.put("items", items);
            myOrders.add(om);
        }
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
            // 从数据库补齐用户ID
            clientRepository.findByUsername(username).ifPresent(c -> resp.put("clientId", c.getClientId()));
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
        // 从数据库加载 Client
    var dbClientOpt = clientRepository.findByUsername(username);
    Client dbClient = dbClientOpt.orElse(null);
        // 更新用户名（演示：直接改 key；实际生产中需迁移所有相关数据）
        if (newUsername != null && !newUsername.isEmpty() && !newUsername.equals(username)) {
            // 数据库层面的唯一性校验
            if (clientRepository.existsByUsername(newUsername)) {
                resp.put("success", false);
                resp.put("message", "新用户名已存在");
                ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
                return;
            }
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
            // clientChannels 映射迁移
            ChannelHandlerContext ch = clientChannels.remove(username);
            if (ch != null) clientChannels.put(newUsername, ch);
            // 数据库 Client 更新用户名
            if (dbClient != null) {
                dbClient.setUsername(newUsername);
            }
            username = newUsername;
        }
        // 同步数据库中的手机号/密码；若 DB 还不存在该用户，进行兜底创建
        if (dbClient != null) {
            if (newPhone != null) dbClient.setPhone(newPhone);
            if (newPassword != null && !newPassword.isEmpty()) dbClient.setPassword(newPassword);
            clientRepository.save(dbClient);
        } else {
            // 兜底：某些老账号可能只存在于内存 userStore 中，这里创建一条 DB 记录
            try {
                Client create = new Client();
                create.setUsername(username);
                // 优先使用新密码；否则用内存里的旧密码；再不行留空字符串（演示环境）
                String pwdFallback = (newPassword != null && !newPassword.isEmpty()) ? newPassword : userStore.getOrDefault(username, "");
                create.setPassword(pwdFallback);
                // phone 来自本次更新或历史资料
                String phoneFallback = newPhone;
                if (phoneFallback == null) {
                    Object ph = userProfiles.getOrDefault(username, new HashMap<>()).get("phone");
                    phoneFallback = ph == null ? null : String.valueOf(ph);
                }
                create.setPhone(phoneFallback);
                create.setPurchaseCount(0);
                clientRepository.save(create);
            } catch (Exception ignore) {}
        }
        try { saveUsers(); } catch (Exception ignore) {}
        try { saveProfiles(); } catch (Exception ignore) {}
        try { saveCarts(); } catch (Exception ignore) {}
        try { saveOrders(); } catch (Exception ignore) {}
        resp.put("success", true);
        resp.put("message", "账户信息已更新");
        resp.put("username", username);
        resp.put("phone", String.valueOf(userProfiles.getOrDefault(username, new HashMap<>()).getOrDefault("phone", "")));
        // 返回最新的 clientId（若可用）
        clientRepository.findByUsername(username).ifPresent(c -> resp.put("clientId", c.getClientId()));
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
    }
    
    private void handleGetCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        if (username == null) {
            // 未登录仍保持协议：返回空购物车
            Map<String,Object> empty = new HashMap<>();
            empty.put("type", "cart_response");
            empty.put("items", List.of());
            empty.put("headerId", null);
            ctx.writeAndFlush(objectMapper.writeValueAsString(empty) + "\n");
            return;
        }
        // 查 Client
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) {
            Map<String,Object> empty = new HashMap<>();
            empty.put("type", "cart_response");
            empty.put("items", List.of());
            empty.put("headerId", null);
            ctx.writeAndFlush(objectMapper.writeValueAsString(empty) + "\n");
            return;
        }
    var client = clientOpt.get();
    var headerOpt = orderHeaderRepository.fetchCartWithItems(client, OrderStatus.CART);
        if (headerOpt.isEmpty()) {
            Map<String,Object> empty = new HashMap<>();
            empty.put("type", "cart_response");
            empty.put("items", List.of());
            empty.put("headerId", null);
            ctx.writeAndFlush(objectMapper.writeValueAsString(empty) + "\n");
            return;
        }
    OrderHeader header = headerOpt.get();
        sendCartDualResponses(ctx, header);
    }

    private void sendCartDualResponses(ChannelHandlerContext ctx, OrderHeader header) throws Exception {
        // 第一条：cart_items（兼容原协议）
        Map<String,Object> itemsMsg = new HashMap<>();
        itemsMsg.put("type", "cart_items");
        List<Map<String,Object>> itemList = new CopyOnWriteArrayList<>();
        for (OrderItem it : header.getItems()) {
            Map<String,Object> m = new HashMap<>();
            m.put("product_id", it.getProduct().getProductId());
            m.put("name", it.getProduct().getName());
            m.put("price", it.getPrice());
            m.put("quantity", it.getQuantity());
            m.put("stock", it.getProduct().getStock());
            // 新增：为购物车项补充原价与促销信息，客户端可用于展示双价
            m.put("listPrice", it.getProduct().getPrice());
            m.put("onSale", it.getProduct().getOnSale());
            m.put("discountPrice", it.getProduct().getDiscountPrice());
            itemList.add(m);
        }
        itemsMsg.put("items", itemList);
        itemsMsg.put("headerId", header.getId());
        ctx.writeAndFlush(objectMapper.writeValueAsString(itemsMsg) + "\n");

        // 第二条：cart_response（文档字段风格）
        Map<String,Object> resp2 = new HashMap<>();
        resp2.put("type", "cart_response");
        List<Map<String,Object>> items2 = new CopyOnWriteArrayList<>();
        for (OrderItem it : header.getItems()) {
            Map<String,Object> m = new HashMap<>();
            m.put("productId", it.getProduct().getProductId());
            m.put("name", it.getProduct().getName());
            m.put("price", it.getPrice());
            m.put("quantity", it.getQuantity());
            m.put("stock", it.getProduct().getStock());
            // 同步补充原价与促销信息
            m.put("listPrice", it.getProduct().getPrice());
            m.put("onSale", it.getProduct().getOnSale());
            m.put("discountPrice", it.getProduct().getDiscountPrice());
            items2.add(m);
        }
        resp2.put("items", items2);
        resp2.put("headerId", header.getId());
        ctx.writeAndFlush(objectMapper.writeValueAsString(resp2) + "\n");
    }

    private void handleRemoveFromCart(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Object pidLookup = request.get("product_id");
        if (pidLookup == null) pidLookup = request.get("productId");
        final Object pidObj = pidLookup;
        if (username == null || pidObj == null) {
            handleGetCart(ctx, request);
            return;
        }
        var clientOpt = clientRepository.findByUsername(username);
        clientOpt.ifPresent(client -> {
            var headerOpt = orderHeaderRepository.fetchCartWithItems(client, OrderStatus.CART);
            if (headerOpt.isPresent()) {
                OrderHeader header = headerOpt.get();
                long pid = ((Number)pidObj).longValue();
                header.getItems().removeIf(i -> i.getProduct().getProductId().equals(pid));
                header.setTotalPrice(header.calcTotal());
                orderHeaderRepository.save(header);
                try { sendCartDualResponses(ctx, header); } catch (Exception e) { e.printStackTrace(); }
                return;
            }
            try { handleGetCart(ctx, request); } catch (Exception e) { e.printStackTrace(); }
        });
    }

    private void handleSetCartQuantity(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
    Object pidObj = request.get("product_id");
    if (pidObj == null) pidObj = request.get("productId");
    int qInput = ((Number)request.getOrDefault("quantity", 1)).intValue();
    if (qInput < 0) qInput = 0;
    final int quantity = qInput;
        if (username == null || pidObj == null) { handleGetCart(ctx, request); return; }
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) { handleGetCart(ctx, request); return; }
    var headerOpt = orderHeaderRepository.fetchCartWithItems(clientOpt.get(), OrderStatus.CART);
        if (headerOpt.isEmpty()) { handleGetCart(ctx, request); return; }
        OrderHeader header = headerOpt.get();
    final long pid = ((Number)pidObj).longValue();
        boolean exists = false;
        for (OrderItem it : header.getItems()) {
            if (it.getProduct().getProductId().equals(pid)) {
                exists = true;
                Integer stock = it.getProduct().getStock();
                int max = stock == null ? quantity : Math.min(quantity, stock);
                if (max <= 0) {
                    header.getItems().remove(it);
                } else {
                    it.setQuantity(max);
                }
                break;
            }
        }
        if (!exists && quantity > 0) {
            // 允许直接新增
            productRepository.findById(pid).ifPresent(prod -> {
                int max = prod.getStock() == null ? quantity : Math.min(quantity, prod.getStock());
                if (max > 0) {
                    OrderItem item = new OrderItem();
                    item.setProduct(prod);
                    item.setQuantity(max);
                    item.setPrice(prod.getPrice());
                    header.addItem(item);
                }
            });
        }
        header.setTotalPrice(header.calcTotal());
        orderHeaderRepository.save(header);
        sendCartDualResponses(ctx, header);
    }
    
    private void handleCheckout(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "checkout_response");
        if (username == null) {
            resp.put("success", false);
            resp.put("message", "未登录");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) {
            resp.put("success", false);
            resp.put("message", "用户不存在");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
    var headerOpt = orderHeaderRepository.fetchCartWithItems(clientOpt.get(), OrderStatus.CART);
        if (headerOpt.isEmpty() || headerOpt.get().getItems().isEmpty()) {
            resp.put("success", false);
            resp.put("code", 3001);
            resp.put("message", "购物车为空");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            // 同时发送 order_response 兼容
            Map<String,Object> resp2 = new HashMap<>();
            resp2.put("type", "order_response");
            resp2.put("success", false);
            resp2.put("code", 3001);
            resp2.put("message", "购物车为空");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp2) + "\n");
            return;
        }
        OrderHeader header = headerOpt.get();
        try {
            OrderHeader paid = orderProcessingService.checkout(header.getId());
            resp.put("success", true);
            resp.put("message", "结算成功");
            resp.put("headerId", paid.getId());
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            Map<String,Object> resp2 = new HashMap<>();
            resp2.put("type", "order_response");
            resp2.put("success", true);
            resp2.put("orderId", paid.getId());
            resp2.put("message", "订单创建成功");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp2) + "\n");
        } catch (IllegalStateException ex) {
            resp.put("success", false);
            resp.put("code", 2002);
            resp.put("message", ex.getMessage());
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            Map<String,Object> resp2 = new HashMap<>();
            resp2.put("type", "order_response");
            resp2.put("success", false);
            resp2.put("code", 2002);
            resp2.put("message", ex.getMessage());
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp2) + "\n");
        }
    }

    private void handleCreateOrder(ChannelHandlerContext ctx, Map<String, Object> request) throws Exception {
        String username = (String) request.getOrDefault("username", findUsernameByCtx(ctx));
        Map<String,Object> resp = new HashMap<>();
        resp.put("type", "order_response");
        if (username == null) {
            resp.put("success", false);
            resp.put("code", 3002);
            resp.put("message", "未登录");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
        var clientOpt = clientRepository.findByUsername(username);
        if (clientOpt.isEmpty()) {
            resp.put("success", false);
            resp.put("code", 3002);
            resp.put("message", "用户不存在");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }
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
        if (reqItems.isEmpty()) {
            resp.put("success", false);
            resp.put("code", 3002);
            resp.put("message", "订单创建失败");
            ctx.writeAndFlush(objectMapper.writeValueAsString(resp) + "\n");
            return;
        }

        try {
            OrderHeader paid = orderProcessingService.createOrderForItems(clientOpt.get().getClientId(), reqItems);
            resp.put("success", true);
            resp.put("orderId", paid.getId());
            resp.put("message", "订单创建成功");
        } catch (IllegalArgumentException | IllegalStateException ex) {
            resp.put("success", false);
            resp.put("code", 2002);
            resp.put("message", ex.getMessage() != null ? ex.getMessage() : "订单创建失败");
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