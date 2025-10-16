package com.shopping.server.web;

import com.shopping.server.repository.ClientRepository;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.CrossOrigin;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.HashMap;
import java.util.Map;

@RestController
@RequestMapping("/api/debug")
@CrossOrigin
public class DebugController {

    private final ClientRepository clientRepository;

    public DebugController(ClientRepository clientRepository) {
        this.clientRepository = clientRepository;
    }

    @GetMapping("/clients")
    public ResponseEntity<?> recentClients() {
        Map<String,Object> resp = new HashMap<>();
        resp.put("count", clientRepository.count());
        resp.put("recent", clientRepository.findTop50ByOrderByClientIdDesc());
        return ResponseEntity.ok(resp);
    }
}
