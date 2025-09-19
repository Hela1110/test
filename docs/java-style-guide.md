# Java 代码规范

## 1. 命名规范

### 1.1 包名
- 全部小写字母
- 使用点号分隔
- 例如：`com.shopping.server.model`

### 1.2 类名
- 使用 PascalCase（大驼峰）
- 名词或名词短语
- 例如：`ProductService`, `OrderRepository`

### 1.3 方法名
- 使用 camelCase（小驼峰）
- 动词或动词短语
- 例如：`getProduct()`, `updateOrder()`

### 1.4 变量名
- 使用 camelCase
- 有意义的名称
- 例如：`productList`, `orderCount`

### 1.5 常量
- 全大写，下划线分隔
- 例如：`MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT`

## 2. 代码格式

### 2.1 缩进
- 使用4个空格缩进
- 不使用制表符（Tab）

### 2.2 行长度
- 最大120个字符
- 超过时进行适当换行

### 2.3 括号
```java
// 正确示例
if (condition) {
    // 代码
}

// 错误示例
if (condition)
{
    // 代码
}
```

### 2.4 空格使用
```java
// 操作符两边加空格
int sum = a + b;

// 方法参数逗号后加空格
method(param1, param2);

// 控制语句关键字后加空格
if (condition) {
}
```

## 3. 编码实践

### 3.1 空值处理
```java
// 使用 Optional
public Optional<Product> findProduct(Long id) {
    return Optional.ofNullable(productRepository.findById(id));
}

// 使用 @NotNull 注解
public void updateProduct(@NotNull Product product) {
    // 实现
}
```

### 3.2 异常处理
```java
try {
    // 可能抛出异常的代码
} catch (SpecificException e) {
    logger.error("具体错误信息", e);
    throw new CustomException("友好的错误信息");
} finally {
    // 清理资源
}
```

### 3.3 日志使用
```java
// 使用 SLF4J
private static final Logger logger = LoggerFactory.getLogger(ClassName.class);

// 使用不同级别的日志
logger.debug("调试信息");
logger.info("一般信息");
logger.warn("警告信息");
logger.error("错误信息", exception);
```

### 3.4 注释规范
```java
/**
 * 类的 JavaDoc 注释
 */
public class ExampleClass {
    /**
     * 方法的 JavaDoc 注释
     * @param param1 参数1的说明
     * @return 返回值说明
     * @throws Exception 异常说明
     */
    public String exampleMethod(String param1) throws Exception {
        // 单行注释
        return result;
    }
}
```

## 4. 最佳实践

### 4.1 SOLID原则
- 单一职责原则 (SRP)
- 开闭原则 (OCP)
- 里氏替换原则 (LSP)
- 接口隔离原则 (ISP)
- 依赖倒置原则 (DIP)

### 4.2 设计模式使用
```java
// 单例模式示例
public class Singleton {
    private static final Singleton INSTANCE = new Singleton();
    
    private Singleton() {}
    
    public static Singleton getInstance() {
        return INSTANCE;
    }
}
```

### 4.3 并发编程
```java
// 使用线程安全的集合
private final Map<String, Object> cache = new ConcurrentHashMap<>();

// 正确使用锁
private final Lock lock = new ReentrantLock();
public void method() {
    lock.lock();
    try {
        // 临界区代码
    } finally {
        lock.unlock();
    }
}
```

### 4.4 测试规范
```java
@Test
public void shouldDoSomething() {
    // 准备测试数据
    Product product = new Product();
    
    // 执行被测试的方法
    Result result = service.process(product);
    
    // 验证结果
    assertThat(result).isNotNull();
    assertThat(result.getValue()).isEqualTo(expectedValue);
}
```

## 5. 项目文件结构
```
src/
├── main/
│   ├── java/
│   │   └── com/
│   │       └── shopping/
│   │           ├── config/
│   │           ├── controller/
│   │           ├── model/
│   │           ├── repository/
│   │           ├── service/
│   │           └── util/
│   └── resources/
│       ├── application.properties
│       └── logback.xml
└── test/
    └── java/
        └── com/
            └── shopping/
                └── [测试类]
```