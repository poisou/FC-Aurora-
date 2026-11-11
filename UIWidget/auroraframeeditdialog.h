#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Ui {
class AuroraFrameEditDialog;
}

class QTableWidgetItem;
class QLineEdit;

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

private:
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
};
