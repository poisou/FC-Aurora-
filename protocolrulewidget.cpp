#include "protocolrulewidget.h"

#include "ui_plugin_wid_rulwindow.h"
#include "protocolruleeditdialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTabBar>
#include <QTableWidgetItem>

// ============================================================================
// 构造：加载 .ui，配置表格，隐藏暂不启用的控件，连接"新建规则"按钮
// ============================================================================
// 执行顺序有讲究：
//   先配置表格列与选择行为 → 再隐藏分页/导入导出 → 最后连接信号并首次渲染。
//   若颠倒顺序，refreshTable() 可能按旧列定义写入数据。
// ============================================================================
ProtocolRuleWidget::ProtocolRuleWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::wid_rulwindow)
{
    ui->setupUi(this);

    // ---- 表格基础配置 ----
    // .ui 中虽然已声明三列表头，但仍显式设置一次，
    // 目的是让列定义与代码中的列索引（0=名称、1=协议、2=操作）强绑定，
    // 避免 .ui 被改动后出现"表头与数据错位"。
    auto *table = ui->tabwid_rul;
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({
        QStringLiteral("规则名称"),
        QStringLiteral("协议"),
        QStringLiteral("操作")
    });
    table->horizontalHeader()->setStretchLastSection(false);
    // 列宽策略对齐数据源管理"帧列表"：内容列吃剩余空间，
    // 固定列用 QHeaderView::Fixed + setColumnWidth 显式指定宽度。
    //   - 规则名称 → Stretch（对应帧列表的"值"列）
    //   - 协议     → Fixed 140px
    //   - 操作     → Fixed 180px（与帧列表操作列一致，字体放大后适当加宽）
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table->setColumnWidth(1, 140);
    table->setColumnWidth(2, 180);
    // 禁止直接双击单元格改字：所有编辑必须走对话框，保证字段完整性（名称+协议一起改）。
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 整行选中 + 单选：视觉上更清晰，也避免"选中多个单元格"的歧义。
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    // 隐藏行号列，与数据源管理"帧列表"的样式保持一致。
    table->verticalHeader()->setVisible(false);

    // ---- 隐藏 Tab1 页眉 ----
    // 协议规则只有一个页签，隐藏页签标签栏，让表格直接占据整个区域。
    ui->tabWidget->tabBar()->hide();

    // ---- 放大表格字体与列名 ----
    // 通过覆盖样式表放大表格正文与表头字号，并加高加宽表头，
    // 让列名在放大的字号下完整显示。
    table->setStyleSheet(QStringLiteral(
        "#tabwid_rul {"
        "background-color: #ffffff;"
        "color: #1b2530;"
        "border: 1px solid rgb(70,90,120);"
        "border-radius: 3px;"
        "font: 13pt \"微软雅黑\";"
        "gridline-color: rgb(70,90,120);"
        "}"
        "#tabwid_rul::item {"
        "color: #1b2530;"
        "font-size: 13pt;"
        "}"
        "#tabwid_rul QHeaderView::section {"
        "background-color: #eef2f5;"
        "color: #1b2530;"
        "font-size: 14pt;"
        "font-weight: bold;"
        "padding: 8px 6px;"
        "min-height: 38px;"
        "border: 1px solid #c0ccd4;"
        "}"));

    // ---- 隐藏分页栏 ----
    // 当前规则数量少，一次性全量展示；控件保留在 .ui 中，后续需要分页时取消 hide 即可。
    ui->label_currentpage->hide();   // 当前页码输入框
    ui->label_8->hide();             // 分隔符 "/"
    ui->label_totalpage->hide();     // 总页数
    ui->label_5->hide();             // "页"字
    ui->pbtn_turn->hide();           // 跳转
    ui->pbtn_rulFirst->hide();       // 首页
    ui->pbtn_rulUp->hide();          // 上一页
    ui->pbtn_rulDown->hide();        // 下一页
    ui->pbtn_rulend->hide();         // 尾页

    // ---- 隐藏导入 / 导出按钮 ----
    // 规则目前只存内存，没有序列化格式，导入导出功能留待后续实现。
    ui->pbtn_inport->hide();
    ui->pbtn_export->hide();

    // ---- 唯一需要连接的固定按钮 ----
    // 编辑 / 删除按钮是每行动态生成的，连接在 appendRowButtons() 中完成。
    connect(ui->pbtn_newrul, &QPushButton::clicked,
            this, &ProtocolRuleWidget::onNewRuleClicked);

    // 首次渲染：初始 m_rules 为空，表现为空表 + 空下拉框。
    refreshTable();
}

ProtocolRuleWidget::~ProtocolRuleWidget()
{
    // ui 由 new 创建，需手动 delete；
    // 表格内的 cellWidget（行内按钮）由 QTableWidget 自己销毁，无需在此处理。
    delete ui;
}

// ============================================================================
// refreshTable —— 全量重绘表格与底部规则下拉框
// ============================================================================
// 本页面所有增删改操作后都调用此函数，不做局部更新。
// 理由：规则数量级很小（几十条），全量重绘代码简单且不会出现"数据与界面不同步"的隐患。
//
// 实现要点：
//   1) QSignalBlocker 屏蔽表格信号。setRowCount() 会触发 itemSelectionChanged 等信号，
//      若不屏蔽，重绘过程中可能回调到已失效的行号逻辑；
//   2) 先 setRowCount() 再逐行 setItem()，行数变化时旧 cellWidget 会被自动销毁，
//      因此重复调用不会造成按钮泄漏或信号重复连接；
//   3) 文本统一用 Qt::AlignCenter，与 .ui 中表头样式保持一致。
// ============================================================================
void ProtocolRuleWidget::refreshTable()
{
    auto *table = ui->tabwid_rul;
    // RAII 屏蔽器：构造时 blockSignals(true)，析构时自动恢复原状态，异常安全。
    QSignalBlocker blocker(table);
    table->setRowCount(m_rules.size());

    // ---- 第 0 列（规则名称）与第 1 列（协议）----
    for (int row = 0; row < m_rules.size(); ++row) {
        const auto &entry = m_rules.at(row);

        auto *nameItem = new QTableWidgetItem(entry.name);
        nameItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 0, nameItem);

        // 协议显示文本统一走 ProtocolRuleEditDialog 的静态转换，避免字面量散落各处。
        auto *protoItem = new QTableWidgetItem(
            ProtocolRuleEditDialog::protocolTypeDisplayName(entry.protocolType));
        protoItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 1, protoItem);
    }

    // ---- 第 2 列（操作）：每行嵌入"编辑 + 删除"按钮 ----
    // 单独一个循环，让"写文本"和"装按钮"两件事在代码上分开，便于后续替换成委托。
    for (int row = 0; row < m_rules.size(); ++row) {
        appendRowButtons(row);
    }

    // ---- 底部规则下拉框同步 ----
    // comboBox_rulist 位于"解析测试"区域，用于选择按哪条规则做报文解析。
    // userData 存 ProtocolType 整型值，后续取用时无需再按名称反查。
    ui->comboBox_rulist->clear();
    for (const auto &entry : m_rules) {
        ui->comboBox_rulist->addItem(entry.name,
                                     static_cast<int>(entry.protocolType));
    }
}

// ============================================================================
// appendRowButtons —— 为指定行生成"编辑 + 删除"按钮
// ============================================================================
// 为什么用 setCellWidget 而不是自定义委托（QStyledItemDelegate）：
//   本页面只有两列纯文本，按钮需求简单；嵌入 QWidget 实现成本最低。
//   若将来表格数据量增大（数千行），再改为委托以节省控件开销。
//
// @param row 目标行号。lambda 中按值捕获该行号——注意这带来一个约束：
//            refreshTable() 会整体重建所有行，因此行号不会在按钮存活期间失效。
//            只要坚持"改数据后必调 refreshTable()"，就不会出现按钮点错行的问题。
// ============================================================================
void ProtocolRuleWidget::appendRowButtons(int row)
{
    auto *table = ui->tabwid_rul;

    // 容器控件 + 水平布局，间距与边距对齐数据源管理"帧列表"的操作列样式。
    auto *widget = new QWidget(table);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(5);

    // ---- 编辑按钮 ----
    // 与帧列表的"编辑"按钮一致：54×26 最小尺寸、手型光标。
    // 显式套用白底蓝边样式，避免继承表格 tabwid_rul 的 color: white
    // 导致编辑按钮文字在浅色背景上不可见；圆角 3px 与红色删除按钮保持对称。
    auto *editBtn = new QPushButton(QStringLiteral("编辑"), widget);
    editBtn->setMinimumSize(54, 26);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton { background:#ffffff; color:#36546a;"
            " border:1px solid #a9bfd0; border-radius:3px; }"
            "QPushButton:hover { background:#edf6f8; border-color:#79b7c3; }"));
    // 按值捕获 row：每次 refreshTable 都重建按钮，因此捕获值与当前数据顺序一致。
    connect(editBtn, &QPushButton::clicked,
            this, [this, row]() { onEditRuleClicked(row); });
    layout->addWidget(editBtn);

    // ---- 删除按钮 ----
    // 与帧列表的"删除"按钮一致：红色底、白字、圆角 3px，hover 时加深。
    auto *deleteBtn = new QPushButton(QStringLiteral("删除"), widget);
    deleteBtn->setMinimumSize(54, 26);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton { background:#FF7070; color:white; border:1px solid #E05252;"
            " border-radius:3px; }"
            "QPushButton:hover { background:#FF5C5C; }"));
    connect(deleteBtn, &QPushButton::clicked,
            this, [this, row]() { onDeleteRuleClicked(row); });
    layout->addWidget(deleteBtn);

    // 左对齐按钮组，剩余空间由右侧弹簧吸收（与帧列表一致，而非居中）。
    layout->addStretch();

    // 交给表格托管所有权；下次 setRowCount 变化或再次 setCellWidget 时旧控件自动删除。
    table->setCellWidget(row, 2, widget);
}

// ============================================================================
// onNewRuleClicked —— "新建规则"完整流程
// ============================================================================
// 流程：对话框（新建模式）→ 取消则直接返回（不产生任何副作用）
//       → 全局重名校验 → 追加到 m_rules → 重绘 → 选中并滚动到新行。
//
// 设计要点：
//   - 对话框采用栈对象，exec() 阻塞期间不阻塞事件循环中的其它窗口，
//     返回后无论 Accepted 还是 Rejected 都会自动析构，无内存管理负担；
//   - 重名校验放在"用户已确认"之后，这样用户不必先关掉重名提示再重新填一遍表单
//     （若在对话框内部校验，需要把整个规则集合传进去，耦合更高）。
// ============================================================================
void ProtocolRuleWidget::onNewRuleClicked()
{
    ProtocolRuleEditDialog dlg(this);
    // 空名称 = 新建模式：标题为"新建协议规则"，协议默认 FC。
    dlg.setRule(QString(), ProtocolType::FC);
    if (dlg.exec() != QDialog::Accepted) {
        // 用户取消：不新增、不修改任何数据。
        return;
    }

    // ---- 全局重名校验 ----
    // ruleName() 已做 trim()，因此" 规则A "与"规则A"会被判定为同名。
    const auto &newName = dlg.ruleName();
    for (const auto &entry : m_rules) {
        if (entry.name == newName) {
            QMessageBox::warning(this,
                                 QStringLiteral("新建失败"),
                                 QStringLiteral("已存在同名规则：%1").arg(newName));
            return;
        }
    }

    // ---- 追加记录 ----
    // 显式逐字段赋值而不用聚合初始化，是为了兼容 Qt 5.9 / MinGW 5.3 下
    // QVector<RuleEntry> 对花括号初始化的支持不完整的问题。
    RuleEntry entry;
    entry.name = newName;
    entry.protocolType = dlg.protocolType();
    entry.fields = dlg.fields();   // 收集字段表格内容
    m_rules.append(entry);
    refreshTable();

    // ---- 让用户看到结果 ----
    // 新行位于末尾：选中 + 滚到底部，避免长表格下"点了确定却看不到变化"。
    auto *table = ui->tabwid_rul;
    table->selectRow(table->rowCount() - 1);
    table->scrollToBottom();
}

// ============================================================================
// onEditRuleClicked —— "编辑"流程（由行内按钮触发）
// ============================================================================
// @param row 待编辑行的下标。越界直接返回：
//            正常流程下不会越界（refreshTable 后行号必与 m_rules 对齐），
//            但按钮信号是异步投递的，加一道防护可避免极端时序下的崩溃。
//
// 与新建的唯一差别：重名检查必须跳过自身行，否则"不改名字只改协议"会被误判为重名。
// ============================================================================
void ProtocolRuleWidget::onEditRuleClicked(int row)
{
    if (row < 0 || row >= m_rules.size()) return;

    // 先拷贝原值：对话框阻塞期间 m_rules 不会被改动，这里用拷贝更直观。
    const auto original = m_rules.at(row);
    ProtocolRuleEditDialog dlg(this);
    // 非空名称 = 编辑模式：标题改为"编辑协议规则"，并回填名称、协议与字段列表。
    dlg.setRule(original.name, original.protocolType);
    dlg.setFields(original.fields);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    // ---- 重名校验（跳过自身）----
    const auto &newName = dlg.ruleName();
    for (int i = 0; i < m_rules.size(); ++i) {
        if (i == row) continue;
        if (m_rules.at(i).name == newName) {
            QMessageBox::warning(this,
                                 QStringLiteral("编辑失败"),
                                 QStringLiteral("已存在同名规则：%1").arg(newName));
            return;
        }
    }

    // ---- 原位覆盖 ----
    // 保持行序不变（不做"删除+追加"），用户视线不会跳。
    RuleEntry entry;
    entry.name = newName;
    entry.protocolType = dlg.protocolType();
    entry.fields = dlg.fields();   // 收集字段表格内容
    m_rules[row] = entry;
    refreshTable();

    // 重绘后重新选中原行，便于连续编辑同一条规则。
    ui->tabwid_rul->selectRow(row);
}

// ============================================================================
// onDeleteRuleClicked —— "删除"流程（由行内按钮触发）
// ============================================================================
// @param row 待删除行的下标，越界直接返回。
//
// 删除是不可恢复操作，因此必须二次确认，且默认按钮设为 No：
// 用户误按回车时不会误删。
// ============================================================================
void ProtocolRuleWidget::onDeleteRuleClicked(int row)
{
    if (row < 0 || row >= m_rules.size()) return;

    // 先取名称用于提示文案，比"第 N 行"更能让用户确认删的是哪一条。
    const auto &entry = m_rules.at(row);
    const auto ret = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定删除规则「%1」吗？该操作不可恢复。")
            .arg(entry.name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);   // 默认聚焦 No，防止误删

    if (ret != QMessageBox::Yes) {
        return;
    }

    m_rules.removeAt(row);
    // removeAt 之后所有行号都会前移，必须整体重绘，
    // 否则旧按钮的捕获行号会指向错误数据（这也是强制全量刷新的核心原因）。
    refreshTable();
}
