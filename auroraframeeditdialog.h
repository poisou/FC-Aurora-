#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Ui {
class AuroraFrameEditDialog;
}

class QTableWidgetItem;
class QLineEdit;
class QCheckBox;

// Aurora 1024 字节数据的字段范围配置。
//
// 起始索引从 0 开始；索引 0 表示第 1 个字节，索引 1023 表示最后一个字节。
// 多字节数据统一使用大端方式：起始字节为最高有效字节。
// 后期调整 PW/PRI/TOA 在数据中的位置或长度时，只需修改本配置。
namespace AuroraFieldLayout {

constexpr int DataLength = 1024;

constexpr int PwStartIndex = 0;
constexpr int PwByteCount = 1;

constexpr int PriStartIndex = 1;
constexpr int PriByteCount = 1;

constexpr int ToaStartIndex = 2;
constexpr int ToaByteCount = 1;

static_assert(PwStartIndex >= 0 && PwByteCount >= 1 && PwByteCount <= 8
                  && PwStartIndex + PwByteCount <= DataLength,
              "Invalid PW byte range.");
static_assert(PriStartIndex >= 0 && PriByteCount >= 1 && PriByteCount <= 8
                  && PriStartIndex + PriByteCount <= DataLength,
              "Invalid PRI byte range.");
static_assert(ToaStartIndex >= 0 && ToaByteCount >= 1 && ToaByteCount <= 8
                  && ToaStartIndex + ToaByteCount <= DataLength,
              "Invalid TOA byte range.");

} // namespace AuroraFieldLayout

// Aurora 帧编辑对话框使用的数据。
//
// Aurora 参数只保留 PW、PRI、TOA 三项。
// gridValues 按“行优先”保存 16 列数据值表格内容。
//
// 通过 AuroraFrameEditDialog::addAuroraSequenceParameter() 追加的自定义
// 序列字段（初始值/步进）按添加顺序保存在 extraSequences 中。
struct AuroraSequenceExtraValue
{
    bool enabled = false;      // 是否参与序列生成。
    quint64 initialValue = 0;  // 初始值。
    quint64 stepValue = 1;     // 每包步进。
};

struct AuroraFrameEditData
{
    QString pw;
    QString pri;
    QString toa;

    int baseIndex = 0;
    bool randomMode = true;
    int length = 1024;
    int packetCount = 1;

    // PW/PRI/TOA 序列生成参数。
    // 勾选后，生成时使用“初始值 + 当前帧行索引 × 步进”计算字段值。
    bool pwSequenceEnabled = false;
    quint64 pwInitialValue = 0;
    quint64 pwStepValue = 1;

    bool priSequenceEnabled = false;
    quint64 priInitialValue = 0;
    quint64 priStepValue = 1;

    bool toaSequenceEnabled = false;
    quint64 toaInitialValue = 0;
    quint64 toaStepValue = 1;

    // addAuroraSequenceParameter() / addTcpSequenceParameter() 动态追加的
    // 序列字段，顺序与调用顺序一致。
    QVector<AuroraSequenceExtraValue> extraSequences;

    QStringList gridValues;
};

// Aurora 数据帧编辑对话框。
//
// 对应界面文件：auroraframeeditdialog.ui
// 点击自定义数据帧列表中的“编辑”后以模态窗口显示。
// 用户可填写 PW、PRI、TOA、数据长度和 16 列数据值。
class AuroraFrameEditDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造 Aurora 帧编辑对话框。
    // @param parent Qt 父窗口。
    explicit AuroraFrameEditDialog(QWidget *parent = nullptr);

    // 销毁 Designer UI。
    ~AuroraFrameEditDialog() override;

    // 总线模式：决定“数据参数设置”区域显示哪一组参数。
    enum class BusMode {
        Aurora, // 显示 Aurora 参数：PW / PRI / TOA 及序列设置。
        Fc,     // 显示 FC Header set：D_ID、R_CTL ... MSG_L 共 18 项。
        Tcp     // 以太网参数：与 Aurora 布局相同，由 TcpFrameEditDialog 使用。
    };

    // 切换对话框的参数区域。
    // FC 模式显示 headerFrame 并隐藏 Aurora 参数和序列设置；
    // Aurora 模式恢复原有 PW / PRI / TOA 参数区域。
    // @param mode 需要显示的总线参数模式。
    void setBusMode(BusMode mode);

    // 返回当前参数模式。
    BusMode busMode() const;

    // 获取 FC Header set 中 18 个输入框的当前文本。
    // 顺序：D_ID、R_CTL、S_ID、CS_CTL、F_CTL、TYPE、
    //       SEQ_CN、DF_CTL、SEQ_ID、RX_ID、OX_ID、Paramet、
    //       MSG_ID、RESO、RES1、MSG_LE、MSG_PRI、MSG_L。
    QStringList fcHeaderValues() const;

    // 回填 FC Header set 的 18 个输入框。
    // @param values 按 fcHeaderValues() 的顺序给出；不足的部分保持默认值。
    void setFcHeaderValues(const QStringList &values);

    // 数据参数字段的单位：字节或位。
    enum class DataParameterUnit {
        Byte, // 按“字节”读取：起始字节 + 字节长（1~8 字节，大端组合）
        Bit   // 按“位”读取：起始字节的最高位开始，共占“位长”个二进制位
    };

    // ── 各总线参数字段显示接口 ──────────────────────────────────
    // 在“数据参数设置”区域动态增加一个参数字段（标签 + 十进制只读值）。
    // 值会根据下方数据值列表的当前页、按字节/位位置实时计算，始终以 10 进制显示。
    //
    // 调用示例（在 setBusMode()/setFrameData() 之前调用即可，可多次添加）：
    //   AuroraFrameEditDialog dialog(this);
    //   dialog.setBusMode(AuroraFrameEditDialog::BusMode::Aurora);
    //   dialog.addAuroraParameter(QStringLiteral("通道号"),
    //                             AuroraFrameEditDialog::DataParameterUnit::Byte,
    //                             0, 1);
    //   dialog.addAuroraParameter(QStringLiteral("标志位"),
    //                             AuroraFrameEditDialog::DataParameterUnit::Bit,
    //                             1, 3);
    //
    // 三个函数分别对应三条总线，参数完全相同：
    //   name      变量名（界面显示为“变量名:”）
    //   unit      DataParameterUnit::Byte 或 DataParameterUnit::Bit
    //   startByte 起始字节（Bit 模式下表示起始字节，从其最高位开始取位）
    //   length    Byte 模式为字节长（1~8），Bit 模式为位长（1~64）
    // Aurora / 以太网参数显示在 PW/PRI/TOA 同一行；
    // FC 参数显示在 FC Header set 区域下方新增的行中。
    // 注意：请在本总线对应的模式下调用（FC 用 addFcParameter，
    // Aurora 用 addAuroraParameter，以太网用 addTcpParameter）。
    void addAuroraParameter(const QString &name,
                            DataParameterUnit unit,
                            int startByte,
                            int length);
    void addFcParameter(const QString &name,
                        DataParameterUnit unit,
                        int startByte,
                        int length);
    void addTcpParameter(const QString &name,
                         DataParameterUnit unit,
                         int startByte,
                         int length);

    // ── 序列字段（初始值 / 步进）接口 ────────────────────────────
    // 在“初始值/步进”行中 TOA 之后追加一组字段：
    //   [变量名] 复选框 + 初始值 + 步进
    // 勾选后点击“生成”，会把“初始值 + 包索引 × 步进”写入每个 1024 字节包
    // 中该字段对应的字节/位位置；字节序/位序与 BIN_HEADER_BIG_ENDIAN 一致。
    //
    // 调用示例：
    //   dialog.setBusMode(AuroraFrameEditDialog::BusMode::Aurora);
    //   dialog.addAuroraSequenceParameter(QStringLiteral("CH"),
    //                                     AuroraFrameEditDialog::DataParameterUnit::Byte,
    //                                     3, 2);          // 第 3 字节起 2 字节
    //   dialog.addAuroraSequenceParameter(QStringLiteral("FLAG"),
    //                                     AuroraFrameEditDialog::DataParameterUnit::Bit,
    //                                     5, 4);          // 第 5 字节起 4 位
    //
    // 参数含义与 addAuroraParameter() 相同：
    //   name 变量名；unit 字节/位；startByte 起始字节；length 字节长或位长。
    // Aurora 与以太网共用同一行；FC 模式不显示该行，因此不提供 FC 版本。
    void addAuroraSequenceParameter(const QString &name,
                                    DataParameterUnit unit,
                                    int startByte,
                                    int length);
    void addTcpSequenceParameter(const QString &name,
                                 DataParameterUnit unit,
                                 int startByte,
                                 int length);

    // 设置对话框中显示的帧参数和数据值。
    // @param frameInfo 已保存的 Aurora 帧数据。
    void setFrameData(const AuroraFrameEditData &frameInfo);

    // 获取用户当前填写的 Aurora 帧数据。
    // @return 包含 PW、PRI、TOA、数据长度和表格值的数据对象。
    AuroraFrameEditData frameData() const;

    // 获取完整 1024 字节数据的十六进制表示。
    // 无论对话框当前选择 16 进制还是 10 进制，本函数都返回
    // 两位大写 HEX 文本，供外层帧列表“值”列统一显示使用。
    // @return 1024 个 "00"-"FF" 格式的十六进制文本。
    QStringList hexadecimalValues() const;

private slots:
    // 切换数据值表格和 PW/PRI/TOA 的显示进制。
    // @param index 0=16进制，1=10进制。
    void changeDisplayBase(int index);

    // 将当前单元格修改解析为原始字节并保存到内存。
    // @param item 发生修改的数据值单元格。
    void handleValueItemChanged(QTableWidgetItem *item);

    // 清空 16 列数据值表格中的全部内容。
    void clearValues();

    // 根据当前进制生成一页示例数据值。
    void generateValues();

    // 数据值表格切换到上一页。
    void showPreviousValuePage();

    // 数据值表格切换到下一页。
    void showNextValuePage();

    // 跳转到指定数据页。
    void jumpToValuePage();

protected:
    // 参数区域标题文字。
    // 派生类（例如 TcpFrameEditDialog）可重写本函数，
    // 在不改动基类代码的前提下定制“数据参数设置”下方的标题。
    virtual QString parameterSectionTitle() const;

    // 一个动态数据参数字段。
    struct DataParameter {
        QString name;             // 变量名。
        DataParameterUnit unit;   // 字节 / 位。
        int startByte = 0;        // 起始字节。
        int length = 1;           // 字节长或位长。
        QLineEdit *edit = nullptr; // 显示十进制值的只读输入框。
    };

    // 一个动态序列字段（初始值 / 步进）。
    struct SequenceParameter {
        QString name;                 // 变量名（复选框文字）。
        DataParameterUnit unit;       // 字节 / 位。
        int startByte = 0;            // 起始字节。
        int length = 1;               // 字节长或位长。
        QCheckBox *check = nullptr;   // 是否参与序列生成。
        QLineEdit *initialEdit = nullptr; // 初始值。
        QLineEdit *stepEdit = nullptr;    // 步进。
    };

    // 动态参数字段的公共实现（供本类与派生类复用）。
    // @param fcLayout true 表示放入 FC Header set 网格，false 表示放入
    //                 Aurora/以太网的 PW/PRI/TOA 参数行。
    void addDataParameter(const QString &name,
                          DataParameterUnit unit,
                          int startByte,
                          int length,
                          bool fcLayout);

    // 动态序列字段的公共实现（追加到 TOA 之后、弹簧之前）。
    void addSequenceParameter(const QString &name,
                              DataParameterUnit unit,
                              int startByte,
                              int length);

private:
    // 按当前数据页读取一个参数字段的数值（大端/按位组合）。
    quint64 parameterValue(const DataParameter &parameter) const;

    // 把一个数值写入 1024 字节包内的指定字节/位字段。
    // @param packetOffset 该包在 m_dataBytes 中的起始偏移。
    // @param unit 字节或位。
    // @param startByte 起始字节。
    // @param length 字节长或位长。
    // @param value 需要写入的数值。
    void writeParameterValue(int packetOffset,
                             DataParameterUnit unit,
                             int startByte,
                             int length,
                             quint64 value);

    // 刷新所有动态参数字段的十进制显示值。
    // 在翻页、进制切换、单元格编辑、数据回填后自动调用。
    void refreshDataParameters();

    // 初始化数据值表格的列标题、行标题和编辑方式。
    void configureValueTable();

    // 按当前进制刷新完整数据值表格。
    void refreshValueTable();

    // 按总字节数调整数据缓冲区；表格始终固定显示当前 1024 字节页。
    // @param byteCount 包数 × 每包 1024 字节后的总字节数。
    void resizeDataBuffer(int byteCount);

    // 切换数据值表格页码并刷新表格和 PW/PRI/TOA。
    // @param pageIndex 从 0 开始的页索引。
    void changeValuePage(int pageIndex);

    // 根据当前数据总量刷新分页控件状态。
    void updateValuePageControls();

    // 获取当前页在完整数据中的起始字节偏移。
    int currentPageOffset() const;

    // 根据当前 1024 字节数据和字段范围刷新 PW/PRI/TOA 显示。
    void updateHeaderValues();

    // 将 Header 输入框中的数值写回对应的字节范围。
    // @param startIndex 字段在 1024 字节中的起始索引。
    // @param byteCount 字段占用的字节数量。
    // @param edit 触发编辑的 PW/PRI/TOA 输入框。
    void applyHeaderValue(int startIndex, int byteCount, QLineEdit *edit);

    // 将单个字节格式化为当前进制文本。
    // @param value 0-255 的原始字节值。
    QString formatByteValue(int value) const;

    // 将多字节整数格式化为十进制文本。
    // @param value 组合后的无符号整数值。
    QString formatHeaderValue(quint64 value) const;

    Ui::AuroraFrameEditDialog *ui;
    QVector<int> m_dataBytes;
    int m_displayBase = 0;
    int m_currentValuePage = 0;
    BusMode m_busMode = BusMode::Aurora;

    // 通过 addXxxParameter() 动态添加的参数字段。
    QList<DataParameter> m_dataParameters;

    // 通过 addXxxSequenceParameter() 动态添加的序列字段。
    QList<SequenceParameter> m_sequenceParameters;
};
