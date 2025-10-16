package com.shopping.server.web;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.bind.annotation.RequestParam;

import javax.imageio.ImageIO;
import javax.servlet.http.HttpServletRequest;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/api/upload")
public class UploadController {

    @Value("${app.images.dir:c:/Users/Edward/Desktop/test/images}")
    private String imagesDir;

    private static final long MAX_SIZE_BYTES = 10L * 1024 * 1024; // 10MB

    @PostMapping(value = "/image", consumes = MediaType.MULTIPART_FORM_DATA_VALUE)
    public ResponseEntity<Map<String, Object>> uploadImage(@RequestParam("file") MultipartFile file,
                                                           HttpServletRequest request) throws IOException {
        Map<String, Object> resp = new HashMap<>();
        if (file == null || file.isEmpty()) {
            resp.put("success", false);
            resp.put("message", "文件为空");
            return ResponseEntity.badRequest().body(resp);
        }
        if (file.getSize() > MAX_SIZE_BYTES) {
            resp.put("success", false);
            resp.put("message", "文件过大，最大 10MB");
            return ResponseEntity.badRequest().body(resp);
        }
        String contentType = file.getContentType();
        if (contentType == null) contentType = "application/octet-stream";
        String ext;
        switch (contentType.toLowerCase(Locale.ROOT)) {
            case "image/jpeg":
            case "image/jpg":
                ext = ".jpg";
                break;
            case "image/png":
                // 服务器侧将 PNG 转为 JPEG，避免客户端对 PNG 解码插件的依赖
                ext = ".jpg";
                break;
            case "image/gif":
                ext = ".gif";
                break;
            default:
                resp.put("success", false);
                resp.put("message", "不支持的文件类型，仅支持 JPG/PNG/GIF");
                return ResponseEntity.badRequest().body(resp);
        }

        // 生成安全文件名：时间戳 + 随机 UUID 后缀（短）
        String baseName = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyyMMdd_HHmmss_SSS"));
        String safeName = baseName + "_" + UUID.randomUUID().toString().replace("-", "").substring(0, 8) + ext;
        Path dir = Paths.get(imagesDir).toAbsolutePath().normalize();
        Files.createDirectories(dir);
        Path target = dir.resolve(safeName);

        // 对 PNG 进行转码为 JPEG（白底），其他类型原样保存
        if ("image/png".equalsIgnoreCase(contentType)) {
            try (InputStream in = file.getInputStream()) {
                BufferedImage src = ImageIO.read(in);
                if (src == null) {
                    resp.put("success", false);
                    resp.put("message", "无法读取PNG图片");
                    return ResponseEntity.badRequest().body(resp);
                }
                BufferedImage rgb = new BufferedImage(src.getWidth(), src.getHeight(), BufferedImage.TYPE_INT_RGB);
                Graphics2D g2 = rgb.createGraphics();
                g2.setComposite(AlphaComposite.SrcOver);
                g2.setColor(Color.WHITE);
                g2.fillRect(0, 0, rgb.getWidth(), rgb.getHeight());
                g2.drawImage(src, 0, 0, null);
                g2.dispose();
                if (!ImageIO.write(rgb, "jpg", target.toFile())) {
                    resp.put("success", false);
                    resp.put("message", "服务器无法写入JPEG文件");
                    return ResponseEntity.internalServerError().body(resp);
                }
                // 返回内容类型也调整为 JPEG
                contentType = "image/jpeg";
            } catch (IOException e) {
                resp.put("success", false);
                resp.put("message", "PNG转码失败: " + e.getMessage());
                return ResponseEntity.internalServerError().body(resp);
            }
        } else {
            file.transferTo(target.toFile());
        }

        String contextUrl = getBaseUrl(request);
        String relative = "/images/" + safeName;
        String fullUrl = contextUrl + relative;

        resp.put("success", true);
        resp.put("url", relative);
        resp.put("fullUrl", fullUrl);
        resp.put("filename", safeName);
        resp.put("contentType", contentType);
        resp.put("size", file.getSize());
        return ResponseEntity.ok(resp);
    }

    private static String getBaseUrl(HttpServletRequest request) {
        String scheme = request.getScheme();
        String host = request.getServerName();
        int port = request.getServerPort();
        boolean isDefault = ("http".equalsIgnoreCase(scheme) && port == 80) || ("https".equalsIgnoreCase(scheme) && port == 443);
        return scheme + "://" + host + (isDefault ? "" : (":" + port));
    }
}
