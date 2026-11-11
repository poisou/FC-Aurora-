#pragma once

#include <QWidget>
#include <QVector>

#include "protocolruleeditdialog.h"

namespace Ui {
class wid_rulwindow;
}

// ============================================================================
// RuleEntry —— 单条协议规则的内存记录
// ============================================================================
// 保存"规则名称 + 协议类型 + 字段列表"，是整个协议规则功能的数据单元。
// 字段列表（fields）由新建规则弹窗中的字段表格维护，
// 每个字段包含字段名、单位（byte/bit）与长度。
// 当前为纯内存存储（应用退出即丢失），若后续需要持久化，
// 可在此结构上追加数据库主键 / 更新时间等字段，并在 ProtocolRuleWidget 中
// 补一套 loadRules() / saveRules() 即可，UI 层代码无需改动。
// ============================================================================
struct RuleEntry {
    QString name;                                   // 规则名称（全局唯一，由页面保证）
    ProtocolType protocolType = ProtocolType::FC;   // 协议类型，默认 FC
    QVector<RuleField> fields;                      // 规则字段列表
};

// ============================================================================
// ProtocolRuleWidget —— 协议规则管理页面
// ============================================================================
// 【职责】
//   承载 PluginUi/plugin_wid_rulwindow.ui 的全部控件，实现规则的
//   "新建 / 编辑 / 删除 / 列表显示"完整闭环，并维护 m_rules 这份内存数据源。
//   页面本身不处理任何收发逻辑，也不做协议解析，只做规则的增删改查。
//
// 【装配位置】
//   PluginWorkspace 构造函数中：
//       m_protocolRuleWidget = new ProtocolRuleWidget(ui->tab_2);
//       ui->glay_rul->addWidget(m_protocolRuleWidget);
//   即主窗口"协议规则"页签（tab_2）的内容。
//
// 【数据流】
//   用户点击"新建规则"
//       → onNewRuleClicked()
//       → ProtocolRuleEditDialog 收集名称/协议
//       → 名称去重校验
//       → m_rules.append()
//       → refreshTable() 重绘表格 + 同步底部 comboBox_rulist
//
//   用户点击某行的"编辑"按钮（按钮由 appendRowButtons() 动态创建并嵌入第 3 列）
//       → onEditRuleClicked(row)
//       → 对话框回填原值 → 去重（排除自身行）→ 覆盖 m_rules[row] → refreshTable()
//
//   用户点击某行的"删除"按钮
//       → onDeleteRuleClicked(row)
//       → QMessageBox 二次确认 → m_rules.removeAt(row) → refreshTable()
//
// 【UI 中隐藏的控件】
//   构造函数里隐藏了分页栏（首页/上一页/下一页/尾页/跳转）与导入导出按钮。
//   这些控件在 plugin_wid_rulwindow.ui 中已存在，保留但暂不启用，
//   后续若规则数量变大需要分页，只需取消隐藏并把 refreshTable() 改为
//   "只渲染当前页区间"即可，无需改动其他代码。
//
// 【扩展提示】
//   - 规则需要持久化到数据库/JSON → 在构造函数末尾加载，在增删改后保存。
//   - 需要规则影响解析行为 → 通过 rules() 取快照，或新增 ruleChanged() 信号。
// ============================================================================
class ProtocolRuleWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造页面并完成初始化。
    // 主要动作：
    //   1) ui->setupUi() 加载 .ui 布局；
    //   2) 配置 tabwid_rul 为三列只读表格（规则名称 / 协议 / 操作），整行单选；
    //   3) 隐藏分页栏与导入导出按钮（见类注释）；
    //   4) 连接"新建规则"按钮 → onNewRuleClicked()；
    //   5) refreshTable() 做首次渲染（初始为空表）。
    // @param parent Qt 父控件，通常是 ui->tab_2。
    explicit ProtocolRuleWidget(QWidget *parent = nullptr);

    // 析构：释放 uic 生成的 Ui 对象。表格行内按钮作为 cellWidget 随表格一并销毁。
    ~ProtocolRuleWidget() override;

    // 取得当前全部规则的只读副本。
    // @return QVector<RuleEntry> 值拷贝（非引用），调用方修改不会影响页面内部状态。
    // 用途：其他页面（如 SceneParallelWidget 的规则下拉框）需要展示规则时，
    //       可持有本页面指针并调用此方法，避免直接访问私有成员。
    // 注意：返回的是快照，规则变更后需重新调用才能拿到最新列表。
    QVector<RuleEntry> rules() const { return m_rules; }

private slots:
    // 【新建】按钮槽。
    // 流程：打开对话框（新建模式）→ 用户确定 → 遍历 m_rules 查重
    //       → 重名则 warning 并中止（不插入）
    //       → 否则 append 到 m_rules → refreshTable() → 选中并滚动到新行。
    // 触发源：ui->pbtn_newrul 的 clicked 信号（在构造函数中连接）。
    void onNewRuleClicked();

    // 【编辑】按钮槽，由每行动态创建的"编辑"按钮通过 lambda 捕获行号后调用。
    // @param row 待编辑行在 m_rules 中的下标（0-based）。
    //            越界时直接 return，防止表格与数据不同步导致崩溃。
    // 流程：对话框回填原值 → 用户确定 → 查重（跳过 row 自身）
    //       → 重名则 warning 并中止 → 否则覆盖 m_rules[row] → refreshTable() → 重新选中该行。
    void onEditRuleClicked(int row);

    // 【删除】按钮槽，由每行动态创建的"删除"按钮通过 lambda 捕获行号后调用。
    // @param row 待删除行在 m_rules 中的下标（0-based），越界时直接 return。
    // 流程：QMessageBox::question 二次确认（默认按钮为 No，防误删）
    //       → 用户选 Yes 才 removeAt(row) → refreshTable()。
    void onDeleteRuleClicked(int row);

private:
    // 全量重绘表格与底部规则下拉框。任何增删改之后都必须调用。
    // 具体动作：
    //   1) QSignalBlocker 屏蔽表格信号，避免 setRowCount/setItem 触发多余的重绘与选中回调；
    //   2) setRowCount(m_rules.size())；
    //   3) 逐行写入第 0 列（规则名称）、第 1 列（协议显示名），文本居中对齐；
    //   4) 逐行调用 appendRowButtons(row) 生成第 2 列的操作按钮；
    //   5) 清空并重填 ui->comboBox_rulist（"解析测试"区域选择规则用），
    //      每项的 userData 存 ProtocolType 整型值。
    // 注意：本函数不做局部刷新，规则数量在千级以内时全量重绘成本可忽略。
    void refreshTable();

    // 为指定行的"操作"列生成"编辑 + 删除"两个按钮。
    // @param row 目标行号，lambda 中按值捕获，后续行号变化不会串行。
    // 实现要点：
    //   - 使用一个承载 QWidget + QHBoxLayout，两侧 addStretch 使按钮组居中；
    //   - 按钮固定尺寸 64×26、手型光标，字体设为 9pt 以适配行高；
    //   - 通过 table->setCellWidget(row, 2, widget) 嵌入，
    //     表格模型只保存 QWidget 指针，不参与排序/复制。
    // 注意：单元格控件由表格接管所有权，refreshTable() 重置行数时会自动销毁旧按钮，
    //       因此无需手动 delete，也不会出现重复连接。
    void appendRowButtons(int row);

    Ui::wid_rulwindow *ui = nullptr;   // uic 生成的界面对象，析构时 delete
    QVector<RuleEntry> m_rules;        // 规则内存数据源（表格显示的唯一依据）
};
