package com.shopping.server.web;

import com.shopping.server.model.Product;
import com.shopping.server.model.ProductSizeInventory;
import com.shopping.server.repository.ProductRepository;
import com.shopping.server.repository.ProductSizeInventoryRepository;
import org.springframework.http.ResponseEntity;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.util.*;

@RestController
@RequestMapping("/api/admin/products")
@CrossOrigin
public class AdminProductController {

    private final ProductRepository productRepository;
    private final ProductSizeInventoryRepository psiRepository;

    public AdminProductController(ProductRepository productRepository,
                                  ProductSizeInventoryRepository psiRepository) {
        this.productRepository = productRepository;
        this.psiRepository = psiRepository;
    }

    // 创建商品（可选附带尺码库存）
    @PostMapping
    @Transactional
    public ResponseEntity<?> createProduct(@RequestBody Map<String, Object> body) {
        try {
            String name = str(body.get("name"));
            Object priceObj = body.get("price");
            String description = str(body.get("description"));
            String imageUrl = str(body.get("imageUrl"));
            Boolean onSale = bool(body.get("onSale"));
            Object discountPriceObj = body.get("discountPrice");
            @SuppressWarnings("unchecked")
            Map<String, Object> sizeStocks = (Map<String, Object>) body.get("sizeStocks"); // { "37": 10, "38": 0, ... }

            if (name == null || name.isBlank() || priceObj == null) {
                return ResponseEntity.badRequest().body(Map.of("success", false, "message", "name/price 必填"));
            }
            BigDecimal price = toBigDecimal(priceObj);
            if (price == null || price.compareTo(BigDecimal.ZERO) <= 0) {
                return ResponseEntity.badRequest().body(Map.of("success", false, "message", "非法价格"));
            }
            BigDecimal discountPrice = toBigDecimal(discountPriceObj);
            if (discountPrice != null && (discountPrice.compareTo(BigDecimal.ZERO) <= 0 || discountPrice.compareTo(price) >= 0)) {
                return ResponseEntity.badRequest().body(Map.of("success", false, "message", "折扣价需大于 0 且小于原价"));
            }

            Product p = new Product();
            p.setName(name);
            p.setPrice(price);
            p.setDescription(description);
            p.setImageUrl(imageUrl);
            p.setOnSale(onSale);
            p.setDiscountPrice(discountPrice);
            // 总库存按尺码汇总（若未提供尺码库存，则允许单一 stock）
            Integer singleStock = toInt(body.get("stock"));
            if (sizeStocks != null && !sizeStocks.isEmpty()) {
                int sum = sumStocks(sizeStocks);
                p.setStock(sum);
            } else if (singleStock != null) {
                p.setStock(singleStock);
            } else {
                p.setStock(0);
            }
            p = productRepository.save(p);

            // 保存尺码库存
            if (sizeStocks != null && !sizeStocks.isEmpty()) {
                upsertSizeStocksInternal(p, sizeStocks, true);
            }

            Map<String, Object> resp = new HashMap<>();
            resp.put("success", true);
            resp.put("productId", p.getProductId());
            resp.put("stock", p.getStock());
            return ResponseEntity.ok(resp);
        } catch (Exception e) {
            return ResponseEntity.internalServerError().body(Map.of("success", false, "message", e.getMessage()));
        }
    }

    // 覆盖或增量更新某商品的尺码库存
    @PutMapping("/{id}/sizes")
    @Transactional
    public ResponseEntity<?> upsertSizeStocks(@PathVariable("id") Long productId,
                                              @RequestParam(name = "replaceAll", required = false, defaultValue = "false") boolean replaceAll,
                                              @RequestBody Map<String, Object> body) {
        var opt = productRepository.findById(productId);
        if (opt.isEmpty()) return ResponseEntity.badRequest().body(Map.of("success", false, "message", "商品不存在"));
        Product p = opt.get();
        @SuppressWarnings("unchecked")
        Map<String, Object> sizeStocks = (Map<String, Object>) body.get("sizeStocks");
        if (sizeStocks == null || sizeStocks.isEmpty())
            return ResponseEntity.badRequest().body(Map.of("success", false, "message", "sizeStocks 不能为空"));

        if (replaceAll) {
            // 删除所有旧尺码后重建
            psiRepository.findByProduct(p).forEach(psiRepository::delete);
            upsertSizeStocksInternal(p, sizeStocks, true);
        } else {
            upsertSizeStocksInternal(p, sizeStocks, false);
        }
        // 汇总更新 Product.stock
        int sum = psiRepository.findByProduct(p).stream().mapToInt(x -> x.getStock() == null ? 0 : x.getStock()).sum();
        p.setStock(sum);
        productRepository.save(p);

        return ResponseEntity.ok(Map.of("success", true, "productId", p.getProductId(), "stock", p.getStock()));
    }

    // 查询某商品尺码库存
    @GetMapping("/{id}/sizes")
    public ResponseEntity<?> listSizeStocks(@PathVariable("id") Long productId) {
        var opt = productRepository.findById(productId);
        if (opt.isEmpty()) return ResponseEntity.badRequest().body(Map.of("success", false, "message", "商品不存在"));
        Product p = opt.get();
        List<Map<String,Object>> sizes = new ArrayList<>();
        for (var psi : psiRepository.findByProduct(p)) {
            sizes.add(Map.of("size", psi.getSize(), "stock", psi.getStock()));
        }
        return ResponseEntity.ok(Map.of("success", true, "productId", p.getProductId(), "sizes", sizes));
    }

    private void upsertSizeStocksInternal(Product p, Map<String, Object> sizeStocks, boolean createIfMissing) {
        for (Map.Entry<String, Object> e : sizeStocks.entrySet()) {
            Integer size = toInt(e.getKey());
            Integer stock = toInt(e.getValue());
            if (size == null || stock == null) continue;
            // 仅支持 37-45
            if (size < 30 || size > 50) continue;
            var optPsi = psiRepository.findByProductAndSize(p, size);
            if (optPsi.isPresent()) {
                var psi = optPsi.get();
                psi.setStock(stock);
                psiRepository.save(psi);
            } else if (createIfMissing) {
                var psi = new ProductSizeInventory();
                psi.setProduct(p);
                psi.setSize(size);
                psi.setStock(stock);
                psiRepository.save(psi);
            }
        }
    }

    private static String str(Object o) { return o == null ? null : String.valueOf(o); }
    private static Boolean bool(Object o) { return o == null ? null : (o instanceof Boolean ? (Boolean) o : Boolean.parseBoolean(String.valueOf(o))); }
    private static Integer toInt(Object o) {
        if (o == null) return null;
        if (o instanceof Number) return ((Number) o).intValue();
        try { return Integer.parseInt(String.valueOf(o)); } catch (Exception e) { return null; }
    }
    private static BigDecimal toBigDecimal(Object o) {
        if (o == null) return null;
        if (o instanceof BigDecimal) return (BigDecimal) o;
        if (o instanceof Number) return new BigDecimal(o.toString());
        try { return new BigDecimal(String.valueOf(o)); } catch (Exception e) { return null; }
    }
    private static int sumStocks(Map<String, Object> sizeStocks) {
        int sum = 0;
        for (Object v : sizeStocks.values()) {
            Integer s = toInt(v);
            sum += (s == null ? 0 : s);
        }
        return sum;
    }
}
