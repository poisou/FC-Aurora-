// 对应界面：auroraframeeditdialog.ui。
// 本文件负责 Aurora 数据帧参数、1024 字节分页及序列值生成。
#include "auroraframeeditdialog.h"

// 引入 BIN_HEADER_BIG_ENDIAN：参数字段的字节序/位序与本项目其它
// 解析逻辑共用同一个宏，避免出现两套大小端约定。
#include "binformat.h"
#include "ui_auroraframeeditdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVector>
#include <QtGlobal>

namespace {

constexpr int ValueColumnCount = 16;
constexpr int ValueRowsPerPage =
    AuroraFieldLayout::DataLength / ValueColumnCount;

// AuroraFrameEditDialog 的“保存”“取消”按钮统一样式。
// 与 DataSourceUploadDialog 的“完成”“取消”按钮保持一致。
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

// 总字节数 = 包数 × 每包 1024 字节。
int totalByteCountForPackets(int packetCount)
{
    return qMax(1, packetCount) * AuroraFieldLayout::DataLength;
}

// 从 1024 字节数组中读取指定范围，组合为无符号整数。
// 字节序由 binformat.h 的 BIN_HEADER_BIG_ENDIAN 宏统一决定，
// 与包头解析、参数字段取值保持同一套约定。
// @param values 完整的 1024 字节数据。
// @param startIndex 起始字节索引，从 0 开始。
// @param byteCount 参与组合的字节数量，范围为 1-8。
// @return 组合后的十进制值。
quint64 decodeDecimalValue(const QVector<int> &values,
                           int startIndex,
                           int byteCount)
{
    quint64 result = 0;
    for (int offset = 0; offset < byteCount; ++offset) {
        const quint64 byteValue =
            static_cast<quint64>(
                static_cast<quint8>(values.at(startIndex + offset)));
#if BIN_HEADER_BIG_ENDIAN
        // 大端：第一个字节是最高有效字节，依次左移拼接。
        result = (result << 8) | byteValue;
#else
        // 小端：第一个字节是最低有效字节，按位置左移后累加。
        result |= byteValue << (8 * offset);
#endif
    }
    return result;
}

// 解析序列初始值和步进。支持十进制及 0x 前缀十六进制。
quint64 parseUnsignedValue(const QString &text)
{
    QString valueText = text.trimmed();
    int base = 10;
    if (valueText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        valueText.remove(0, 2);
        base = 16;
    }

    bool ok = false;
    const quint64 value = valueText.toULongLong(&ok, base);
    return ok ? value : 0;
}

// 计算指定字节数量可表示的最大无符号值。
quint64 maximumValueForBytes(int byteCount)
{
    if (byteCount >= 8) {
        return ~static_cast<quint64>(0);
    }
    return (static_cast<quint64>(1) << (byteCount * 8)) - 1;
}

// 将数值写入 1024 字节数组中的指定范围。
// 写入字节序与 decodeDecimalValue() 一致，由 BIN_HEADER_BIG_ENDIAN 决定。
void writeBigEndianValue(QVector<int> &values,
                         int startIndex,
                         int byteCount,
                         quint64 value)
{
#if BIN_HEADER_BIG_ENDIAN
    // 大端：从范围末尾向前填低字节，起始字节为最高有效字节。
    quint64 remaining = value;
    for (int offset = byteCount - 1; offset >= 0; --offset) {
        values[startIndex + offset] =
            static_cast<int>(remaining & 0xFF);
        remaining >>= 8;
    }
#else
    // 小端：从起始字节向后填低字节，起始字节为最低有效字节。
    quint64 remaining = value;
    for (int offset = 0; offset < byteCount; ++offset) {
        values[startIndex + offset] =
            static_cast<int>(remaining & 0xFF);
        remaining >>= 8;
    }
#endif
}

} // namespace

AuroraFrameEditDialog::AuroraFrameEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AuroraFrameEditDialog)
{
    m_dataBytes.fill(0, AuroraFieldLayout::DataLength);
    ui->setupUi(this);
    configureValueTable();

    applyDialogActionButtonStyle(
        ui->buttonBox,
        QDialogButtonBox::Ok,
        QStringLiteral("保存"));
    applyDialogActionButtonStyle(
        ui->buttonBox,
        QDialogButtonBox::Cancel,
        QStringLiteral("取消"));

    connect(ui->clearButton,
            &QPushButton::clicked,
            this,
            &AuroraFrameEditDialog::clearValues);
    connect(ui->generateButton,
            &QPushButton::clicked,
            this,
            &AuroraFrameEditDialog::generateValues);
    connect(ui->baseCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &AuroraFrameEditDialog::changeDisplayBase);
    connect(ui->valueTable,
            &QTableWidget::itemChanged,
            this,
            &AuroraFrameEditDialog::handleValueItemChanged);
    connect(ui->valuePrevButton,
            &QPushButton::clicked,
            this,
            &AuroraFrameEditDialog::showPreviousValuePage);
    connect(ui->valueNextButton,
            &QPushButton::clicked,
            this,
            &AuroraFrameEditDialog::showNextValuePage);
    connect(ui->valueJumpButton,
            &QPushButton::clicked,
            this,
            &AuroraFrameEditDialog::jumpToValuePage);
    connect(ui->pwEdit,
            &QLineEdit::editingFinished,
            this,
            [this]() {
                applyHeaderValue(
                    AuroraFieldLayout::PwStartIndex,
                    AuroraFieldLayout::PwByteCount,
                    ui->pwEdit);
            });
    connect(ui->priEdit,
            &QLineEdit::editingFinished,
            this,
            [this]() {
                applyHeaderValue(
                    AuroraFieldLayout::PriStartIndex,
                    AuroraFieldLayout::PriByteCount,
                    ui->priEdit);
            });
    connect(ui->toaEdit,
            &QLineEdit::editingFinished,
            this,
            [this]() {
                applyHeaderValue(
                    AuroraFieldLayout::ToaStartIndex,
                    AuroraFieldLayout::ToaByteCount,
                    ui->toaEdit);
            });
    connect(ui->buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &QDialog::accept);
    connect(ui->buttonBox,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject);

    refreshValueTable();
    updateHeaderValues();
}

AuroraFrameEditDialog::~AuroraFrameEditDialog()
{
    delete ui;
}

void AuroraFrameEditDialog::setBusMode(BusMode mode)
{
    m_busMode = mode;
    const bool fcMode = (mode == BusMode::Fc);

    // FC 模式：显示 FC Header set，隐藏 Aurora 的 PW/PRI/TOA
    // 参数区和序列设置区；
    // Aurora / 以太网模式：显示 PW/PRI/TOA 参数行与序列设置行。
    ui->headerSetTitle->setText(parameterSectionTitle());
    ui->headerFrame->setVisible(fcMode);
    ui->auroraParamSimpleFrame->setVisible(!fcMode);
    ui->sequenceParamFrame->setVisible(!fcMode);
}

QString AuroraFrameEditDialog::parameterSectionTitle() const
{
    // 基类同时服务于 FC 与 Aurora：
    //   Fc    → “FC Header set”
    //   其它  → “Aurora参数设置”
    // 派生类（TcpFrameEditDialog）可重写本函数换成自己的标题。
    return m_busMode == BusMode::Fc ? QStringLiteral("FC Header set")
                                    : QStringLiteral("Aurora参数设置");
}

AuroraFrameEditDialog::BusMode AuroraFrameEditDialog::busMode() const
{
    return m_busMode;
}

QStringList AuroraFrameEditDialog::fcHeaderValues() const
{
    // 按界面从左到右、从上到下的顺序收集 18 个 FC Header 字段。
    return {
        ui->dIdEdit->text().trimmed(),
        ui->rCtlEdit->text().trimmed(),
        ui->sIdEdit->text().trimmed(),
        ui->csCtlEdit->text().trimmed(),
        ui->fCtlEdit->text().trimmed(),
        ui->typeEdit->text().trimmed(),
        ui->seqCnEdit->text().trimmed(),
        ui->dfCtlEdit->text().trimmed(),
        ui->seqIdEdit->text().trimmed(),
        ui->rxIdEdit->text().trimmed(),
        ui->oxIdEdit->text().trimmed(),
        ui->parameterEdit->text().trimmed(),
        ui->msgIdEdit->text().trimmed(),
        ui->res0Edit->text().trimmed(),
        ui->res1Edit->text().trimmed(),
        ui->msgLeEdit->text().trimmed(),
        ui->msgPrEdit->text().trimmed(),
        ui->msgLEdit->text().trimmed()
    };
}

void AuroraFrameEditDialog::setFcHeaderValues(const QStringList &values)
{
    const QList<QLineEdit *> edits = {
        ui->dIdEdit,
        ui->rCtlEdit,
        ui->sIdEdit,
        ui->csCtlEdit,
        ui->fCtlEdit,
        ui->typeEdit,
        ui->seqCnEdit,
        ui->dfCtlEdit,
        ui->seqIdEdit,
        ui->rxIdEdit,
        ui->oxIdEdit,
        ui->parameterEdit,
        ui->msgIdEdit,
        ui->res0Edit,
        ui->res1Edit,
        ui->msgLeEdit,
        ui->msgPrEdit,
        ui->msgLEdit
    };

    for (int index = 0; index < edits.size() && index < values.size(); ++index) {
        if (!values.at(index).isEmpty()) {
            edits.at(index)->setText(values.at(index));
        }
    }
}

void AuroraFrameEditDialog::addAuroraParameter(const QString &name,
                                               DataParameterUnit unit,
                                               int startByte,
                                               int length)
{
    addDataParameter(name, unit, startByte, length, false);
}

void AuroraFrameEditDialog::addFcParameter(const QString &name,
                                           DataParameterUnit unit,
                                           int startByte,
                                           int length)
{
    addDataParameter(name, unit, startByte, length, true);
}

void AuroraFrameEditDialog::addTcpParameter(const QString &name,
                                            DataParameterUnit unit,
                                            int startByte,
                                            int length)
{
    addDataParameter(name, unit, startByte, length, false);
}

void AuroraFrameEditDialog::addDataParameter(const QString &name,
                                             DataParameterUnit unit,
                                             int startByte,
                                             int length,
                                             bool fcLayout)
{
    if (name.isEmpty()) {
        return;
    }

    DataParameter parameter;
    parameter.name = name;
    parameter.unit = unit;
    parameter.startByte = qMax(0, startByte);
    parameter.length = qMax(1, length);
    // 字节模式最多 8 字节、位模式最多 64 位（quint64 可表示的范围）。
    parameter.length = qBound(
        1,
        parameter.length,
        unit == DataParameterUnit::Byte ? 8 : 64);

    auto *label = new QLabel(name + QStringLiteral(":"), this);
    auto *edit = new QLineEdit(this);
    edit->setReadOnly(true);
    edit->setAlignment(Qt::AlignCenter);
    edit->setMinimumSize(90, 30);
    edit->setMaximumWidth(130);
    edit->setText(QStringLiteral("0"));
    edit->setToolTip(
        QStringLiteral("按数据值列表当前页自动计算，始终以 10 进制显示。"));
    parameter.edit = edit;

    if (fcLayout) {
        // FC：在 FC Header set 网格下方追加一行（标签 + 值）。
        QGridLayout *grid = ui->headerGridLayout;
        const int row = grid->rowCount();
        grid->addWidget(label, row, 0);
        grid->addWidget(edit, row, 1);
    } else {
        // Aurora / 以太网：插入到 PW/PRI/TOA 所在参数行的末尾。
        // 该行最后一项是弹簧（spacer），插到它前面即可保持左对齐。
        QBoxLayout *layout = ui->auroraParamSimpleLayout;
        const int insertIndex = qMax(0, layout->count() - 1);
        layout->insertWidget(insertIndex, label);
        layout->insertWidget(insertIndex + 1, edit);
    }

    m_dataParameters.append(parameter);
    refreshDataParameters();
}

quint64 AuroraFrameEditDialog::parameterValue(
    const DataParameter &parameter) const
{
    // 数据来源：下方数据值列表当前页对应的 1024 字节。
    const int pageOffset = currentPageOffset();

    if (parameter.unit == DataParameterUnit::Byte) {
        // 字节模式：多字节数值的字节序与项目其它解析逻辑保持一致，
        // 统一由 binformat.h 的 BIN_HEADER_BIG_ENDIAN 宏控制。
        quint64 value = 0;
        for (int index = 0; index < parameter.length; ++index) {
            const int dataIndex = pageOffset + parameter.startByte + index;
            if (dataIndex < 0 || dataIndex >= m_dataBytes.size()) {
                return value;
            }
            const quint64 byteValue =
                static_cast<quint8>(m_dataBytes.at(dataIndex));
#if BIN_HEADER_BIG_ENDIAN
            // 大端：起始字节是最高有效字节，依次左移拼接。
            value = (value << 8) | byteValue;
#else
            // 小端：起始字节是最低有效字节，按所在位置左移后累加。
            value |= byteValue << (8 * index);
#endif
        }
        return value;
    }

    // 位模式：位的排列顺序同样由 BIN_HEADER_BIG_ENDIAN 决定。
    // 大端（默认）：从起始字节的最高位 bit7 开始，依次向低位、后续字节取位；
    // 小端：从起始字节的最低位 bit0 开始，依次向高位、后续字节取位。
    quint64 value = 0;
    for (int index = 0; index < parameter.length; ++index) {
        const int absoluteBit = parameter.startByte * 8 + index;
        const int dataIndex = pageOffset + absoluteBit / 8;
        if (dataIndex < 0 || dataIndex >= m_dataBytes.size()) {
            break;
        }
        const int bitInByte = absoluteBit % 8;
#if BIN_HEADER_BIG_ENDIAN
        const int shift = 7 - bitInByte;
#else
        const int shift = bitInByte;
#endif
        const quint8 bit =
            (static_cast<quint8>(m_dataBytes.at(dataIndex)) >> shift) & 0x01;
        value = (value << 1) | bit;
    }
    return value;
}

void AuroraFrameEditDialog::refreshDataParameters()
{
    for (const DataParameter &parameter : m_dataParameters) {
        if (parameter.edit == nullptr) {
            continue;
        }
        // 始终以 10 进制显示。
        parameter.edit->setText(
            QString::number(parameterValue(parameter)));
    }
}

void AuroraFrameEditDialog::addAuroraSequenceParameter(const QString &name,
                                                       DataParameterUnit unit,
                                                       int startByte,
                                                       int length)
{
    addSequenceParameter(name, unit, startByte, length);
}

void AuroraFrameEditDialog::addTcpSequenceParameter(const QString &name,
                                                    DataParameterUnit unit,
                                                    int startByte,
                                                    int length)
{
    addSequenceParameter(name, unit, startByte, length);
}

void AuroraFrameEditDialog::addSequenceParameter(const QString &name,
                                                 DataParameterUnit unit,
                                                 int startByte,
                                                 int length)
{
    if (name.isEmpty()) {
        return;
    }

    SequenceParameter parameter;
    parameter.name = name;
    parameter.unit = unit;
    parameter.startByte = qMax(0, startByte);
    parameter.length = qBound(1,
                              qMax(1, length),
                              unit == DataParameterUnit::Byte ? 8 : 64);

    // 与 PW/PRI/TOA 完全一致的控件组合：
    // [变量名] 复选框 | 初始值: 输入框 | 步进: 输入框
    auto *check = new QCheckBox(name, this);
    auto *initialLabel = new QLabel(QStringLiteral("初始值:"), this);
    auto *initialEdit = new QLineEdit(QStringLiteral("0"), this);
    initialEdit->setMinimumSize(82, 30);
    auto *stepLabel = new QLabel(QStringLiteral("步进:"), this);
    auto *stepEdit = new QLineEdit(QStringLiteral("1"), this);
    stepEdit->setMinimumSize(82, 30);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);

    parameter.check = check;
    parameter.initialEdit = initialEdit;
    parameter.stepEdit = stepEdit;

    // 追加到该行末尾（最后一项是弹簧，插到它前面即可保持左对齐），
    // 因此新字段自然显示在 TOA 之后。
    QBoxLayout *layout = ui->sequenceParamLayout;
    int insertIndex = qMax(0, layout->count() - 1);
    layout->insertWidget(insertIndex++, separator);
    layout->insertWidget(insertIndex++, check);
    layout->insertWidget(insertIndex++, initialLabel);
    layout->insertWidget(insertIndex++, initialEdit);
    layout->insertWidget(insertIndex++, stepLabel);
    layout->insertWidget(insertIndex++, stepEdit);

    m_sequenceParameters.append(parameter);
}

void AuroraFrameEditDialog::writeParameterValue(int packetOffset,
                                                DataParameterUnit unit,
                                                int startByte,
                                                int length,
                                                quint64 value)
{
    if (unit == DataParameterUnit::Byte) {
        // 字节模式：直接复用统一的字节序写入逻辑。
        writeBigEndianValue(m_dataBytes,
                            packetOffset + startByte,
                            length,
                            value);
        return;
    }

    // 位模式：逐位写入，保留同一字节中其它位的原值。
    for (int index = 0; index < length; ++index) {
        const int absoluteBit = startByte * 8 + index;
        const int dataIndex = packetOffset + absoluteBit / 8;
        if (dataIndex < 0 || dataIndex >= m_dataBytes.size()) {
            break;
        }
        const int bitInByte = absoluteBit % 8;
#if BIN_HEADER_BIG_ENDIAN
        const int shift = 7 - bitInByte;
#else
        const int shift = bitInByte;
#endif
        // 数值按“高位先写”的顺序取位，与 parameterValue() 的读取顺序一致。
        const int bit =
            static_cast<int>((value >> (length - 1 - index)) & 0x01);
        int byteValue = static_cast<quint8>(m_dataBytes.at(dataIndex));
        byteValue &= ~(1 << shift);
        byteValue |= (bit << shift);
        m_dataBytes[dataIndex] = byteValue;
    }
}

void AuroraFrameEditDialog::setFrameData(const AuroraFrameEditData &frameInfo)
{
    // ---------------------------------------------------------------------
    // 编辑窗口回填赋值区
    // 数据来源均为 frameInfo（外层帧记录保存的 AuroraFrameEditData）。
    // 进制、包数、序列开关/初始值/步进在此恢复；如新增界面字段，
    // 应同步在 frameData() 中读取，否则保存时会丢失。
    // ---------------------------------------------------------------------
    m_displayBase = qBound(0, frameInfo.baseIndex, 1);
    {
        QSignalBlocker blocker(ui->baseCombo);
        ui->baseCombo->setCurrentIndex(m_displayBase);
    }

    ui->randomRadioButton->setChecked(frameInfo.randomMode);
    ui->lengthSpinBox->setValue(AuroraFieldLayout::DataLength);
    ui->packetCountSpinBox->setValue(frameInfo.packetCount);

    ui->pwSequenceCheckBox->setChecked(frameInfo.pwSequenceEnabled);
    ui->pwInitialValueEdit->setText(
        QString::number(frameInfo.pwInitialValue));
    ui->pwStepValueEdit->setText(
        QString::number(frameInfo.pwStepValue));

    ui->priSequenceCheckBox->setChecked(frameInfo.priSequenceEnabled);
    ui->priInitialValueEdit->setText(
        QString::number(frameInfo.priInitialValue));
    ui->priStepValueEdit->setText(
        QString::number(frameInfo.priStepValue));

    ui->toaSequenceCheckBox->setChecked(frameInfo.toaSequenceEnabled);
    ui->toaInitialValueEdit->setText(
        QString::number(frameInfo.toaInitialValue));
    ui->toaStepValueEdit->setText(
        QString::number(frameInfo.toaStepValue));

    // 回填动态追加的序列字段（按添加顺序与 extraSequences 一一对应）。
    for (int index = 0;
         index < m_sequenceParameters.size()
         && index < frameInfo.extraSequences.size();
         ++index) {
        const SequenceParameter &parameter = m_sequenceParameters.at(index);
        const AuroraSequenceExtraValue &value =
            frameInfo.extraSequences.at(index);
        if (parameter.check != nullptr) {
            parameter.check->setChecked(value.enabled);
        }
        if (parameter.initialEdit != nullptr) {
            parameter.initialEdit->setText(
                QString::number(value.initialValue));
        }
        if (parameter.stepEdit != nullptr) {
            parameter.stepEdit->setText(
                QString::number(value.stepValue));
        }
    }

    // 数据总长度 = 包数 × 每包 1024 字节。
    const int totalBytes = totalByteCountForPackets(frameInfo.packetCount);
    m_currentValuePage = 0;
    resizeDataBuffer(totalBytes);

    // 数据库中保存的是当前进制下的文本。读取时先按保存时的进制解析，
    // 再统一转换成 0-255 的原始字节，后续切换进制不会改变数据本身。
    for (int index = 0;
         index < frameInfo.gridValues.size() && index < totalBytes;
         ++index) {
        bool valueOk = false;
        const QString text = frameInfo.gridValues.at(index).trimmed();
        const int value =
            text.toInt(&valueOk, m_displayBase == 0 ? 16 : 10);
        if (valueOk && value >= 0 && value <= 0xFF) {
            m_dataBytes[index] = value;
        }
    }

    refreshValueTable();
    updateHeaderValues();
}

AuroraFrameEditData AuroraFrameEditDialog::frameData() const
{
    // ---------------------------------------------------------------------
    // 编辑窗口保存赋值区
    // 这里把控件当前值写回 AuroraFrameEditData。若要新增可保存参数，
    // 需要同时增加结构体字段、setFrameData() 回填和这里的数据采集。
    // ---------------------------------------------------------------------
    AuroraFrameEditData frameInfo;
    frameInfo.pw = ui->pwEdit->text();
    frameInfo.pri = ui->priEdit->text();
    frameInfo.toa = ui->toaEdit->text();

    frameInfo.baseIndex = ui->baseCombo->currentIndex();
    frameInfo.randomMode = ui->randomRadioButton->isChecked();
    frameInfo.length = AuroraFieldLayout::DataLength;
    frameInfo.packetCount = ui->packetCountSpinBox->value();

    frameInfo.pwSequenceEnabled =
        ui->pwSequenceCheckBox->isChecked();
    frameInfo.pwInitialValue =
        parseUnsignedValue(ui->pwInitialValueEdit->text());
    frameInfo.pwStepValue =
        parseUnsignedValue(ui->pwStepValueEdit->text());

    frameInfo.priSequenceEnabled =
        ui->priSequenceCheckBox->isChecked();
    frameInfo.priInitialValue =
        parseUnsignedValue(ui->priInitialValueEdit->text());
    frameInfo.priStepValue =
        parseUnsignedValue(ui->priStepValueEdit->text());

    frameInfo.toaSequenceEnabled =
        ui->toaSequenceCheckBox->isChecked();
    frameInfo.toaInitialValue =
        parseUnsignedValue(ui->toaInitialValueEdit->text());
    frameInfo.toaStepValue =
        parseUnsignedValue(ui->toaStepValueEdit->text());

    // 采集动态追加的序列字段（顺序与 addXxxSequenceParameter() 调用顺序一致）。
    frameInfo.extraSequences.clear();
    frameInfo.extraSequences.reserve(m_sequenceParameters.size());
    for (const SequenceParameter &parameter : m_sequenceParameters) {
        AuroraSequenceExtraValue value;
        value.enabled = parameter.check != nullptr
            && parameter.check->isChecked();
        value.initialValue = parameter.initialEdit != nullptr
            ? parseUnsignedValue(parameter.initialEdit->text())
            : 0;
        value.stepValue = parameter.stepEdit != nullptr
            ? parseUnsignedValue(parameter.stepEdit->text())
            : 1;
        frameInfo.extraSequences.append(value);
    }

    frameInfo.gridValues.clear();
    frameInfo.gridValues.reserve(m_dataBytes.size());
    for (int value : m_dataBytes) {
        frameInfo.gridValues.append(formatByteValue(value));
    }

    return frameInfo;
}

QStringList AuroraFrameEditDialog::hexadecimalValues() const
{
    QStringList hexValues;
    hexValues.reserve(m_dataBytes.size());
    for (int value : m_dataBytes) {
        hexValues.append(
            QStringLiteral("%1")
                .arg(value, 2, 16, QLatin1Char('0'))
                .toUpper());
    }
    return hexValues;
}

void AuroraFrameEditDialog::clearValues()
{
    m_currentValuePage = 0;
    resizeDataBuffer(totalByteCountForPackets(
        ui->packetCountSpinBox->value()));
    refreshValueTable();
    updateHeaderValues();
}

void AuroraFrameEditDialog::generateValues()
{
    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
    const bool randomMode = ui->randomRadioButton->isChecked();
    const int packetCount = qMax(1, ui->packetCountSpinBox->value());
    const int totalBytes = totalByteCountForPackets(packetCount);
    m_currentValuePage = 0;

    // 先生成“包数 × 1024”个完整数据字节。勾选“随机数”时全部随机，
    // 未勾选时全部为 0；随后在每个 1024 字节包的对应位置写入序列值。
    resizeDataBuffer(totalBytes);
    for (int index = 0; index < m_dataBytes.size(); ++index) {
        m_dataBytes[index] = randomMode ? (qrand() % 256) : 0;
    }

    // 勾选序列参数的字段会在每个 1024 字节包的对应位置写入：
    // 当前数值 = 初始值 + 包索引 × 步进。
    // 包索引从 0 开始，因此第 1 个包直接使用初始值。
    const auto applySequence =
        [this](bool enabled,
               const QString &initialText,
               const QString &stepText,
               int startIndex,
               int byteCount,
               int packetIndex) {
            if (!enabled) {
                return;
            }

            const quint64 initialValue = parseUnsignedValue(initialText);
            const quint64 stepValue = parseUnsignedValue(stepText);
            const quint64 maximumValue =
                maximumValueForBytes(byteCount);
            const quint64 currentValue =
                (initialValue
                 + stepValue * static_cast<quint64>(packetIndex))
                & maximumValue;
            const int packetOffset =
                packetIndex * AuroraFieldLayout::DataLength;
            writeBigEndianValue(
                m_dataBytes,
                packetOffset + startIndex,
                byteCount,
                currentValue);
        };

    for (int packetIndex = 0; packetIndex < packetCount; ++packetIndex) {
        applySequence(ui->pwSequenceCheckBox->isChecked(),
                      ui->pwInitialValueEdit->text(),
                      ui->pwStepValueEdit->text(),
                      AuroraFieldLayout::PwStartIndex,
                      AuroraFieldLayout::PwByteCount,
                      packetIndex);
        applySequence(ui->priSequenceCheckBox->isChecked(),
                      ui->priInitialValueEdit->text(),
                      ui->priStepValueEdit->text(),
                      AuroraFieldLayout::PriStartIndex,
                      AuroraFieldLayout::PriByteCount,
                      packetIndex);
        applySequence(ui->toaSequenceCheckBox->isChecked(),
                      ui->toaInitialValueEdit->text(),
                      ui->toaStepValueEdit->text(),
                      AuroraFieldLayout::ToaStartIndex,
                      AuroraFieldLayout::ToaByteCount,
                      packetIndex);

        // 动态追加的序列字段（TOA 之后的自定义字段）：
        // 使用各自的字节/位位置与位长写入，规则同样是
        // 当前数值 = 初始值 + 包索引 × 步进。
        for (const SequenceParameter &parameter : m_sequenceParameters) {
            if (parameter.check == nullptr
                || !parameter.check->isChecked()) {
                continue;
            }

            const quint64 initialValue = parameter.initialEdit != nullptr
                ? parseUnsignedValue(parameter.initialEdit->text())
                : 0;
            const quint64 stepValue = parameter.stepEdit != nullptr
                ? parseUnsignedValue(parameter.stepEdit->text())
                : 0;

            // 掩码：字节模式按字节数、位模式按位数计算最大可表示值。
            const int bitCount = parameter.unit == DataParameterUnit::Byte
                ? parameter.length * 8
                : parameter.length;
            const quint64 maximumValue = bitCount >= 64
                ? ~static_cast<quint64>(0)
                : ((static_cast<quint64>(1) << bitCount) - 1);
            const quint64 currentValue =
                (initialValue
                 + stepValue * static_cast<quint64>(packetIndex))
                & maximumValue;

            writeParameterValue(packetIndex * AuroraFieldLayout::DataLength,
                                parameter.unit,
                                parameter.startByte,
                                parameter.length,
                                currentValue);
        }
    }

    // 表格和 Header 都按当前进制显示，但底层始终保存原始字节值。
    refreshValueTable();
    updateHeaderValues();
}

int AuroraFrameEditDialog::currentPageOffset() const
{
    return m_currentValuePage * AuroraFieldLayout::DataLength;
}

void AuroraFrameEditDialog::changeValuePage(int pageIndex)
{
    const int pageCount =
        qMax(1, (m_dataBytes.size() + AuroraFieldLayout::DataLength - 1)
                   / AuroraFieldLayout::DataLength);
    m_currentValuePage = qBound(0, pageIndex, pageCount - 1);
    refreshValueTable();
    updateHeaderValues();
    updateValuePageControls();
}

void AuroraFrameEditDialog::showPreviousValuePage()
{
    if (m_currentValuePage <= 0) {
        return;
    }
    changeValuePage(m_currentValuePage - 1);
}

void AuroraFrameEditDialog::showNextValuePage()
{
    const int pageCount =
        qMax(1, (m_dataBytes.size() + AuroraFieldLayout::DataLength - 1)
                   / AuroraFieldLayout::DataLength);
    if (m_currentValuePage + 1 >= pageCount) {
        return;
    }
    changeValuePage(m_currentValuePage + 1);
}

void AuroraFrameEditDialog::jumpToValuePage()
{
    changeValuePage(ui->valuePageSpinBox->value() - 1);
}

void AuroraFrameEditDialog::updateValuePageControls()
{
    const int pageCount =
        qMax(1, (m_dataBytes.size() + AuroraFieldLayout::DataLength - 1)
                   / AuroraFieldLayout::DataLength);
    QSignalBlocker blocker(ui->valuePageSpinBox);
    ui->valuePageSpinBox->setRange(1, pageCount);
    ui->valuePageSpinBox->setValue(m_currentValuePage + 1);
    ui->valuePageSpinBox->setEnabled(pageCount > 1);
    ui->valuePageCountLabel->setText(
        QStringLiteral("/ %1 页").arg(pageCount));
    ui->valuePrevButton->setEnabled(m_currentValuePage > 0);
    ui->valueNextButton->setEnabled(m_currentValuePage + 1 < pageCount);
    ui->valueJumpButton->setEnabled(pageCount > 1);
}

void AuroraFrameEditDialog::changeDisplayBase(int index)
{
    m_displayBase = qBound(0, index, 1);
    refreshValueTable();
    updateHeaderValues();
}

void AuroraFrameEditDialog::handleValueItemChanged(QTableWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    // 表格是分页视图，row/column 只是当前页内坐标；
    // 必须加上 currentPageOffset() 才是 m_dataBytes 的全局索引。
    const int index = currentPageOffset()
                      + item->row() * ValueColumnCount
                      + item->column();
    if (index < 0 || index >= m_dataBytes.size()) {
        return;
    }

    QString text = item->text().trimmed();
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text.remove(0, 2);
    }

    bool valueOk = false;
    const int value =
        text.toInt(&valueOk, m_displayBase == 0 ? 16 : 10);
    if (!valueOk || value < 0 || value > 0xFF) {
        // 输入非法时恢复当前原始字节对应的进制文本。
        QSignalBlocker blocker(ui->valueTable);
        item->setText(formatByteValue(m_dataBytes.at(index)));
        return;
    }

    m_dataBytes[index] = value;
    {
        // 输入合法后统一显示为大写 HEX 或普通十进制文本。
        QSignalBlocker blocker(ui->valueTable);
        item->setText(formatByteValue(value));
    }

    // 数据值变化后，PW/PRI/TOA 的取值可能随之变化，需要同步刷新。
    updateHeaderValues();
    // 动态参数字段同样依赖数据值，一并刷新（始终 10 进制）。
    refreshDataParameters();
}

void AuroraFrameEditDialog::refreshValueTable()
{
    const int pageOffset = currentPageOffset();
    QSignalBlocker blocker(ui->valueTable);
    for (int row = 0; row < ValueRowsPerPage; ++row) {
        for (int column = 0; column < ValueColumnCount; ++column) {
            const int index =
                pageOffset + row * ValueColumnCount + column;
            if (index >= m_dataBytes.size()) {
                continue;
            }
            QTableWidgetItem *item = ui->valueTable->item(row, column);
            if (item) {
                item->setText(formatByteValue(m_dataBytes.at(index)));
            }
        }
    }

    // 翻页、切换进制、清空/生成数据后，动态参数字段都要按当前页重新计算。
    refreshDataParameters();
}

void AuroraFrameEditDialog::resizeDataBuffer(int byteCount)
{
    const int safeByteCount =
        qMax(AuroraFieldLayout::DataLength, byteCount);
    m_dataBytes.fill(0, safeByteCount);

    const int pageCount =
        qMax(1, (safeByteCount + AuroraFieldLayout::DataLength - 1)
                   / AuroraFieldLayout::DataLength);
    m_currentValuePage = qBound(0, m_currentValuePage, pageCount - 1);

    QSignalBlocker blocker(ui->valueTable);
    ui->valueTable->clearContents();
    ui->valueTable->setRowCount(ValueRowsPerPage);
    ui->valueTable->setColumnCount(ValueColumnCount);

    QStringList rowHeaders;
    rowHeaders.reserve(ValueRowsPerPage);
    for (int row = 1; row <= ValueRowsPerPage; ++row) {
        rowHeaders.append(QString::number(row));
    }
    ui->valueTable->setVerticalHeaderLabels(rowHeaders);

    for (int row = 0; row < ValueRowsPerPage; ++row) {
        for (int column = 0; column < ValueColumnCount; ++column) {
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            ui->valueTable->setItem(row, column, item);
        }
    }

    updateValuePageControls();
}

void AuroraFrameEditDialog::updateHeaderValues()
{
    // PW/PRI/TOA 始终读取“当前页”对应的 1024 字节块。
    // 当前页偏移 = 页索引 × 1024。
    const int pageOffset = currentPageOffset();
    const quint64 pwValue =
        decodeDecimalValue(m_dataBytes,
                           pageOffset + AuroraFieldLayout::PwStartIndex,
                           AuroraFieldLayout::PwByteCount);
    const quint64 priValue =
        decodeDecimalValue(m_dataBytes,
                           pageOffset + AuroraFieldLayout::PriStartIndex,
                           AuroraFieldLayout::PriByteCount);
    const quint64 toaValue =
        decodeDecimalValue(m_dataBytes,
                           pageOffset + AuroraFieldLayout::ToaStartIndex,
                           AuroraFieldLayout::ToaByteCount);

    // PW/PRI/TOA 始终显示十进制，不随数据值表格的
    // “16进制/10进制”下拉框变化。
    // PW/PRI/TOA 显示值均来自当前页 1024 字节中的字段范围。
    // 字段位置和长度在 auroraframeeditdialog.h 的 AuroraFieldLayout 中修改。
    ui->pwEdit->setText(formatHeaderValue(pwValue));
    ui->priEdit->setText(formatHeaderValue(priValue));
    ui->toaEdit->setText(formatHeaderValue(toaValue));
}

void AuroraFrameEditDialog::applyHeaderValue(int startIndex,
                                              int byteCount,
                                              QLineEdit *edit)
{
    if (edit == nullptr) {
        return;
    }

    QString text = edit->text().trimmed();

    // 参数设置中的三个字段始终按十进制解析。
    bool valueOk = false;
    const quint64 enteredValue = text.toULongLong(&valueOk, 10);

    // 每个字节提供 8 位，计算该字段允许的最大值。
    // byteCount 已由编译期 static_assert 限制在 1-8。
    const quint64 maximumValue =
        byteCount >= 8
            ? ~static_cast<quint64>(0)
            : ((static_cast<quint64>(1) << (byteCount * 8)) - 1);
    if (!valueOk || enteredValue > maximumValue) {
        // 输入非法或超出字段范围时，恢复为由当前字节数据计算出的值。
        const int pageOffset = currentPageOffset();
        const quint64 currentValue =
            decodeDecimalValue(
                m_dataBytes, pageOffset + startIndex, byteCount);
        edit->setText(formatHeaderValue(currentValue));
        return;
    }

    // 与读取方式对应写回：字节序由 BIN_HEADER_BIG_ENDIAN 决定，
    // 例如大端两字节字段值 0x0102 会写为 [起始] = 01、[起始+1] = 02；
    // 小端则写为 [起始] = 02、[起始+1] = 01。
    // 写回目标 = 当前页起始偏移 + 字段起始索引。
    const int pageOffset = currentPageOffset();
#if BIN_HEADER_BIG_ENDIAN
    quint64 remainingValue = enteredValue;
    for (int offset = byteCount - 1; offset >= 0; --offset) {
        m_dataBytes[pageOffset + startIndex + offset] =
            static_cast<int>(remainingValue & 0xFF);
        remainingValue >>= 8;
    }
#else
    quint64 remainingValue = enteredValue;
    for (int offset = 0; offset < byteCount; ++offset) {
        m_dataBytes[pageOffset + startIndex + offset] =
            static_cast<int>(remainingValue & 0xFF);
        remainingValue >>= 8;
    }
#endif

    // Header 参数修改后同步刷新数据值表格，使两者始终一致。
    refreshValueTable();
    updateHeaderValues();
}

QString AuroraFrameEditDialog::formatByteValue(int value) const
{
    if (m_displayBase == 1) {
        return QString::number(value);
    }
    return QStringLiteral("%1")
        .arg(value, 2, 16, QLatin1Char('0'))
        .toUpper();
}

QString AuroraFrameEditDialog::formatHeaderValue(quint64 value) const
{
    // 参数设置中的 PW/PRI/TOA 始终输出十进制文本。
    return QString::number(value);
}

void AuroraFrameEditDialog::configureValueTable()
{
    ui->valueTable->setColumnCount(ValueColumnCount);

    QStringList columnHeaders;
    for (int column = 1; column <= ValueColumnCount; ++column) {
        columnHeaders.append(QString::number(column));
    }
    ui->valueTable->setHorizontalHeaderLabels(columnHeaders);

    ui->valueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->valueTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->valueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    resizeDataBuffer(AuroraFieldLayout::DataLength);
}
