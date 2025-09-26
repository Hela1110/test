package com.shopping.server.admin;

import com.shopping.server.model.Product;
import com.shopping.server.repository.ProductRepository;

import javax.swing.*;
import javax.swing.table.DefaultTableModel;
import java.awt.*;
import java.math.BigDecimal;
import java.util.List;

/**
 * 简易的服务端管理控制台：首期仅实现“商品管理”页，支持搜索、增改、删除。
 */
public class AdminFrame extends JFrame {
    private final ProductRepository productRepository;

    public AdminFrame(ProductRepository productRepository) {
        super("微商系统 - 服务端管理控制台");
        this.productRepository = productRepository;
        setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        setSize(1000, 700);
        setLocationRelativeTo(null);

        JTabbedPane tabs = new JTabbedPane();
        tabs.addTab("商品管理", buildProductsPanel());
        tabs.addTab("聊天/反馈(预留)", new JLabel("即将上线：与在线用户的会话管理"));
        tabs.addTab("活动商品(预留)", new JLabel("即将上线：设置 onSale/discountPrice"));
        tabs.addTab("数据统计(预留)", new JLabel("即将上线：订单/销量图表"));
        tabs.addTab("系统/用户(预留)", new JLabel("即将上线：服务状态与用户信息"));
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
                    try { p.setProductId(Long.parseLong(idText)); } catch (Exception ignore) {}
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
}
