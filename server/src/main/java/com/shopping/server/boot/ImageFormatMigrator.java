package com.shopping.server.boot;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

import javax.imageio.ImageIO;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.*;

/**
 * 启动时扫描图片目录：
 * - 对每个 .png 文件，若不存在同名 .jpg，则转码生成 JPG（白底），便于历史 PNG 也能以 JPG 访问。
 */
@Component
@Order(10) // 较早执行，但不阻塞主体启动
public class ImageFormatMigrator implements ApplicationRunner {

    @Value("${app.images.dir:c:/Users/Edward/Desktop/test/images}")
    private String imagesDir;

    @Override
    public void run(ApplicationArguments args) {
        try {
            Path dir = Paths.get(imagesDir).toAbsolutePath().normalize();
            if (!Files.isDirectory(dir)) return;
            try (DirectoryStream<Path> stream = Files.newDirectoryStream(dir, "*.png")) {
                for (Path png : stream) {
                    try {
                        String name = png.getFileName().toString();
                        String base = name.substring(0, name.length() - 4); // drop .png
                        Path jpg = dir.resolve(base + ".jpg");
                        if (Files.exists(jpg)) continue; // 已有同名 JPG

                        BufferedImage src = ImageIO.read(png.toFile());
                        if (src == null) continue;
                        BufferedImage rgb = new BufferedImage(src.getWidth(), src.getHeight(), BufferedImage.TYPE_INT_RGB);
                        Graphics2D g2 = rgb.createGraphics();
                        g2.setComposite(AlphaComposite.SrcOver);
                        g2.setColor(Color.WHITE);
                        g2.fillRect(0, 0, rgb.getWidth(), rgb.getHeight());
                        g2.drawImage(src, 0, 0, null);
                        g2.dispose();
                        ImageIO.write(rgb, "jpg", jpg.toFile());
                        System.out.println("[ImageFormatMigrator] Generated JPG for PNG: " + png.getFileName() + " -> " + jpg.getFileName());
                    } catch (Exception ex) {
                        System.out.println("[ImageFormatMigrator] Skip file due to error: " + png + ", err=" + ex.getMessage());
                    }
                }
            }
        } catch (IOException ignore) {
        }
    }
}
