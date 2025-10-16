/*
 Navicat Premium Dump SQL

 Source Server         : 1
 Source Server Type    : MySQL
 Source Server Version : 80043 (8.0.43)
 Source Host           : localhost:3306
 Source Schema         : shopping_system

 Target Server Type    : MySQL
 Target Server Version : 80043 (8.0.43)
 File Encoding         : 65001

 Date: 24/09/2025 17:53:18
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for cart_items
-- ----------------------------
DROP TABLE IF EXISTS `cart_items`;
CREATE TABLE `cart_items`  (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `quantity` int NOT NULL,
  `client_id` bigint NOT NULL,
  `product_id` bigint NOT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE INDEX `UK17n3sdo2ts1p0pkdtywumqx2g`(`client_id` ASC, `product_id` ASC) USING BTREE,
  INDEX `FK1re40cjegsfvw58xrkdp6bac6`(`product_id` ASC) USING BTREE,
  CONSTRAINT `FK1re40cjegsfvw58xrkdp6bac6` FOREIGN KEY (`product_id`) REFERENCES `products` (`product_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `FKs7s09sblyi77am85dp1je4u2h` FOREIGN KEY (`client_id`) REFERENCES `clients` (`client_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of cart_items
-- ----------------------------

-- ----------------------------
-- Table structure for clients
-- ----------------------------
DROP TABLE IF EXISTS `clients`;
CREATE TABLE `clients`  (
  `client_id` bigint NOT NULL AUTO_INCREMENT,
  `avatar` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT NULL,
  `email` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT NULL,
  `password` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `phone` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT NULL,
  `purchase_count` int NULL DEFAULT NULL,
  `username` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`client_id`) USING BTREE,
  UNIQUE INDEX `uk_client_username`(`username` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 2 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of clients
-- ----------------------------
INSERT INTO `clients` VALUES (1, NULL, NULL, '$2a$10$BGuW30YvaB6dxNiRiK50mujrIoBS78/LeSBM8u52Lv3o3yGW8pWk2', NULL, 0, 'admin');

-- ----------------------------
-- Table structure for order_headers
-- ----------------------------
DROP TABLE IF EXISTS `order_headers`;
CREATE TABLE `order_headers`  (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `created_at` datetime(6) NOT NULL,
  `status` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `total_price` decimal(19, 2) NOT NULL,
  `client_id` bigint NOT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  INDEX `FKew1xoxdsxeg81qh8hx7aoknh1`(`client_id` ASC) USING BTREE,
  CONSTRAINT `FKew1xoxdsxeg81qh8hx7aoknh1` FOREIGN KEY (`client_id`) REFERENCES `clients` (`client_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of order_headers
-- ----------------------------

-- ----------------------------
-- Table structure for order_items
-- ----------------------------
DROP TABLE IF EXISTS `order_items`;
CREATE TABLE `order_items`  (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `price` decimal(19, 2) NOT NULL,
  `quantity` int NOT NULL,
  `order_id` bigint NOT NULL,
  `product_id` bigint NOT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  INDEX `FKp1p3m90e43wxlw16ul58haosp`(`order_id` ASC) USING BTREE,
  INDEX `FKocimc7dtr037rh4ls4l95nlfi`(`product_id` ASC) USING BTREE,
  CONSTRAINT `FKocimc7dtr037rh4ls4l95nlfi` FOREIGN KEY (`product_id`) REFERENCES `products` (`product_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `FKp1p3m90e43wxlw16ul58haosp` FOREIGN KEY (`order_id`) REFERENCES `order_headers` (`id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of order_items
-- ----------------------------

-- ----------------------------
-- Table structure for orders
-- ----------------------------
DROP TABLE IF EXISTS `orders`;
CREATE TABLE `orders`  (
  `order_id` bigint NOT NULL AUTO_INCREMENT,
  `order_time` datetime(6) NULL DEFAULT NULL,
  `quantity` int NULL DEFAULT NULL,
  `status` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT NULL,
  `total_price` decimal(19, 2) NULL DEFAULT NULL,
  `client_id` bigint NULL DEFAULT NULL,
  `product_id` bigint NULL DEFAULT NULL,
  PRIMARY KEY (`order_id`) USING BTREE,
  INDEX `FKm2dep9derpoaehshbkkatam3v`(`client_id` ASC) USING BTREE,
  INDEX `FKkp5k52qtiygd8jkag4hayd0qg`(`product_id` ASC) USING BTREE,
  CONSTRAINT `FKkp5k52qtiygd8jkag4hayd0qg` FOREIGN KEY (`product_id`) REFERENCES `products` (`product_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `FKm2dep9derpoaehshbkkatam3v` FOREIGN KEY (`client_id`) REFERENCES `clients` (`client_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of orders
-- ----------------------------

-- ----------------------------
-- Table structure for product_types
-- ----------------------------
DROP TABLE IF EXISTS `product_types`;
CREATE TABLE `product_types`  (
  `type_id` bigint NOT NULL AUTO_INCREMENT,
  `type_name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `value` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `product_id` bigint NULL DEFAULT NULL,
  PRIMARY KEY (`type_id`) USING BTREE,
  INDEX `FKjosgas3rvidq01xnqpwy2hvhe`(`product_id` ASC) USING BTREE,
  CONSTRAINT `FKjosgas3rvidq01xnqpwy2hvhe` FOREIGN KEY (`product_id`) REFERENCES `products` (`product_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB AUTO_INCREMENT = 5 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of product_types
-- ----------------------------
INSERT INTO `product_types` VALUES (1, '颜色', '黑色', 1);
INSERT INTO `product_types` VALUES (2, '颜色', '白色', 1);
INSERT INTO `product_types` VALUES (3, '内存', '8G', 2);
INSERT INTO `product_types` VALUES (4, '内存', '12G', 2);

-- ----------------------------
-- Table structure for products
-- ----------------------------
DROP TABLE IF EXISTS `products`;
CREATE TABLE `products`  (
  `product_id` bigint NOT NULL AUTO_INCREMENT,
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT NULL,
  `discount_price` decimal(19, 2) NULL DEFAULT NULL,
  `image_url` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT NULL,
  `name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `on_sale` bit(1) NULL DEFAULT NULL,
  `price` decimal(19, 2) NOT NULL,
  `stock` int NULL DEFAULT NULL,
  `version` bigint NULL DEFAULT NULL,
  `discount` decimal(10, 2) NULL DEFAULT NULL,
  PRIMARY KEY (`product_id`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 16 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of products
-- ----------------------------
INSERT INTO `products` VALUES (1, NULL, NULL, NULL, '苹果 iPhone', b'0', 5999.00, 200, 0, NULL);
INSERT INTO `products` VALUES (2, NULL, NULL, NULL, '小米 手机', b'0', 1999.00, 500, 0, NULL);
INSERT INTO `products` VALUES (3, NULL, NULL, NULL, '华为 Mate', b'0', 4999.00, 300, 0, NULL);
INSERT INTO `products` VALUES (4, NULL, NULL, NULL, '联想 笔记本', b'0', 6999.00, 150, 0, NULL);
INSERT INTO `products` VALUES (5, NULL, NULL, NULL, '罗技 鼠标', b'0', 199.00, 800, 0, NULL);
INSERT INTO `products` VALUES (6, NULL, NULL, NULL, '机械键盘', b'0', 399.00, 400, 0, NULL);
INSERT INTO `products` VALUES (7, NULL, NULL, NULL, '显示器 27寸', b'0', 1299.00, 260, 0, NULL);
INSERT INTO `products` VALUES (8, NULL, NULL, NULL, '移动电源', b'0', 159.00, 600, 0, NULL);
INSERT INTO `products` VALUES (9, NULL, NULL, NULL, '蓝牙耳机', b'0', 299.00, 420, 0, NULL);
INSERT INTO `products` VALUES (10, NULL, NULL, NULL, '智能手表', b'0', 999.00, 380, 0, NULL);
INSERT INTO `products` VALUES (11, NULL, NULL, NULL, '平板电脑', b'0', 2499.00, 180, 0, NULL);
INSERT INTO `products` VALUES (12, NULL, NULL, NULL, '游戏手柄', b'0', 259.00, 500, 0, NULL);
INSERT INTO `products` VALUES (13, NULL, NULL, NULL, 'U 盘 128G', b'0', 89.00, 1000, 0, NULL);
INSERT INTO `products` VALUES (14, NULL, NULL, NULL, '移动硬盘 1T', b'0', 359.00, 340, 0, NULL);
INSERT INTO `products` VALUES (15, NULL, NULL, NULL, '打印机', b'0', 699.00, 130, 0, NULL);

SET FOREIGN_KEY_CHECKS = 1;
