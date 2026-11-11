#include "protocolruleeditdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// ProtocolRuleEditDialog 的"确定""取消"按钮统一样式。
// 参数与配色与数据源上传弹窗（datasourceuploaddialog.cpp）保持一致，
// 直接设置到按钮本身，避免只设置 QDialogButtonBox 时内部 QPushButton
// 无法正确继承样式。
QString dialogActionButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "min-width: 76px;"
        "min-height: 30px;"
        "background-color: #ffffff;"
        "color: #36546a;"
        "border: 1px solid #a9bfd0;"
        "border-radius: 1px;"
        "padding: 0 12px;"
        "}"
        "QPushButton:hover {"
        "background-color: #edf6f8;"
        "border-color: #79b7c3;"
        "}"
        "QPushButton:pressed {"
        "background-color: #e0eef1;"
        "border-color: #6fb5c0;"
        "}"
        "QPushButton:disabled {"
        "background-color: #f8fafb;"
        "color: #a6b5bf;"
        "border-color: #d6e0e7;"
        "}");
}

// 字段表格"删除"按钮统一样式：红色底、白字、圆角 3px，hover 加深。
QString fieldDeleteButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background:#FF7070; color:white; border:1px solid #E05252;"
        " border-radius:3px; }"
        "QPushButton:hover { background:#FF5C5C; }");
}

// 对 QDialogButtonBox 中的标准按钮设置文案和样式。
// @param buttonBox 包含标准按钮的按钮盒。
// @param standardButton 需要设置的标准按钮类型。
// @param text 按钮显示文字。
void applyDialogActionButtonStyle(QDialogButtonBox *buttonBox,
                                  QDialogButtonBox::StandardButton standardButton,
                                  const QString &text)
{
    if (buttonBox == nullptr) {
        return;
    }

    QPushButton *button = buttonBox->button(standardButton);
    if (button == nullptr) {
        return;
    }

    button->setText(text);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(dialogActionButtonStyle());
}

} // namespace

// ============================================================================
// protocolTypeDisplayName —— 协议类型到显示文本的唯一转换入口
// ============================================================================
// 集中在此处是为了让"表格列文本""下拉框项文本""提示信息"共用同一份字面量，
// 后续若协议名称要改成中文（如"光纤通道"），只需改这一处。
// ============================================================================
QString ProtocolRuleEditDialog::protocolTypeDisplayName(ProtocolType type)
{
    switch (type) {
    case ProtocolType::FC:     return QStringLiteral("FC");
    case ProtocolType::Aurora: return QStringLiteral("Aurora");
    case ProtocolType::TCP:    return QStringLiteral("TCP");
    }
    // 枚举值非法（例如从配置文件读入了越界数字）时返回空串，
    // 调用方表格会显示为空白单元格，而不是崩溃。
    return QString();
}

// ============================================================================
// 构造：设置基础窗口属性后交给 buildUi() 搭界面
// ============================================================================
ProtocolRuleEditDialog::ProtocolRuleEditDialog(QWidget *parent)
    : QDialog(parent)
{
    // 默认按"新建"场景显示；编辑场景由 setRule() 内部改写标题。
    setWindowTitle(QStringLiteral("新建协议规则"));
    // 默认尺寸放大：容纳四列字段表格，同时让内容更易读。
    setMinimumSize(880, 720);
    resize(880, 720);

    // 全局放大字体，输入框 / 下拉框 / 表格 / 标签 / 按钮默认继承该字号。
    QFont baseFont = font();
    baseFont.setPointSize(13);
    setFont(baseFont);

    buildUi();
}

ProtocolRuleEditDialog::~ProtocolRuleEditDialog() = default;

// ============================================================================
// buildUi —— 一次性搭建标题、表单、字段列表与按钮区
// ============================================================================
// 布局结构：
//   QVBoxLayout(this)
//     ├── QLabel 标题（"新建协议规则" / "编辑协议规则"）
//     ├── QFormLayout
//     │     ├── "规则名称：" → m_nameEdit
//     │     └── "协议类型：" → m_protocolCombo
//     ├── QHBoxLayout（"字段列表："标签 + 弹簧 + "新增"按钮）
//     ├── m_fieldTable（字段名 / byte-bit / 长度 / 操作）
//     ├── 垂直弹簧（把按钮区压到底部）
//     └── QDialogButtonBox(Ok | Cancel)
//
// 所有控件以 this 为父对象，随对话框析构自动释放，因此无需手写 delete。
// ============================================================================
void ProtocolRuleEditDialog::buildUi()
{
    // ---- 外层布局 ----
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 16, 18, 14);

    // ---- 内嵌标题（参考上传弹窗的 dialogTitle）----
    m_titleLabel = new QLabel(QStringLiteral("新建协议规则"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    // ---- 表单区 ----
    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    // ---- 规则名称输入框 ----
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("请输入规则名称"));
    m_nameEdit->setMinimumWidth(320);
    // 加高输入框，提升可点击区域与视觉舒适度。
    m_nameEdit->setMinimumHeight(40);
    form->addRow(QStringLiteral("规则名称："), m_nameEdit);

    // ---- 协议类型下拉框 ----
    // 关键点：addItem 的第二个参数（userData）存 ProtocolType 的整型值，
    // protocolType() 通过 currentData().toInt() 反查，二者必须严格对应。
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItem(protocolTypeDisplayName(ProtocolType::FC),
                             static_cast<int>(ProtocolType::FC));
    m_protocolCombo->addItem(protocolTypeDisplayName(ProtocolType::Aurora),
                             static_cast<int>(ProtocolType::Aurora));
    m_protocolCombo->addItem(protocolTypeDisplayName(ProtocolType::TCP),
                             static_cast<int>(ProtocolType::TCP));
    m_protocolCombo->setMinimumWidth(320);
    m_protocolCombo->setMinimumHeight(40);
    form->addRow(QStringLiteral("协议类型："), m_protocolCombo);

    mainLayout->addLayout(form);

    // ---- 字段列表区：标签 + "新增"按钮 ----
    auto *fieldHeaderLayout = new QHBoxLayout();
    fieldHeaderLayout->setSpacing(8);
    auto *fieldListLabel = new QLabel(QStringLiteral("字段列表："), this);
    fieldHeaderLayout->addWidget(fieldListLabel);
    fieldHeaderLayout->addStretch(1);
    m_addFieldButton = new QPushButton(QStringLiteral("+"), this);
    m_addFieldButton->setCursor(Qt::PointingHandCursor);
    // 方形"+"按钮：固定尺寸，带边框，去掉文字改用加号。
    m_addFieldButton->setFixedSize(40, 40);
    m_addFieldButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#36546a;"
        " border:1px solid #a9bfd0; border-radius:3px;"
        " font-size:18pt; font-weight:bold; }"
        "QPushButton:hover { background:#edf6f8; border-color:#79b7c3; }"
        "QPushButton:pressed { background:#e0eef1; }"));
    connect(m_addFieldButton, &QPushButton::clicked,
            this, &ProtocolRuleEditDialog::onAddFieldClicked);
    fieldHeaderLayout->addWidget(m_addFieldButton);
    mainLayout->addLayout(fieldHeaderLayout);

    // ---- 字段表格 ----
    m_fieldTable = new QTableWidget(this);
    m_fieldTable->setColumnCount(4);
    m_fieldTable->setHorizontalHeaderLabels({
        QStringLiteral("字段名"),
        QStringLiteral("byte/bit"),
        QStringLiteral("长度"),
        QStringLiteral("操作")
    });
    m_fieldTable->verticalHeader()->setVisible(false);
    m_fieldTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fieldTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fieldTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_fieldTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_fieldTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_fieldTable->setColumnWidth(1, 120);
    m_fieldTable->setColumnWidth(2, 100);
    m_fieldTable->setColumnWidth(3, 100);
    m_fieldTable->setMinimumHeight(340);

    // 放大字段表格表头字体与高度，让列名更醒目。
    auto *fieldHeader = m_fieldTable->horizontalHeader();
    QFont fieldHeaderFont = fieldHeader->font();
    fieldHeaderFont.setPointSize(13);
    fieldHeaderFont.setBold(true);
    fieldHeader->setFont(fieldHeaderFont);
    fieldHeader->setMinimumHeight(44);

    mainLayout->addWidget(m_fieldTable);

    // ---- 垂直弹簧：把按钮区压到底部，与上传弹窗一致 ----
    mainLayout->addStretch(1);

    // ---- 确定 / 取消按钮 ----
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    applyDialogActionButtonStyle(
        buttonBox,
        QDialogButtonBox::Ok,
        QStringLiteral("确定"));
    applyDialogActionButtonStyle(
        buttonBox,
        QDialogButtonBox::Cancel,
        QStringLiteral("取消"));
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &ProtocolRuleEditDialog::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

// ============================================================================
// setDialogTitle —— 同步窗口标题栏与内嵌标题标签
// ============================================================================
void ProtocolRuleEditDialog::setDialogTitle(const QString &title)
{
    setWindowTitle(title);
    if (m_titleLabel != nullptr) {
        m_titleLabel->setText(title);
    }
}

// ============================================================================
// setRule —— 回填数据并决定"新建 / 编辑"模式
// ============================================================================
void ProtocolRuleEditDialog::setRule(const QString &name, ProtocolType type)
{
    setDialogTitle(name.isEmpty()
                   ? QStringLiteral("新建协议规则")
                   : QStringLiteral("编辑协议规则"));
    m_nameEdit->setText(name);
    // setCurrentIndex 按枚举值定位，依赖 addItem 时的添加顺序与枚举值一致（FC=0/Aurora=1/TCP=2）。
    m_protocolCombo->setCurrentIndex(static_cast<int>(type));
    if (name.isEmpty()) {
        // 新建时把焦点交给输入框，用户打开对话框即可直接打字。
        m_nameEdit->setFocus();
    }
}

// ============================================================================
// setFields —— 回填字段列表（编辑场景）
// ============================================================================
// 清空表格后逐行重建。字段名 / 单位 / 长度通过 cellWidget 回填。
// 注意：必须在 buildUi() 之后调用（m_fieldTable 已创建），
//       通常由调用方在 setRule() 之后、exec() 之前调用。
// ============================================================================
void ProtocolRuleEditDialog::setFields(const QVector<RuleField> &fields)
{
    if (m_fieldTable == nullptr) {
        return;
    }
    m_fieldTable->setRowCount(0);
    for (const RuleField &field : fields) {
        addFieldRow(field);
    }
}

// ============================================================================
// addFieldRow —— 追加一行字段，并按 field 回填
// ============================================================================
// 每一行由四个 cellWidget 组成：
//   列0 字段名   → QLineEdit（无边框，融入表格）
//   列1 byte/bit → QComboBox（byte / bit，userData 存 RuleFieldUnit 整型值）
//   列2 长度     → QLineEdit（无边框，数字文本）
//   列3 操作     → QPushButton（红色删除按钮）
// 删除按钮通过捕获自身指针 + 遍历 cellWidget 定位行号，
// 避免"固定行号"在删除后前移导致的错位。
// ============================================================================
void ProtocolRuleEditDialog::addFieldRow(const RuleField &field)
{
    const int row = m_fieldTable->rowCount();
    m_fieldTable->insertRow(row);

    // ---- 字段名 ----
    auto *nameEdit = new QLineEdit(field.name, m_fieldTable);
    nameEdit->setPlaceholderText(QStringLiteral("字段名"));
    nameEdit->setFrame(false);
    nameEdit->setMinimumHeight(36);
    m_fieldTable->setCellWidget(row, 0, nameEdit);

    // ---- byte/bit ----
    auto *unitCombo = new QComboBox(m_fieldTable);
    unitCombo->addItem(QStringLiteral("byte"), static_cast<int>(RuleFieldUnit::Byte));
    unitCombo->addItem(QStringLiteral("bit"), static_cast<int>(RuleFieldUnit::Bit));
    unitCombo->setCurrentIndex(field.unit == RuleFieldUnit::Bit ? 1 : 0);
    unitCombo->setMinimumHeight(36);
    m_fieldTable->setCellWidget(row, 1, unitCombo);

    // ---- 长度 ----
    auto *lengthEdit = new QLineEdit(QString::number(field.length), m_fieldTable);
    lengthEdit->setPlaceholderText(QStringLiteral("长度"));
    lengthEdit->setFrame(false);
    lengthEdit->setMinimumHeight(36);
    m_fieldTable->setCellWidget(row, 2, lengthEdit);

    // ---- 操作（删除按钮）----
    auto *deleteBtn = new QPushButton(QStringLiteral("删除"), m_fieldTable);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setMinimumSize(64, 34);
    deleteBtn->setStyleSheet(fieldDeleteButtonStyle());
    connect(deleteBtn, &QPushButton::clicked, this, [this, deleteBtn]() {
        // 通过按钮指针定位当前所在行（删除后行号会前移，不能用固定行号）。
        for (int r = 0; r < m_fieldTable->rowCount(); ++r) {
            if (m_fieldTable->cellWidget(r, 3) == deleteBtn) {
                removeFieldRow(r);
                break;
            }
        }
    });
    m_fieldTable->setCellWidget(row, 3, deleteBtn);
}

// ============================================================================
// removeFieldRow —— 删除字段表格指定行
// ============================================================================
// 使用 QTableWidget::removeRow()，行内 cellWidget 会随行一起销毁。
// 其余行的删除按钮通过遍历 cellWidget 定位，因此删除后仍能正确工作。
// ============================================================================
void ProtocolRuleEditDialog::removeFieldRow(int row)
{
    if (row < 0 || row >= m_fieldTable->rowCount()) {
        return;
    }
    m_fieldTable->removeRow(row);
}

// ============================================================================
// onAddFieldClicked —— "新增"按钮槽
// ============================================================================
void ProtocolRuleEditDialog::onAddFieldClicked()
{
    addFieldRow(RuleField());
}

// ============================================================================
// ruleName —— 读取并规范化规则名
// ============================================================================
QString ProtocolRuleEditDialog::ruleName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

// ============================================================================
// protocolType —— 从下拉框反查协议枚举
// ============================================================================
ProtocolType ProtocolRuleEditDialog::protocolType() const
{
    if (!m_protocolCombo) {
        return ProtocolType::FC;
    }
    return static_cast<ProtocolType>(m_protocolCombo->currentData().toInt());
}

// ============================================================================
// fields —— 收集字段表格每一行的数据
// ============================================================================
// 遍历所有行，从 cellWidget 中读取字段名、单位与长度。
// 字段名为空的行仍会保留（调用方自行决定是否过滤），长度解析失败时为 0。
// ============================================================================
QVector<RuleField> ProtocolRuleEditDialog::fields() const
{
    QVector<RuleField> result;
    if (m_fieldTable == nullptr) {
        return result;
    }

    for (int row = 0; row < m_fieldTable->rowCount(); ++row) {
        RuleField field;
        if (auto *nameEdit = qobject_cast<QLineEdit *>(
                m_fieldTable->cellWidget(row, 0))) {
            field.name = nameEdit->text().trimmed();
        }
        if (auto *unitCombo = qobject_cast<QComboBox *>(
                m_fieldTable->cellWidget(row, 1))) {
            field.unit = static_cast<RuleFieldUnit>(unitCombo->currentData().toInt());
        }
        if (auto *lengthEdit = qobject_cast<QLineEdit *>(
                m_fieldTable->cellWidget(row, 2))) {
            field.length = lengthEdit->text().toInt();
        }
        result.append(field);
    }
    return result;
}

// ============================================================================
// onOkClicked —— "确定"前的唯一校验点
// ============================================================================
// 只校验"规则名非空"。重名检查不在这里做，因为本对话框看不到完整规则集合，
// 由调用方 ProtocolRuleWidget 在拿到结果后统一去重（见类注释）。
// 字段列表的合法性（字段名非空、长度合法）由调用方在必要时校验。
// ============================================================================
void ProtocolRuleEditDialog::onOkClicked()
{
    if (ruleName().isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("提示"),
                             QStringLiteral("请输入规则名称。"));
        m_nameEdit->setFocus();
        return;
    }
    accept();
}
