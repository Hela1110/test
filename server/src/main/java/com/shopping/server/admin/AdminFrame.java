package com.shopping.server.admin;

import com.shopping.server.model.Product;
import com.shopping.server.model.Client;
import com.shopping.server.model.OrderHeader;
import com.shopping.server.model.OrderStatus;
import com.shopping.server.model.ChatMessage;
import com.shopping.server.model.OrderItem;
import com.shopping.server.repository.ProductRepository;
import com.shopping.server.repository.ChatMessageRepository;
import com.shopping.server.repository.OrderHeaderRepository;
import com.shopping.server.repository.OrderItemRepository;
import com.shopping.server.socket.SocketMessageHandler;
import com.shopping.server.repository.ProductTypeRepository;
import com.shopping.server.repository.ClientRepository;

import javax.swing.*;
import javax.swing.table.DefaultTableModel;
import java.awt.*;
import java.math.BigDecimal;
import java.net.URL;
import java.util.List;
import java.util.stream.Collectors;

// XChart
import org.knowm.xchart.PieChart;
import org.knowm.xchart.PieChartBuilder;
import org.knowm.xchart.CategoryChart;
import org.knowm.xchart.CategoryChartBuilder;
import org.knowm.xchart.style.Styler;
import org.knowm.xchart.XChartPanel;

/**
 * 简易的服务端管理控制台：首期仅实现“商品管理”页，支持搜索、增改、删除。
 */
public class AdminFrame extends JFrame {
    private final ProductRepository productRepository;
    private final ChatMessageRepository chatRepo;
    private final OrderHeaderRepository orderHeaderRepo;
    private final OrderItemRepository orderItemRepo;
    private final ProductTypeRepository typeRepo;
    private final ClientRepository clientRepo;

    public AdminFrame(ProductRepository productRepository,
                      ChatMessageRepository chatRepo,
                      OrderHeaderRepository orderHeaderRepo,
                      OrderItemRepository orderItemRepo,
                      ClientRepository clientRepo,
                      ProductTypeRepository typeRepo) {
        super("微商系统 - 服务端管理控制台");
        this.productRepository = productRepository;
        this.chatRepo = chatRepo;
        this.orderHeaderRepo = orderHeaderRepo;
        this.orderItemRepo = orderItemRepo;
    this.clientRepo = clientRepo;
    this.typeRepo = typeRepo;
        setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        setSize(1000, 700);
        setLocationRelativeTo(null);

        JTabbedPane tabs = new JTabbedPane();
        tabs.addTab("商品管理", buildProductsPanel());
        tabs.addTab("聊天/反馈", buildChatPanel());
        tabs.addTab("活动商品", buildPromotionPanel());
        tabs.addTab("数据统计", buildStatsPanel());
        tabs.addTab("系统/用户", buildUsersPanel());
        setContentPane(tabs);
    }

    private JPanel buildProductsPanel() {
        JPanel root = new JPanel(new BorderLayout(8, 8));
        root.setBorder(BorderFactory.createEmptyBorder(8, 8, 8, 8));

        // Top search bar
        JPanel top = new JPanel(new FlowLayout(FlowLayout.LEFT));
        JTextField keyword = new JTextField(24);
        JButton btnSearch = new JButton("搜索");
        JButton btnRefresh = new JButton("刷新");
        top.add(new JLabel("关键字:"));
        top.add(keyword);
        top.add(btnSearch);
        top.add(btnRefresh);
        root.add(top, BorderLayout.NORTH);

        // Table
        String[] cols = {"ID", "名称", "价格", "库存", "在售", "折后价", "图片URL", "描述"};
        DefaultTableModel model = new DefaultTableModel(cols, 0) {
            @Override public boolean isCellEditable(int r, int c) { return false; }
        };
        JTable table = new JTable(model);
        table.setAutoCreateRowSorter(true);
        JScrollPane sp = new JScrollPane(table);
        root.add(sp, BorderLayout.CENTER);

        // Right side form
        JPanel right = new JPanel();
        right.setLayout(new BoxLayout(right, BoxLayout.Y_AXIS));
        JTextField tfId = new JTextField();
        JTextField tfName = new JTextField();
        JTextField tfPrice = new JTextField();
        JTextField tfStock = new JTextField();
        JCheckBox cbOnSale = new JCheckBox("促销中");
        JTextField tfDiscount = new JTextField();
        JTextField tfImage = new JTextField();
        JTextArea taDesc = new JTextArea(6, 20);
        taDesc.setLineWrap(true);
        taDesc.setWrapStyleWord(true);
        JButton btnNew = new JButton("新增/保存");
        JButton btnDelete = new JButton("删除选中");

        right.add(labeled("ID(留空为新增):", tfId));
        right.add(labeled("名称:", tfName));
        right.add(labeled("价格:", tfPrice));
        right.add(labeled("库存:", tfStock));
        right.add(cbOnSale);
        right.add(labeled("折后价(可空):", tfDiscount));
        right.add(labeled("图片URL(可空):", tfImage));
        right.add(labeled("描述(可空):", new JScrollPane(taDesc)));
        right.add(Box.createVerticalStrut(8));
        right.add(btnNew);
        right.add(Box.createVerticalStrut(4));
        right.add(btnDelete);

        root.add(right, BorderLayout.EAST);

        Runnable reloadAll = () -> fillTable(model, productRepository.findAll());
        btnRefresh.addActionListener(e -> reloadAll.run());
        btnSearch.addActionListener(e -> {
            String kw = keyword.getText().trim();
            List<Product> list = kw.isEmpty() ? productRepository.findAll() : productRepository.findByNameContainingIgnoreCase(kw);
            fillTable(model, list);
        });

        table.getSelectionModel().addListSelectionListener(e -> {
            if (e.getValueIsAdjusting()) return;
            int row = table.getSelectedRow();
            if (row < 0) return;
            int idx = table.convertRowIndexToModel(row);
            tfId.setText(String.valueOf(model.getValueAt(idx, 0)));
            tfName.setText(s(model.getValueAt(idx, 1)));
            tfPrice.setText(s(model.getValueAt(idx, 2)));
            tfStock.setText(s(model.getValueAt(idx, 3)));
            cbOnSale.setSelected(Boolean.parseBoolean(s(model.getValueAt(idx, 4))));
            tfDiscount.setText(s(model.getValueAt(idx, 5)));
            tfImage.setText(s(model.getValueAt(idx, 6)));
            taDesc.setText(s(model.getValueAt(idx, 7)));
        });

        btnNew.addActionListener(e -> {
            try {
                String idText = tfId.getText().trim();
                Product p = new Product();
                if (!idText.isEmpty()) {
                    try {
                        long pid = Long.parseLong(idText);
                        p.setProductId(pid);
                        // 更新场景：保留已有销量，避免被 null 覆盖
                        try {
                            var existing = productRepository.findById(pid);
                            existing.ifPresent(ex -> p.setSales(ex.getSales()));
                        } catch (Exception ignoreFind) {}
                    } catch (Exception ignore) {}
                }
                p.setName(tfName.getText().trim());
                p.setPrice(parseBig(tfPrice.getText().trim()));
                p.setStock(parseInt(tfStock.getText().trim()));
                p.setOnSale(cbOnSale.isSelected());
                String dp = tfDiscount.getText().trim();
                p.setDiscountPrice(dp.isEmpty() ? null : parseBig(dp));
                p.setImageUrl(emptyToNull(tfImage.getText()));
                p.setDescription(emptyToNull(taDesc.getText()));
                if (p.getName() == null || p.getName().isEmpty() || p.getPrice() == null) {
                    JOptionPane.showMessageDialog(this, "名称与价格必填", "校验", JOptionPane.WARNING_MESSAGE);
                    return;
                }
                // 新增校验：当勾选“促销中”时，折扣价必须 > 0 且 < 原价
                if (Boolean.TRUE.equals(p.getOnSale())) {
                    if (p.getDiscountPrice() == null) {
                        JOptionPane.showMessageDialog(this, "促销开启时必须填写折扣价", "校验", JOptionPane.WARNING_MESSAGE);
                        return;
                    }
                    if (p.getDiscountPrice().compareTo(BigDecimal.ZERO) <= 0) {
                        JOptionPane.showMessageDialog(this, "折扣价必须大于 0", "校验", JOptionPane.WARNING_MESSAGE);
                        return;
                    }
                    if (p.getPrice() != null && p.getDiscountPrice().compareTo(p.getPrice()) >= 0) {
                        JOptionPane.showMessageDialog(this, "折扣价必须小于原价", "校验", JOptionPane.WARNING_MESSAGE);
                        return;
                    }
                }
                Product saved = productRepository.save(p);
                JOptionPane.showMessageDialog(this, "已保存，ID=" + saved.getProductId());
                fillTable(model, productRepository.findAll());
                tfId.setText(String.valueOf(saved.getProductId()));
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "保存失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        btnDelete.addActionListener(e -> {
            int row = table.getSelectedRow();
            if (row < 0) {
                JOptionPane.showMessageDialog(this, "请先选择要删除的行");
                return;
            }
            int idx = table.convertRowIndexToModel(row);
            Object idObj = model.getValueAt(idx, 0);
            if (idObj == null) return;
            long id = Long.parseLong(String.valueOf(idObj));
            int confirm = JOptionPane.showConfirmDialog(this, "确定删除商品 ID=" + id + "?", "确认", JOptionPane.YES_NO_OPTION);
            if (confirm != JOptionPane.YES_OPTION) return;
            try {
                productRepository.deleteById(id);
                JOptionPane.showMessageDialog(this, "已删除");
                fillTable(model, productRepository.findAll());
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "删除失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        // 首次加载
        fillTable(model, productRepository.findAll());
        return root;
    }

    private static JPanel labeled(String text, JComponent comp) {
        JPanel p = new JPanel(new BorderLayout(4, 4));
        p.add(new JLabel(text), BorderLayout.NORTH);
        p.add(comp, BorderLayout.CENTER);
        return p;
    }

    private JPanel buildChatPanel() {
        JPanel root = new JPanel(new BorderLayout(8, 8));
        root.setBorder(BorderFactory.createEmptyBorder(8, 8, 8, 8));
        DefaultListModel<String> historyModel = new DefaultListModel<>();
        // 额外保存一份实体列表，和显示行一一对应，便于后续解析出订单ID与原始消息
        java.util.List<ChatMessage> historyEntities = new java.util.ArrayList<>();
        JList<String> history = new JList<>(historyModel);
        JTextArea input = new JTextArea(4, 40);
        input.setLineWrap(true);
        JTextField tfFrom = new JTextField("admin", 12);
        JTextField tfTo = new JTextField("", 12);
    JButton btnSend = new JButton("发送");
    JButton btnReload = new JButton("刷新历史");
    JButton btnDelete = new JButton("删除与对方的历史");
    JLabel online = new JLabel("在线用户: ");
    JComboBox<String> onlineCombo = new JComboBox<>();
    onlineCombo.setPrototypeDisplayValue("someverylongusername____");
    // 用颜色区分在线/离线：通过一个可变集合记录在线用户，渲染时着色
    final java.util.Set<String> onlineSet = new java.util.HashSet<>();
    onlineCombo.setRenderer(new javax.swing.DefaultListCellRenderer() {
        @Override
        public java.awt.Component getListCellRendererComponent(javax.swing.JList<?> list, Object value, int index, boolean isSelected, boolean cellHasFocus) {
            java.awt.Component c = super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);
            String u = (value == null) ? "" : String.valueOf(value);
            if (!isSelected) {
                if ("全体".equals(u)) {
                    setForeground(java.awt.Color.DARK_GRAY);
                } else if ("admin".equalsIgnoreCase(u)) {
                    setForeground(new java.awt.Color(52, 152, 219)); // 蓝色
                } else if (onlineSet.contains(u)) {
                    setForeground(new java.awt.Color(34, 139, 34)); // 绿色 在线
                } else {
                    setForeground(new java.awt.Color(128, 128, 128)); // 灰色 离线
                }
            }
            setText(u);
            return c;
        }
    });

        JPanel top = new JPanel(new FlowLayout(FlowLayout.LEFT));
        top.add(new JLabel("发送者:")); top.add(tfFrom);
        top.add(new JLabel("接收者(空为群聊):")); top.add(tfTo);
        top.add(btnReload); top.add(btnDelete);
        root.add(top, BorderLayout.NORTH);

        root.add(new JScrollPane(history), BorderLayout.CENTER);
        JPanel bottom = new JPanel(new BorderLayout(4, 4));
        bottom.add(new JScrollPane(input), BorderLayout.CENTER);
        JPanel ctrl = new JPanel(new FlowLayout(FlowLayout.RIGHT));
    ctrl.add(new JLabel("选择用户:"));
    ctrl.add(onlineCombo);
    ctrl.add(online);
    ctrl.add(btnSend);
        bottom.add(ctrl, BorderLayout.SOUTH);
        root.add(bottom, BorderLayout.SOUTH);

        Runnable reload = () -> {
            historyModel.clear();
            historyEntities.clear();
            java.util.List<ChatMessage> msgs;
            String peerRaw = tfTo.getText().trim();
            String peer = "全体".equals(peerRaw) ? "" : peerRaw;
            if (peer.isEmpty()) {
                msgs = chatRepo.findLatestGlobal(org.springframework.data.domain.PageRequest.of(0, 100));
            } else {
                msgs = chatRepo.findConversation(tfFrom.getText().trim(), peer, org.springframework.data.domain.PageRequest.of(0, 100));
            }
            java.util.Collections.reverse(msgs);
            java.time.format.DateTimeFormatter fmt = java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
            for (ChatMessage m : msgs) {
                String ts = m.getCreatedAt() == null ? "" : m.getCreatedAt().format(fmt);
                historyModel.addElement("[" + ts + "] " + m.getFromUser() + (m.getToUser()==null?" -> 全体":" -> "+m.getToUser()) + ": " + m.getContent());
                historyEntities.add(m);
            }
            // 在线用户集合
            java.util.Set<String> on = SocketMessageHandler.getOnlineUsernames();
            onlineSet.clear(); onlineSet.addAll(on); // 更新渲染器参考集合
            online.setText("在线用户: " + (on.isEmpty()?"(无)":String.join(", ", on)));
            // 刷新下拉：显示“全体”+“admin”+所有注册用户（包括离线）
            String desired = tfTo.getText().trim();
            if (desired.isEmpty()) desired = "全体";
            java.util.Set<String> all = new java.util.TreeSet<>(String.CASE_INSENSITIVE_ORDER);
            try {
                for (var c : clientRepo.findAll()) {
                    String u = String.valueOf(c.getUsername());
                    if (u != null && !u.isBlank()) all.add(u);
                }
            } catch (Exception ignore) {}
            // 合并当前在线（可能包含未在表里的）
            all.addAll(on);
            // 去掉 admin，稍后强制置顶
            all.remove("admin");
            onlineCombo.removeAllItems();
            onlineCombo.addItem("全体");
            onlineCombo.addItem("admin");
            for (String u : all) onlineCombo.addItem(u);
            onlineCombo.setSelectedItem(desired);
        };
        // 选择在线用户即填充接收者并刷新
        onlineCombo.addActionListener(e -> {
            Object sel = onlineCombo.getSelectedItem();
            if (sel == null) return;
            String u = sel.toString();
            if (u.isBlank()) return;
            // 选择“全体”时，上方接收者也显示“全体”，并在 reload 中当作群聊处理
            tfTo.setText("全体".equals(u) ? "全体" : u);
            reload.run();
        });
        btnReload.addActionListener(e -> reload.run());

        btnSend.addActionListener(e -> {
            try {
                ChatMessage m = new ChatMessage();
                m.setFromUser(tfFrom.getText().trim());
                String to = tfTo.getText().trim();
                // "全体" 或 空 都当作群聊（toUser = null）
                if (to.isEmpty() || "全体".equals(to)) m.setToUser(null); else m.setToUser(to);
                m.setContent(input.getText().trim());
                m.setCreatedAt(java.time.LocalDateTime.now());
                if (m.getContent().isEmpty()) return;
                m = chatRepo.save(m);
                SocketMessageHandler.pushChatToTargets(m);
                input.setText("");
                reload.run();
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "发送失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        btnDelete.addActionListener(e -> {
            String peer = tfTo.getText().trim();
            String from = tfFrom.getText().trim();
            if (peer.isEmpty() || "全体".equals(peer)) {
                int res = JOptionPane.showConfirmDialog(this, "确定要清空公共聊天（全体）历史吗？该操作不可撤销。", "确认删除全体", JOptionPane.YES_NO_OPTION);
                if (res != JOptionPane.YES_OPTION) return;
                int deleted = chatRepo.deleteAllGlobalCompat();
                JOptionPane.showMessageDialog(this, "已删除公共聊天条目: " + deleted);
            } else {
                int res = JOptionPane.showConfirmDialog(this, "确定要删除与 "+peer+" 的历史记录吗？该操作不可撤销。", "确认删除", JOptionPane.YES_NO_OPTION);
                if (res != JOptionPane.YES_OPTION) return;
                long n1 = chatRepo.deleteByFromUserAndToUser(from, peer);
                long n2 = chatRepo.deleteByFromUserAndToUser(peer, from);
                JOptionPane.showMessageDialog(this, "已删除条目: " + (n1+n2));
            }
            reload.run();
        });

        // ===== 售后处理辅助：解析包含 ORDER_ID 的消息 =====
        java.util.function.Function<String, Long> parseOrderId = (content) -> {
            if (content == null) return null;
            // 支持两种格式：
            // 1) "[售后申请] ORDER_ID=123"
            // 2) "ORDER_ID=123"（任意位置）
            String mark = "ORDER_ID=";
            int idx = content.indexOf(mark);
            if (idx >= 0) {
                int start = idx + mark.length();
                StringBuilder num = new StringBuilder();
                while (start < content.length()) {
                    char ch = content.charAt(start++);
                    if (Character.isDigit(ch)) num.append(ch); else break;
                }
                try { return Long.parseLong(num.toString()); } catch (Exception ignore) {}
            }
            // 简单 JSON 场景：{"type":"refund_request","orderId":123}
            try {
                if (content.trim().startsWith("{")) {
                    com.fasterxml.jackson.databind.ObjectMapper om = new com.fasterxml.jackson.databind.ObjectMapper();
                    java.util.Map<?,?> m = om.readValue(content, java.util.Map.class);
                    Object t = m.get("type"); Object oid = m.get("orderId");
                    if (t != null && "refund_request".equals(String.valueOf(t)) && oid != null) {
                        return Long.parseLong(String.valueOf(oid));
                    }
                }
            } catch (Exception ignore) {}
            return null;
        };

        // 解析退款原因（来自 JSON）
        java.util.function.Function<String, String> parseRefundReason = (content) -> {
            if (content == null) return null;
            try {
                if (content.trim().startsWith("{")) {
                    com.fasterxml.jackson.databind.ObjectMapper om = new com.fasterxml.jackson.databind.ObjectMapper();
                    java.util.Map<?,?> m = om.readValue(content, java.util.Map.class);
                    Object t = m.get("type"); Object rs = m.get("reason");
                    if (t != null && "refund_request".equals(String.valueOf(t)) && rs != null) {
                        String s = String.valueOf(rs);
                        return (s == null || s.isBlank()) ? null : s;
                    }
                }
            } catch (Exception ignore) {}
            return null;
        };

        // 双击历史记录，若消息中带 ORDER_ID，则弹出订单卡片对话框
        history.addMouseListener(new java.awt.event.MouseAdapter() {
            @Override public void mouseClicked(java.awt.event.MouseEvent e) {
                if (e.getClickCount() != 2) return;
                int idx = history.locationToIndex(e.getPoint());
                if (idx < 0 || idx >= historyEntities.size()) return;
                ChatMessage sel = historyEntities.get(idx);
                Long orderId = parseOrderId.apply(sel.getContent());
                if (orderId == null) return;
                // 展示订单卡片，并传递申请原因（可能为空）
                String refundReason = parseRefundReason.apply(sel.getContent());
                showOrderCardDialog(orderId, sel.getFromUser(), tfTo.getText().trim(), refundReason, reload);
            }
        });

        reload.run();
        return root;
    }

    private JPanel buildPromotionPanel() {
        JPanel root = new JPanel(new BorderLayout(8, 8));
        root.setBorder(BorderFactory.createEmptyBorder(8, 8, 8, 8));
        DefaultTableModel model = new DefaultTableModel(new String[]{"ID","名称","原价","折后价","库存"}, 0){@Override public boolean isCellEditable(int r,int c){return false;}};
        JTable table = new JTable(model);
        JTextField tfSearch = new JTextField(20);
        JButton btnSearch = new JButton("搜索");
        JButton btnList = new JButton("仅显示促销中");
        JTextField tfId = new JTextField(10);
        JTextField tfDiscount = new JTextField(10);
        JButton btnSet = new JButton("设置折扣");
        JButton btnRemove = new JButton("移除折扣");

        JPanel top = new JPanel(new FlowLayout(FlowLayout.LEFT));
        top.add(new JLabel("关键字:")); top.add(tfSearch); top.add(btnSearch); top.add(btnList);
        root.add(top, BorderLayout.NORTH);
        root.add(new JScrollPane(table), BorderLayout.CENTER);
        JPanel bottom = new JPanel(new FlowLayout(FlowLayout.LEFT));
        bottom.add(new JLabel("商品ID:")); bottom.add(tfId);
        bottom.add(new JLabel("折后价:")); bottom.add(tfDiscount);
        bottom.add(btnSet); bottom.add(btnRemove);
        root.add(bottom, BorderLayout.SOUTH);

        Runnable fillAll = () -> {
            model.setRowCount(0);
            for (Product p : productRepository.findAll()) {
                model.addRow(new Object[]{p.getProductId(), p.getName(), p.getPrice(), p.getDiscountPrice(), p.getStock()});
            }
        };
        Runnable fillSale = () -> {
            model.setRowCount(0);
            for (Product p : productRepository.findByOnSaleTrue()) {
                model.addRow(new Object[]{p.getProductId(), p.getName(), p.getPrice(), p.getDiscountPrice(), p.getStock()});
            }
        };
        btnList.addActionListener(e -> fillSale.run());
        btnSearch.addActionListener(e -> {
            String kw = tfSearch.getText().trim();
            java.util.List<Product> list = kw.isEmpty()? productRepository.findAll() : productRepository.findByNameContainingIgnoreCase(kw);
            model.setRowCount(0);
            for (Product p : list) model.addRow(new Object[]{p.getProductId(), p.getName(), p.getPrice(), p.getDiscountPrice(), p.getStock()});
        });
        btnSet.addActionListener(e -> {
            try {
                long id = Long.parseLong(tfId.getText().trim());
                java.math.BigDecimal dp = new java.math.BigDecimal(tfDiscount.getText().trim());
                Product p = productRepository.findById(id).orElseThrow();
                if (dp.compareTo(java.math.BigDecimal.ZERO) <= 0 || dp.compareTo(p.getPrice()) >= 0) {
                    JOptionPane.showMessageDialog(this, "折扣价需大于0且小于原价"); return;
                }
                p.setOnSale(true); p.setDiscountPrice(dp);
                productRepository.save(p);
                JOptionPane.showMessageDialog(this, "已设置折扣");
                fillSale.run();
            } catch (Exception ex) {
                ex.printStackTrace(); JOptionPane.showMessageDialog(this, "设置失败: "+ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });
        btnRemove.addActionListener(e -> {
            try {
                long id = Long.parseLong(tfId.getText().trim());
                Product p = productRepository.findById(id).orElseThrow();
                p.setOnSale(false); p.setDiscountPrice(null);
                productRepository.save(p);
                JOptionPane.showMessageDialog(this, "已移除折扣");
                fillSale.run();
            } catch (Exception ex) {
                ex.printStackTrace(); JOptionPane.showMessageDialog(this, "移除失败: "+ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        fillAll.run();
        return root;
    }

    private JPanel buildStatsPanel() {
        JPanel root = new JPanel(new BorderLayout(8, 8));
        root.setBorder(BorderFactory.createEmptyBorder(8, 8, 8, 8));
        JTextField tfStart = new JTextField("2025-01-01", 12);
        JTextField tfEnd = new JTextField(java.time.LocalDate.now().toString(), 12);
        JComboBox<String> cbUser = new JComboBox<>();
        cbUser.setEditable(true); // 可手输
        JComboBox<String> cbCategory = new JComboBox<>();
        cbCategory.setEditable(true);
        JButton btnLoad = new JButton("加载");
        JButton btnExportMonthly = new JButton("导出CSV(月度)");
        JButton btnExportProduct = new JButton("导出CSV(商品)");

        String[] mcols = {"年","月","金额(总)"};
        DefaultTableModel mModel = new DefaultTableModel(mcols, 0){@Override public boolean isCellEditable(int r,int c){return false;}};
        JTable tblMonthly = new JTable(mModel);

    String[] pcols = {"商品ID","名称","销量","销售额"};
        DefaultTableModel pModel = new DefaultTableModel(pcols, 0){@Override public boolean isCellEditable(int r,int c){return false;}};
        JTable tblProduct = new JTable(pModel);

        JPanel top = new JPanel(new FlowLayout(FlowLayout.LEFT));
        top.add(new JLabel("开始:")); top.add(tfStart);
        top.add(new JLabel("结束:")); top.add(tfEnd);
        top.add(new JLabel("用户(可选):")); top.add(cbUser);
        top.add(new JLabel("类别(可选):")); top.add(cbCategory);
    JButton btnAllOrders = new JButton("查看所有订单");
    top.add(btnLoad); top.add(btnAllOrders); top.add(btnExportMonthly); top.add(btnExportProduct);
        root.add(top, BorderLayout.NORTH);

        // 左侧表格（上：月度，下：商品）
        JSplitPane leftSplit = new JSplitPane(JSplitPane.VERTICAL_SPLIT, new JScrollPane(tblMonthly), new JScrollPane(tblProduct));
        leftSplit.setDividerLocation(260);

        // 右侧图表：上 饼图（商品销售额 Top N），下 折线图（月度金额）
        PieChart pie = new PieChartBuilder().width(400).height(260).title("商品销售额占比").build();
        pie.getStyler().setLegendVisible(true);
        CategoryChart line = new CategoryChartBuilder().width(400).height(260).title("月度销售趋势").xAxisTitle("月份").yAxisTitle("金额").build();
        line.getStyler().setLegendPosition(Styler.LegendPosition.InsideNE);
        XChartPanel<PieChart> piePanel = new XChartPanel<>(pie);
    XChartPanel<CategoryChart> linePanel = new XChartPanel<>(line);
        JSplitPane rightSplit = new JSplitPane(JSplitPane.VERTICAL_SPLIT, piePanel, linePanel);
        rightSplit.setDividerLocation(260);

        JSplitPane mainSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, leftSplit, rightSplit);
        mainSplit.setDividerLocation(560);
        root.add(mainSplit, BorderLayout.CENTER);

        // 初始化下拉：用户（全部用户名）、类别（去重 type_name）
        SwingUtilities.invokeLater(() -> {
            cbUser.addItem("");
            for (var c : clientRepo.findAll()) cbUser.addItem(String.valueOf(c.getUsername()));
            cbCategory.addItem("");
            try {
                var typeNames = typeRepo.findAll().stream()
                        .map(t -> t.getTypeName())
                        .filter(n -> n != null && !n.isBlank())
                        .distinct().sorted()
                        .collect(Collectors.toList());
                for (var n : typeNames) cbCategory.addItem(n);
            } catch (Exception ignore) {}
        });

        btnLoad.addActionListener(e -> {
            try {
                java.time.LocalDate start = java.time.LocalDate.parse(tfStart.getText().trim());
                java.time.LocalDate end = java.time.LocalDate.parse(tfEnd.getText().trim());
                var startDt = start.atStartOfDay();
                var endDt = end.plusDays(1).atStartOfDay().minusSeconds(1);
                // 月度统计：未选用户 -> 全体；选了用户 -> 仅该用户
                mModel.setRowCount(0);
                java.util.List<Object[]> monthly = new java.util.ArrayList<>();
                String u = String.valueOf(cbUser.getEditor().getItem()).trim();
                if (u.isEmpty()) {
                    for (Object[] row : orderHeaderRepo.sumByMonth(startDt, endDt)) { monthly.add(row); mModel.addRow(new Object[]{row[0], row[1], row[2]}); }
                } else {
                    var cOpt = clientRepo.findByUsername(u);
                    if (cOpt.isPresent()) {
                        for (Object[] row : orderHeaderRepo.sumByMonthForClient(cOpt.get(), startDt, endDt)) { monthly.add(row); mModel.addRow(new Object[]{row[0], row[1], row[2]}); }
                    }
                }
                // 商品维度：未选用户 -> 全体；选了用户 -> 仅该用户
                pModel.setRowCount(0);
                String cate = String.valueOf(cbCategory.getEditor().getItem()).trim();
                boolean filterByCate = !cate.isEmpty();
                java.util.Map<String, java.math.BigDecimal> amountByName = new java.util.LinkedHashMap<>();
                java.util.List<Object[]> productRows;
                if (u.isEmpty()) productRows = orderItemRepo.sumSalesByProduct(startDt, endDt);
                else {
                    var cOpt = clientRepo.findByUsername(u);
                    productRows = cOpt.isPresent()? orderItemRepo.sumSalesByProductForClient(cOpt.get(), startDt, endDt) : java.util.List.of();
                }
                for (Object[] row : productRows) {
                    // row: productId, name, qty, amount
                    Long pid = (row[0] instanceof Long) ? (Long) row[0] : Long.parseLong(String.valueOf(row[0]));
                    if (filterByCate) {
                        try {
                            var prodOpt = productRepository.findById(pid);
                            if (prodOpt.isPresent()) {
                                var types = typeRepo.findByProduct(prodOpt.get());
                                boolean any = false;
                                for (var t : types) { if (t.getTypeName() != null && t.getTypeName().equalsIgnoreCase(cate)) { any = true; break; } }
                                if (!any) continue; // 不匹配类别，跳过
                            }
                        } catch (Exception ignore) {}
                    }
                    pModel.addRow(new Object[]{row[0], row[1], row[2], row[3]});
                    // 聚合用于饼图（按商品名）
                    String name = String.valueOf(row[1]);
                    java.math.BigDecimal amt = (row[3] instanceof java.math.BigDecimal) ? (java.math.BigDecimal) row[3] : new java.math.BigDecimal(String.valueOf(row[3]));
                    amountByName.merge(name, amt, java.math.BigDecimal::add);
                }

                // 刷新饼图（显示 Top 6 + 其它）
                pie.getSeriesMap().clear();
                var entries = amountByName.entrySet().stream()
                        .sorted((a,b) -> b.getValue().compareTo(a.getValue()))
                        .collect(Collectors.toList());
                java.math.BigDecimal others = java.math.BigDecimal.ZERO;
                int limit = 6;
                for (int i=0;i<entries.size();i++) {
                    var e2 = entries.get(i);
                    if (i < limit) {
                        pie.addSeries(e2.getKey(), e2.getValue());
                    } else {
                        others = others.add(e2.getValue());
                    }
                }
                if (others.compareTo(java.math.BigDecimal.ZERO) > 0) pie.addSeries("其它", others);
                piePanel.revalidate(); piePanel.repaint();

                // 刷新折线图（按 year-month 汇总）
                line.getSeriesMap().clear();
                java.util.Map<String, java.math.BigDecimal> byYm = new java.util.LinkedHashMap<>();
                for (Object[] r : monthly) {
                    String ym = r[0] + "-" + String.valueOf(r[1]);
                    java.math.BigDecimal amt = (r[2] instanceof java.math.BigDecimal) ? (java.math.BigDecimal) r[2] : new java.math.BigDecimal(String.valueOf(r[2]));
                    byYm.merge(ym, amt, java.math.BigDecimal::add);
                }
                java.util.List<String> x = new java.util.ArrayList<>(byYm.keySet());
                java.util.List<Double> y = byYm.values().stream().map(v -> v.doubleValue()).collect(Collectors.toList());
                line.addSeries("金额", x, y);
                linePanel.revalidate(); linePanel.repaint();
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "加载失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        // 导出 CSV（简易实现）
        btnExportMonthly.addActionListener(e -> {
            JFileChooser chooser = new JFileChooser();
            chooser.setDialogTitle("保存月度统计 CSV");
            if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
                java.io.File f = chooser.getSelectedFile();
                // 自动补全 .csv 扩展名
                if (f != null) {
                    String path = f.getAbsolutePath();
                    if (!path.toLowerCase().endsWith(".csv")) {
                        f = new java.io.File(path + ".csv");
                    }
                }
             if (f == null) return;
             try (java.io.FileOutputStream fos = new java.io.FileOutputStream(f);
                     java.io.OutputStreamWriter osw = new java.io.OutputStreamWriter(fos, java.nio.charset.StandardCharsets.UTF_8);
                     java.io.PrintWriter pw = new java.io.PrintWriter(osw)) {
                    // 写入 UTF-8 BOM，便于 Windows Excel 正确识别编码
                    pw.print('\uFEFF');
                    // 表头与行数据，使用 CRLF 结尾
                    pw.print("year,month,total_amount\r\n");
                    for (int i=0;i<mModel.getRowCount();++i) {
                        pw.print(String.valueOf(mModel.getValueAt(i,0)) + "," +
                                 String.valueOf(mModel.getValueAt(i,1)) + "," +
                                 String.valueOf(mModel.getValueAt(i,2)) + "\r\n");
                    }
                    pw.flush();
                    JOptionPane.showMessageDialog(this, "已导出: "+f.getAbsolutePath());
                } catch (Exception ex) {
                    ex.printStackTrace(); JOptionPane.showMessageDialog(this, "导出失败: "+ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
                }
            }
        });
        btnExportProduct.addActionListener(e -> {
            JFileChooser chooser = new JFileChooser();
            chooser.setDialogTitle("保存商品统计 CSV");
            if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
                java.io.File f = chooser.getSelectedFile();
                // 自动补全 .csv 扩展名
                if (f != null) {
                    String path = f.getAbsolutePath();
                    if (!path.toLowerCase().endsWith(".csv")) {
                        f = new java.io.File(path + ".csv");
                    }
                }
             if (f == null) return;
             try (java.io.FileOutputStream fos = new java.io.FileOutputStream(f);
                     java.io.OutputStreamWriter osw = new java.io.OutputStreamWriter(fos, java.nio.charset.StandardCharsets.UTF_8);
                     java.io.PrintWriter pw = new java.io.PrintWriter(osw)) {
                    // 写入 UTF-8 BOM
                    pw.print('\uFEFF');
                    pw.print("product_id,name,quantity,amount\r\n");
                    for (int i=0;i<pModel.getRowCount();++i) {
                        // 简单转义逗号
                        String name = String.valueOf(pModel.getValueAt(i,1)).replace(","," ");
                        pw.print(String.valueOf(pModel.getValueAt(i,0)) + "," +
                                 name + "," +
                                 String.valueOf(pModel.getValueAt(i,2)) + "," +
                                 String.valueOf(pModel.getValueAt(i,3)) + "\r\n");
                    }
                    pw.flush();
                    JOptionPane.showMessageDialog(this, "已导出: "+f.getAbsolutePath());
                } catch (Exception ex) {
                    ex.printStackTrace(); JOptionPane.showMessageDialog(this, "导出失败: "+ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
                }
            }
        });

        // 查看所有订单：弹窗表格
        btnAllOrders.addActionListener(e -> {
            try {
                String[] cols = {"订单ID","用户名","状态","金额","下单时间"};
                DefaultTableModel m = new DefaultTableModel(cols, 0){ @Override public boolean isCellEditable(int r,int c){ return false; } };

                // 构建对话框 UI：上方过滤器 + 按钮，中央表格，底部按钮
                JDialog dlg = new JDialog(this, "所有订单（可筛选）", true);
                dlg.setSize(900, 600);
                dlg.setLocationRelativeTo(this);
                dlg.setLayout(new BorderLayout(6,6));

                JPanel north = new JPanel(new FlowLayout(FlowLayout.LEFT));
                JTextField fStart = new JTextField(12); fStart.setText(java.time.LocalDate.now().minusMonths(1).toString());
                JTextField fEnd = new JTextField(12); fEnd.setText(java.time.LocalDate.now().toString());
                JTextField fUser = new JTextField(12);
                JComboBox<String> cbStatus = new JComboBox<>(new String[]{"", "CART", "PAID", "CANCELLED", "DELETED_BY_CLIENT", "REFUNDED"});
                JButton btnQuery = new JButton("查询");
                JButton btnExport = new JButton("导出CSV");
                north.add(new JLabel("开始:")); north.add(fStart);
                north.add(new JLabel("结束:")); north.add(fEnd);
                north.add(new JLabel("用户(模糊):")); north.add(fUser);
                north.add(new JLabel("状态:")); north.add(cbStatus);
                north.add(btnQuery); north.add(btnExport);
                dlg.add(north, BorderLayout.NORTH);

                JTable t = new JTable(m);
                t.setAutoCreateRowSorter(true);
                dlg.add(new JScrollPane(t), BorderLayout.CENTER);

                JPanel south = new JPanel(new FlowLayout(FlowLayout.RIGHT));
                JButton close = new JButton("关闭"); close.addActionListener(ev -> dlg.dispose());
                south.add(close); dlg.add(south, BorderLayout.SOUTH);

                // 查询动作
                Runnable doQuery = () -> {
                    try {
                        java.time.LocalDateTime startDt = null, endDt = null;
                        String sv = fStart.getText().trim(); if (!sv.isEmpty()) startDt = java.time.LocalDate.parse(sv).atStartOfDay();
                        String ev = fEnd.getText().trim(); if (!ev.isEmpty()) endDt = java.time.LocalDate.parse(ev).plusDays(1).atStartOfDay().minusSeconds(1);
                        String user = fUser.getText().trim(); if (user.isEmpty()) user = null;
                        String st = String.valueOf(cbStatus.getSelectedItem()); if (st != null && st.isBlank()) st = null;
                        com.shopping.server.model.OrderStatus status = (st==null) ? null : com.shopping.server.model.OrderStatus.valueOf(st);
                        java.util.List<Object[]> rows = orderHeaderRepo.findOrderSummaries(startDt, endDt, user, status);
                        m.setRowCount(0);
                        for (Object[] r : rows) m.addRow(new Object[]{ r[0], r[1], r[2], r[3], r[4] });
                    } catch (Exception ex2) {
                        ex2.printStackTrace();
                        JOptionPane.showMessageDialog(dlg, "查询失败: " + ex2.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
                    }
                };
                btnQuery.addActionListener(ev -> doQuery.run());

                // 导出 CSV（按当前表格内容）
                btnExport.addActionListener(ev -> {
                    JFileChooser chooser = new JFileChooser();
                    chooser.setDialogTitle("保存订单列表 CSV");
                    if (chooser.showSaveDialog(dlg) == JFileChooser.APPROVE_OPTION) {
                        java.io.File f = chooser.getSelectedFile();
                        if (f != null) { String path = f.getAbsolutePath(); if (!path.toLowerCase().endsWith(".csv")) f = new java.io.File(path + ".csv"); }
                        if (f == null) return;
                        try (java.io.FileOutputStream fos = new java.io.FileOutputStream(f);
                             java.io.OutputStreamWriter osw = new java.io.OutputStreamWriter(fos, java.nio.charset.StandardCharsets.UTF_8);
                             java.io.PrintWriter pw = new java.io.PrintWriter(osw)) {
                            pw.print('\uFEFF');
                            pw.print("order_id,username,status,total,created_at\r\n");
                            for (int i=0;i<m.getRowCount();++i) {
                                pw.print(String.valueOf(m.getValueAt(i,0)) + "," +
                                         String.valueOf(m.getValueAt(i,1)) + "," +
                                         String.valueOf(m.getValueAt(i,2)) + "," +
                                         String.valueOf(m.getValueAt(i,3)) + "," +
                                         String.valueOf(m.getValueAt(i,4)) + "\r\n");
                            }
                            pw.flush();
                            JOptionPane.showMessageDialog(dlg, "已导出: " + f.getAbsolutePath());
                        } catch (Exception ex3) {
                            ex3.printStackTrace();
                            JOptionPane.showMessageDialog(dlg, "导出失败: " + ex3.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
                        }
                    }
                });

                // 打开弹窗并加载一次默认查询
                dlg.setVisible(true);
                doQuery.run();
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "加载订单失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        // 初始触发一次
        btnLoad.doClick();
        return root;
    }

    // 用户管理页：列出所有用户，双击查看详情
    private JPanel buildUsersPanel() {
        JPanel root = new JPanel(new BorderLayout(8, 8));
        root.setBorder(BorderFactory.createEmptyBorder(8, 8, 8, 8));

        // 顶部：搜索与刷新
        JPanel top = new JPanel(new FlowLayout(FlowLayout.LEFT));
        JTextField keyword = new JTextField(22);
        JButton btnSearch = new JButton("搜索");
        JButton btnRefresh = new JButton("刷新");
        top.add(new JLabel("关键字(用户名/邮箱/手机号):"));
        top.add(keyword);
        top.add(btnSearch);
        top.add(btnRefresh);
        root.add(top, BorderLayout.NORTH);

        // 表格：只读
        String[] cols = {"ID", "用户名", "邮箱", "手机号", "购买次数", "状态"};
        DefaultTableModel model = new DefaultTableModel(cols, 0) {
            @Override public boolean isCellEditable(int r, int c) { return false; }
        };
        JTable table = new JTable(model);
        table.setAutoCreateRowSorter(true);
        table.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        JScrollPane sp = new JScrollPane(table);
        root.add(sp, BorderLayout.CENTER);

        // 底部：分页与启用/禁用按钮
        JPanel bottom = new JPanel(new FlowLayout(FlowLayout.LEFT));
        JComboBox<Integer> pageSize = new JComboBox<>(new Integer[]{10, 20, 50, 100});
        pageSize.setSelectedItem(20);
        JButton prev = new JButton("上一页");
        JButton next = new JButton("下一页");
        JLabel pageInfo = new JLabel("第 1/1 页，共 0 条");
        JButton toggle = new JButton("禁用/启用");
        bottom.add(new JLabel("每页:")); bottom.add(pageSize);
        bottom.add(prev); bottom.add(next); bottom.add(pageInfo);
        bottom.add(Box.createHorizontalStrut(18)); bottom.add(toggle);
        root.add(bottom, BorderLayout.SOUTH);

        final int[] currentPage = {0}; // 从 0 开始
        final long[] totalItems = {0};

        java.util.function.Consumer<String> doLoad = (kw) -> {
            try {
                int size = (Integer) pageSize.getSelectedItem();
                var pageable = org.springframework.data.domain.PageRequest.of(currentPage[0], size, org.springframework.data.domain.Sort.by("clientId").descending());
                org.springframework.data.domain.Page<Client> page;
                if (kw != null && !kw.isBlank()) page = clientRepo.searchByKeyword(kw.trim(), pageable); else page = clientRepo.findAll(pageable);
                model.setRowCount(0);
                for (Client c : page.getContent()) {
                    Integer pc = c.getPurchaseCount();
                    long dyn = 0L;
                    if (pc == null || pc == 0) {
                        try { dyn = orderHeaderRepo.countPaidByClient(c); } catch (Exception ignore) {}
                    }
                    int show = (pc != null && pc > 0) ? pc : (int) dyn;
                    model.addRow(new Object[]{ c.getClientId(), c.getUsername(), c.getEmail(), c.getPhone(), show, (c.getEnabled()!=null && c.getEnabled())?"启用":"禁用" });
                }
                totalItems[0] = page.getTotalElements();
                int totalPages = Math.max(1, page.getTotalPages());
                int showPage = Math.min(currentPage[0]+1, totalPages);
                pageInfo.setText("第 " + showPage + "/" + totalPages + " 页，共 " + totalItems[0] + " 条");
                prev.setEnabled(currentPage[0] > 0);
                next.setEnabled(currentPage[0] < totalPages - 1);
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "加载用户失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        };

        btnRefresh.addActionListener(e -> { currentPage[0] = 0; doLoad.accept(keyword.getText()); });
        btnSearch.addActionListener(e -> {
            String kw = keyword.getText().trim().toLowerCase();
            currentPage[0] = 0; doLoad.accept(kw);
        });

        pageSize.addActionListener(e -> { currentPage[0] = 0; doLoad.accept(keyword.getText()); });
        prev.addActionListener(e -> { if (currentPage[0] > 0) currentPage[0]--; doLoad.accept(keyword.getText()); });
        next.addActionListener(e -> { currentPage[0]++; doLoad.accept(keyword.getText()); });

        toggle.addActionListener(e -> {
            int row = table.getSelectedRow();
            if (row < 0) { JOptionPane.showMessageDialog(this, "请先选择一名用户"); return; }
            int idx = table.convertRowIndexToModel(row);
            Object idObj = model.getValueAt(idx, 0);
            if (idObj == null) return;
            long cid = Long.parseLong(String.valueOf(idObj));
            try {
                var opt = clientRepo.findById(cid);
                if (opt.isEmpty()) { JOptionPane.showMessageDialog(this, "用户不存在"); return; }
                Client c = opt.get();
                boolean cur = Boolean.TRUE.equals(c.getEnabled());
                c.setEnabled(!cur);
                clientRepo.save(c);
                JOptionPane.showMessageDialog(this, (c.getEnabled()?"已启用: ":"已禁用: ") + c.getUsername());
                doLoad.accept(keyword.getText());
            } catch (Exception ex) {
                ex.printStackTrace();
                JOptionPane.showMessageDialog(this, "操作失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
            }
        });

        // 双击查看详情
        table.addMouseListener(new java.awt.event.MouseAdapter() {
            @Override public void mouseClicked(java.awt.event.MouseEvent e) {
                if (e.getClickCount() != 2) return;
                int row = table.getSelectedRow();
                if (row < 0) return;
                int idx = table.convertRowIndexToModel(row);
                Object idObj = model.getValueAt(idx, 0);
                if (idObj == null) return;
                long cid = Long.parseLong(String.valueOf(idObj));
                showClientDetailDialog(cid);
            }
        });

        // 初次加载
        doLoad.accept(keyword.getText());
        return root;
    }

    // 用户详情对话框
    private void showClientDetailDialog(long clientId) {
        try {
            var opt = clientRepo.findById(clientId);
            if (opt.isEmpty()) {
                JOptionPane.showMessageDialog(this, "未找到该用户: " + clientId, "提示", JOptionPane.WARNING_MESSAGE);
                return;
            }
            Client c = opt.get();

            JDialog dlg = new JDialog(this, "用户详情 - " + String.valueOf(c.getUsername()), true);
            dlg.setSize(460, 420);
            dlg.setLocationRelativeTo(this);
            dlg.setLayout(new BorderLayout(8,8));

            JPanel center = new JPanel();
            center.setLayout(new BoxLayout(center, BoxLayout.Y_AXIS));
            center.add(line("用户ID:", String.valueOf(c.getClientId())));
            center.add(line("用户名:", String.valueOf(c.getUsername())));
            center.add(line("邮箱:", nullToDash(c.getEmail())));
            center.add(line("手机号:", nullToDash(c.getPhone())));
            {
                Integer pc = c.getPurchaseCount();
                String v = (pc == null || pc == 0) ? null : String.valueOf(pc);
                if (v == null) {
                    try { v = String.valueOf(orderHeaderRepo.countPaidByClient(c)); } catch (Exception ignore) { v = "0"; }
                }
                center.add(line("购买次数:", v));
            }

            // 头像（若有）
            String av = c.getAvatar();
            if (av != null && !av.isBlank()) {
                try {
                    JLabel img = new JLabel();
                    img.setHorizontalAlignment(SwingConstants.CENTER);
                    img.setBorder(BorderFactory.createTitledBorder("头像预览"));
                    ImageIcon ico = new ImageIcon(new URL(av));
                    // 尝试缩放到较小尺寸
                    Image scaled = ico.getImage().getScaledInstance(120, 120, Image.SCALE_SMOOTH);
                    img.setIcon(new ImageIcon(scaled));
                    center.add(Box.createVerticalStrut(6));
                    center.add(img);
                } catch (Exception ignore) {
                    center.add(line("头像URL:", av));
                }
            }

            dlg.add(new JScrollPane(center), BorderLayout.CENTER);
            JPanel south = new JPanel(new FlowLayout(FlowLayout.RIGHT));
            JButton btnClose = new JButton("关闭");
            btnClose.addActionListener(e -> dlg.dispose());
            south.add(btnClose);
            dlg.add(south, BorderLayout.SOUTH);

            dlg.setVisible(true);
        } catch (Exception ex) {
            ex.printStackTrace();
            JOptionPane.showMessageDialog(this, "无法展示用户详情: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
        }
    }

    // 小工具：一行 label-value 展示
    private JPanel line(String label, String value) {
        JPanel p = new JPanel(new BorderLayout(6, 6));
        p.add(new JLabel(label), BorderLayout.WEST);
        JTextField tf = new JTextField(value == null ? "-" : value);
        tf.setEditable(false);
        p.add(tf, BorderLayout.CENTER);
        return p;
    }

    private static String nullToDash(String s) {
        return (s == null || s.isBlank()) ? "-" : s;
    }

    // 弹出“订单卡片”对话框：展示订单详情，并提供“退款/驳回”操作
    private void showOrderCardDialog(Long orderId, String requestFromUser, String currentToField, String refundReason, Runnable reloadHistory) {
        try {
            // 使用 fetch join 一次性抓取 client/items/product，避免延迟加载导致的 no Session
            var opt = orderHeaderRepo.fetchByIdWithItemsAndClient(orderId);
            if (opt.isEmpty()) {
                JOptionPane.showMessageDialog(this, "未找到该订单: " + orderId, "提示", JOptionPane.WARNING_MESSAGE);
                return;
            }
            OrderHeader header = opt.get();

            JDialog dlg = new JDialog(this, "售后申请 - 订单#" + orderId, true);
            dlg.setSize(600, 480);
            dlg.setLocationRelativeTo(this);
            dlg.setLayout(new BorderLayout(8, 8));

        // 顶部信息（使用 HTML 分两行展示，避免单行过长被截断）
        String topHtml = String.format(
            "<html><div style='padding:6px 8px;'>" +
            "<b>订单ID:</b> %d &nbsp;&nbsp; <b>用户:</b> %s &nbsp;&nbsp; <b>状态:</b> %s &nbsp;&nbsp; <b>金额:</b> %s" +
            "<br/><b>下单时间:</b> %s" +
            "</div></html>",
            header.getId(),
            header.getClient() == null ? "-" : String.valueOf(header.getClient().getUsername()),
            String.valueOf(header.getStatus()),
            String.valueOf(header.getTotalPrice()),
            String.valueOf(header.getCreatedAt()));
        JLabel topLabel = new JLabel(topHtml);
        dlg.add(topLabel, BorderLayout.NORTH);

            // 明细表
            String[] cols = {"商品ID", "名称", "数量", "单价", "小计"};
            var model = new DefaultTableModel(cols, 0) { @Override public boolean isCellEditable(int r,int c){ return false; } };
            JTable tbl = new JTable(model);
            dlg.add(new JScrollPane(tbl), BorderLayout.CENTER);

            // items 已通过 fetch join 初始化，优先直接读取；如为空再通过仓库查询兜底
            java.util.List<OrderItem> items = header.getItems();
            if (items == null || items.isEmpty()) {
                try { items = orderItemRepo.findByOrder(header); } catch (Exception ignore) {}
            }
            if (items != null) {
                for (OrderItem it : items) {
                    Long pid = null; String name = "-"; java.math.BigDecimal price = java.math.BigDecimal.ZERO; Integer qty = 0;
                    try {
                        if (it.getProduct() != null) {
                            pid = it.getProduct().getProductId();
                            name = it.getProduct().getName();
                        }
                        price = it.getPrice();
                        qty = it.getQuantity();
                    } catch (Exception ignore) {}
                    java.math.BigDecimal subtotal = (price == null || qty == null) ? java.math.BigDecimal.ZERO : price.multiply(java.math.BigDecimal.valueOf(qty));
                    model.addRow(new Object[]{pid, name, qty, price, subtotal});
                }
            }

            // SOUTH：退款原因 + 按钮
            JPanel south = new JPanel(new BorderLayout(4,4));
            String reasonText = (refundReason == null || refundReason.isBlank()) ? "(无)" : refundReason;
            JLabel reasonLbl = new JLabel("<html><div style='padding:4px 6px;'><b>退款原因:</b> " +
                                         escapeHtml(reasonText) +
                                         "</div></html>");
            // 按钮行
            JPanel bottom = new JPanel(new FlowLayout(FlowLayout.RIGHT));
            JButton btnRefund = new JButton("退款");
            JButton btnReject = new JButton("驳回");
            JButton btnClose = new JButton("关闭");
            bottom.add(btnReject); bottom.add(btnRefund); bottom.add(btnClose);
            south.add(reasonLbl, BorderLayout.CENTER);
            south.add(bottom, BorderLayout.SOUTH);
            dlg.add(south, BorderLayout.SOUTH);

            // 工具函数：向用户发送系统消息
            java.util.function.Consumer<String> notifyUser = (text) -> {
                try {
                    ChatMessage m = new ChatMessage();
                    m.setFromUser("admin");
                    String to = (requestFromUser == null || requestFromUser.isBlank() || "全体".equals(requestFromUser)) ? null : requestFromUser;
                    m.setToUser(to); // 如果为 null 则群发；我们更倾向于定向发送
                    m.setContent(text);
                    m.setCreatedAt(java.time.LocalDateTime.now());
                    m = chatRepo.save(m);
                    SocketMessageHandler.pushChatToTargets(m);
                } catch (Exception ex) {
                    ex.printStackTrace();
                }
            };

            btnRefund.addActionListener(ev -> {
                try {
                    // 仅支持已支付订单退款
                    if (header.getStatus() != OrderStatus.PAID) {
                        JOptionPane.showMessageDialog(dlg, "该订单当前状态不支持退款: " + header.getStatus());
                        return;
                    }
                    int confirm = JOptionPane.showConfirmDialog(dlg, "确定要退款并回滚库存/销量吗？", "确认退款", JOptionPane.YES_NO_OPTION);
                    if (confirm != JOptionPane.YES_OPTION) return;

                    // 回滚库存与销量
                    java.util.List<OrderItem> its = orderItemRepo.findByOrder(header);
                    for (OrderItem it : its) {
                        if (it.getProduct() == null) continue;
                        Long pid = it.getProduct().getProductId();
                        var prodOpt = productRepository.findById(pid);
                        if (prodOpt.isPresent()) {
                            Product p = prodOpt.get();
                            int stock = p.getStock() == null ? 0 : p.getStock();
                            int sales = p.getSales() == null ? 0 : p.getSales();
                            int qty = it.getQuantity() == null ? 0 : it.getQuantity();
                            p.setStock(stock + qty);
                            int newSales = sales - qty; if (newSales < 0) newSales = 0;
                            p.setSales(newSales);
                            productRepository.save(p);
                        }
                    }

                    // 更新订单状态
                    header.setStatus(OrderStatus.REFUNDED);
                    orderHeaderRepo.save(header);

                    // 通知用户
                    notifyUser.accept("[售后结果] 订单#" + orderId + " 已退款，感谢理解。");

                    JOptionPane.showMessageDialog(dlg, "已退款并回滚库存/销量");
                    dlg.dispose();
                    if (reloadHistory != null) reloadHistory.run();
                } catch (Exception ex) {
                    ex.printStackTrace();
                    JOptionPane.showMessageDialog(dlg, "退款失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
                }
            });

            btnReject.addActionListener(ev -> {
                try {
                    int confirm = JOptionPane.showConfirmDialog(dlg, "确定要驳回该售后申请吗？", "确认驳回", JOptionPane.YES_NO_OPTION);
                    if (confirm != JOptionPane.YES_OPTION) return;
                    // 驳回不修改订单，仅通知
                    notifyUser.accept("[售后结果] 订单#" + orderId + " 的退款申请已被驳回。");
                    JOptionPane.showMessageDialog(dlg, "已驳回");
                    dlg.dispose();
                    if (reloadHistory != null) reloadHistory.run();
                } catch (Exception ex) {
                    ex.printStackTrace();
                    JOptionPane.showMessageDialog(dlg, "操作失败: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
                }
            });

            btnClose.addActionListener(ev -> dlg.dispose());

            dlg.setVisible(true);
        } catch (Exception ex) {
            ex.printStackTrace();
            JOptionPane.showMessageDialog(this, "无法展示订单卡片: " + ex.getMessage(), "错误", JOptionPane.ERROR_MESSAGE);
        }
    }

    private static void fillTable(DefaultTableModel model, List<Product> list) {
        model.setRowCount(0);
        for (Product p : list) {
            model.addRow(new Object[]{
                    p.getProductId(),
                    p.getName(),
                    p.getPrice(),
                    p.getStock(),
                    p.getOnSale(),
                    p.getDiscountPrice(),
                    p.getImageUrl(),
                    p.getDescription()
            });
        }
    }

    private static String s(Object o) { return o == null ? "" : String.valueOf(o); }
    private static Integer parseInt(String s) { if (s == null || s.isEmpty()) return null; try { return Integer.parseInt(s); } catch (Exception e) { return null; } }
    private static BigDecimal parseBig(String s) { if (s == null || s.isEmpty()) return null; try { return new BigDecimal(s); } catch (Exception e) { return null; } }
    private static String emptyToNull(String s) { return (s == null || s.trim().isEmpty()) ? null : s.trim(); }

    private static String escapeHtml(String s) {
        if (s == null) return "";
        String r = s;
        r = r.replace("&", "&amp;");
        r = r.replace("<", "&lt;");
        r = r.replace(">", "&gt;");
        r = r.replace("\"", "&quot;");
        r = r.replace("'", "&#39;");
        return r;
    }
}
