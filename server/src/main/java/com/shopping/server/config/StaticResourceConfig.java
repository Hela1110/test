package com.shopping.server.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.ResourceHandlerRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * 将本地图片目录映射为 HTTP 静态资源，默认映射到 /images/**。
 * 支持在 application.yml 通过 app.images.dir 覆盖路径。
 */
@Configuration
public class StaticResourceConfig implements WebMvcConfigurer {

    /**
     * 本地图片目录。可在 application.yml 通过 app.images.dir 覆盖。
     * 默认指向工作区 test/image 目录。
     */
    @Value("${app.images.dir:c:/Users/Edward/Desktop/test/image}")
    private String imagesDir;

    @Override
    public void addResourceHandlers(ResourceHandlerRegistry registry) {
        Path dirPath = Paths.get(imagesDir).toAbsolutePath().normalize();
        // 使用 file:/// 前缀，Windows 路径需要以 / 开头的三个斜杠
        String location = dirPath.toUri().toString(); // e.g. file:///C:/Users/Edward/Desktop/test/image/

        registry.addResourceHandler("/images/**")
                .addResourceLocations(location)
                .setCachePeriod(3600 * 24); // 缓存一天
    }
}
