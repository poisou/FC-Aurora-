#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

// ============================================================================
// 协议类型枚举
// ============================================================================
// 与协议规则管理页面、场景并行页面的下拉框选项一致。
// 枚举值同时作为 QComboBox::currentData() 的整型值使用（见 ProtocolRuleEditDialog），
// 因此新增协议类型时只需在此追加，并在 protocolTypeDisplayName() 中补一个分支。
// ============================================================================
enum class ProtocolType {
    FC = 0,      // Fibre Channel 光纤通道协议
    Aurora = 1,  // Aurora 高速串行协议
    TCP = 2      // TCP 以太网协议
};

// ============================================================================
// 规则字段单位枚举
// ============================================================================
// 用于协议规则字段定义中"byte/bit"列的二选一取值。
// 枚举值同时作为字段表格中该列 QComboBox 的 userData 使用。
// ============================================================================
enum class RuleFieldUnit {
    Byte = 0,  // 按字节
    Bit = 1    // 按位
};

// ============================================================================
// RuleField —— 单条规则字段（字段名 + 单位 + 长度）
// ============================================================================
// 协议规则的每个字段由三要素构成：
//   - name：字段名
//   - unit：单位（byte 按字节 / bit 按位）
//   - length：长度
// 该结构是新建规则弹窗中字段表格每一行的内存表示。
// ============================================================================
struct RuleField {
    QString name;                              // 字段名
    RuleFieldUnit unit = RuleFieldUnit::Byte;  // 单位（byte/bit）
    int length = 0;                            // 长度
};

// ============================================================================
// ProtocolRuleEditDialog —— 协议规则编辑对话框
// ============================================================================
// 【职责】
//   只负责"收集用户输入"，不持有规则列表、不做增删改，也不访问任何数据层。
//   确认/取消语义完全由 QDialog::accept() / reject() 决定：
//     - accept()：用户点击"确定"且校验通过（规则名非空）
//     - reject()：用户点击"取消"、按 Esc 或关闭窗口
//   调用方必须根据 exec() 的返回值决定是否读取数据，避免"取消也生效"。
//
// 【典型调用】
//   // 新建
//   ProtocolRuleEditDialog dlg(this);
//   dlg.setRule(QString(), ProtocolType::FC);   // 空名称 = 新建模式，标题为"新建协议规则"
//   if (dlg.exec() == QDialog::Accepted) {
//       QString name = dlg.ruleName();
//       ProtocolType type = dlg.protocolType();
//       QVector<RuleField> fields = dlg.fields();
//       // → 此处再做重名校验，然后把结果加入自己的容器
//   }
//
//   // 编辑
//   ProtocolRuleEditDialog dlg(this);
//   dlg.setRule(existing.name, existing.protocolType);  // 非空名称 = 编辑模式
//   dlg.setFields(existing.fields);                      // 回填字段列表
//   if (dlg.exec() == QDialog::Accepted) { /* 覆盖原条目 */ }
//
// 【注意】
//   - 对话框内部不去重。重名校验放在调用方（ProtocolRuleWidget），
//     因为只有调用方知道完整的规则集合。
//   - ruleName() 返回的是 trim() 之后的文本，可直接用于比较与存储。
//   - 字段表格中的"字段名""长度"允许为空或非法，由调用方在必要时自行校验，
//     对话框只保证规则名非空这一基础约束。
// ============================================================================
class ProtocolRuleEditDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造对话框。
    // 内部调用 buildUi() 搭建"规则名称输入框 + 协议类型下拉框 + 字段列表表格 + 确定/取消按钮"。
    // 默认标题为"新建协议规则"，若随后调用 setRule() 传非空名称会自动改为"编辑协议规则"。
    // @param parent 父控件，用于居中与生命周期管理；可为 nullptr。
    explicit ProtocolRuleEditDialog(QWidget *parent = nullptr);
    ~ProtocolRuleEditDialog() override;

    // 回填规则数据，同时决定对话框进入"新建"还是"编辑"模式。
    // @param name 规则名称。
    //             - 空字符串 → 新建模式：标题"新建协议规则"，输入框清空并自动聚焦；
    //             - 非空字符串 → 编辑模式：标题"编辑协议规则"，输入框填入该名称。
    // @param type 协议类型，用于设置协议下拉框当前选中项。
    //             若传入非法值（超出 0~2），下拉框会停在首项 FC。
    // 调用时机：必须在 exec() 之前调用，否则用户看到的是空表单。
    void setRule(const QString &name, ProtocolType type);

    // 回填字段列表（编辑场景）。
    // 清空字段表格后按 fields 逐行重建，字段名、单位、长度一并回填。
    // @param fields 字段列表，可为空（此时表格为空）。
    void setFields(const QVector<RuleField> &fields);

    // 读取用户输入的规则名称。
    // @return 已去除首尾空白的规则名；若控件未初始化则返回空字符串。
    // 调用时机：仅在 exec() 返回 QDialog::Accepted 后调用才有意义，
    //           取消时返回的仍是用户输入内容（可能为空），不要据此写数据。
    QString ruleName() const;

    // 读取用户选择的协议类型。
    // @return 协议类型枚举值；若控件未初始化则返回 ProtocolType::FC。
    // 实现方式：取 QComboBox::currentData().toInt() 强转，
    //           因此下拉框各项的 userData 必须与 ProtocolType 枚举值保持一致。
    // 调用时机：同 ruleName()，仅在 Accepted 分支中使用。
    ProtocolType protocolType() const;

    // 读取用户在字段表格中输入的字段列表。
    // @return 当前表格每一行的 RuleField 快照；空表时返回空列表。
    // 调用时机：仅在 exec() 返回 QDialog::Accepted 后调用。
    QVector<RuleField> fields() const;

    // 协议类型 → 显示文本的静态转换。
    // 表格"协议"列、下拉框选项文本、日志输出都统一走这里，避免出现多个不一致的字面量。
    // @param type 协议类型。
    // @return "FC" / "Aurora" / "TCP"；类型非法时返回空字符串。
    static QString protocolTypeDisplayName(ProtocolType type);

private slots:
    // "确定"按钮的槽。
    // 校验规则名是否为空：
    //   - 为空 → 弹 QMessageBox::warning 并把焦点还给输入框，不关闭对话框；
    //   - 非空 → accept() 关闭对话框，exec() 返回 QDialog::Accepted。
    // 重名校验不在此处做（见类注释）。
    void onOkClicked();

    // "新增"按钮的槽：向字段表格追加一行空字段。
    void onAddFieldClicked();

private:
    // 搭建界面：标题 + QFormLayout 两行（规则名称、协议类型）
    //          + 字段列表区（标签 + 新增按钮 + 表格）+ QDialogButtonBox。
    // 视觉风格参考 datasourceuploaddialog.ui（上传数据源弹窗），
    // 按钮样式与配色由 .cpp 中的 dialogActionButtonStyle() 统一提供。
    // 仅在构造函数中调用一次。所有控件以 this 为父对象，随对话框析构自动释放。
    void buildUi();

    // 更新对话框标题（窗口标题栏 + 内嵌标题 QLabel），
    // 由 setRule() 根据"新建 / 编辑"模式切换文案。
    // @param title 标题文案，例如"新建协议规则"或"编辑协议规则"。
    void setDialogTitle(const QString &title);

    // 向字段表格追加一行，并按 field 回填字段名、单位与长度。
    // @param field 要回填的字段数据；新建空行时传入默认构造的 RuleField()。
    void addFieldRow(const RuleField &field);

    // 删除字段表格中指定行。
    // @param row 待删除行下标（0-based），越界时直接返回。
    void removeFieldRow(int row);

    QLabel *m_titleLabel = nullptr;          // 内嵌标题标签（位于表单上方）
    QLineEdit *m_nameEdit = nullptr;         // 规则名称输入框
    QComboBox *m_protocolCombo = nullptr;    // 协议类型下拉框（userData 存 ProtocolType 整型值）
    QTableWidget *m_fieldTable = nullptr;    // 字段列表表格（字段名 / byte-bit / 长度 / 操作）
    QPushButton *m_addFieldButton = nullptr; // "新增"按钮
};
