package com.shopping.server.web;

import com.shopping.server.model.Client;
import com.shopping.server.repository.ClientRepository;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.HashMap;
import java.util.Map;

@RestController
@RequestMapping("/api/auth")
@CrossOrigin // 开发阶段放开跨域，生产建议改为白名单
public class AuthController {

    private final ClientRepository clientRepository;

    public AuthController(ClientRepository clientRepository) {
        this.clientRepository = clientRepository;
    }

    @PostMapping("/register")
    public ResponseEntity<Map<String, Object>> register(@RequestBody Map<String, Object> body) {
        String username = str(body.get("username"));
        String password = str(body.get("password"));
        String phone = str(body.get("phone"));

        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "register_response");

        if (isBlank(username) || isBlank(password)) {
            resp.put("success", false);
            resp.put("message", "用户名和密码不能为空");
            return ResponseEntity.badRequest().body(resp);
        }
        if (username.length() < 3 || username.length() > 32) {
            resp.put("success", false);
            resp.put("message", "用户名长度需在 3-32 之间");
            return ResponseEntity.badRequest().body(resp);
        }
        if (password.length() < 6 || password.length() > 64) {
            resp.put("success", false);
            resp.put("message", "密码长度需在 6-64 之间");
            return ResponseEntity.badRequest().body(resp);
        }
        if (clientRepository.existsByUsername(username)) {
            resp.put("success", false);
            resp.put("message", "用户名已存在");
            return ResponseEntity.ok(resp);
        }

        Client c = new Client();
        c.setUsername(username);
        c.setPassword(password); // 演示用明文；生产请改为哈希
        if (!isBlank(phone)) c.setPhone(phone);
        c.setPurchaseCount(0);
        clientRepository.save(c);

        resp.put("success", true);
        resp.put("message", "注册成功");
        return ResponseEntity.ok(resp);
    }

    @PostMapping("/login")
    public ResponseEntity<Map<String, Object>> login(@RequestBody Map<String, Object> body) {
        String username = str(body.get("username"));
        String password = str(body.get("password"));

        Map<String, Object> resp = new HashMap<>();
        resp.put("type", "login_response");
        if (isBlank(username) || isBlank(password)) {
            resp.put("success", false);
            resp.put("message", "用户名或密码不能为空");
            return ResponseEntity.badRequest().body(resp);
        }
        var opt = clientRepository.findByUsername(username);
        boolean ok = opt.isPresent() && Boolean.TRUE.equals(opt.get().getEnabled()) && password.equals(opt.get().getPassword());
        resp.put("success", ok);
        if (!opt.isPresent()) {
            resp.put("message", "用户名或密码错误");
        } else if (Boolean.FALSE.equals(opt.get().getEnabled())) {
            resp.put("message", "该账号已被禁用，请联系管理员");
        } else {
            resp.put("message", ok ? "登录成功" : "用户名或密码错误");
        }
        if (ok) {
            Map<String, Object> user = new HashMap<>();
            user.put("username", opt.get().getUsername());
            user.put("phone", opt.get().getPhone());
            user.put("email", opt.get().getEmail());
            resp.put("user", user);
        }
        return ResponseEntity.ok(resp);
    }

    private static boolean isBlank(String s) { return s == null || s.trim().isEmpty(); }
    private static String str(Object o) { return o == null ? null : String.valueOf(o); }
}
