#pragma once

// =============================================================================
// 数据回看页面
//
// 对应界面文件：wid_review.ui
// 负责协调以下模块：
//   1. 文件选择、拖放和命令行导入；
//   2. 后台线程流式扫描 BIN 包头；
//   3. 数据包分页、页码跳转和数据摘要；
//   4. 按需加载当前包数据段；
//   5. 16/10 进制编辑以及修改原位写回 BIN 文件。
// =============================================================================

#include "binformat.h"

#include <QList>
#include <QWidget>
#include <QString>
#include <QVector>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QLabel;
class QProgressBar;
class QResizeEvent;
class QTableWidgetItem;
class QThread;
class QTimer;

class BinFileImporterBase;
class ChunkedTableFiller;

namespace Ui {
class WidReview;
}

// 数据浏览表格某一列显示的数据来源。
// 修改列名/列数量时，同时指定每列取哪个字段，填充逻辑无需改动。
enum class ReviewColumnField {
    Index,          // 包序号（显示时 +1）
    Offset,         // 包在文件中的起始偏移
    Header,         // 6 字节包头原始内容（HEX）
    DataCount,      // 包头解析出的 n
    PayloadLength,  // 数据段实际字节数
    PayloadPreview, // 数据段前 12 字节预览
    CustomData      // 从数据段按“字节/位 + 起始位置 + 长度”取值
};

// 自定义数据列的取值单位。
enum class ReviewDataUnit {
    Byte, // 按字节读取：起始字节 + 字节长（1~8 字节组合成一个整数）
    Bit   // 按位读取：起始字节内的起始位 + 位长（1~64 位组合成一个整数）
};

// 数据浏览表格的一列定义。
//
// 两种用法：
//   1) 内置列：只给列名和数据来源，例如
//        {QStringLiteral("包序号"), ReviewColumnField::Index}
//   2) 自定义数据列：从该行数据的数据段中按位置取值，例如
//        {QStringLiteral("通道号"), ReviewDataUnit::Bit, 0, 0, 0, 4}
//      参数含义见第二个构造函数的注释。
struct ReviewColumn
{
    QString title;          // 列名（可自由修改）。
    ReviewColumnField field;// 该列显示的数据来源。

    // 以下 5 项只在 field == ReviewColumnField::CustomData 时生效。
    ReviewDataUnit unit;    // 取值单位：字节或位。
    int startByte = 0;      // 数据段内的起始字节序号（从 0 开始）。
    int startBit = 0;       // 起始字节内的起始位（0~7，0 表示最高位）。
    int byteLength = 0;     // 字节长（1~8），Byte 模式使用。
    int bitLength = 0;      // 位长（1~64），Bit 模式使用。

    // 内置列构造：只指定列名和数据来源字段。
    // @param columnTitle 列名。
    // @param columnField 内置数据来源。
    ReviewColumn(const QString &columnTitle = QString(),
                 ReviewColumnField columnField = ReviewColumnField::Index)
        : title(columnTitle)
        , field(columnField)
        , unit(ReviewDataUnit::Byte)
        , startByte(0)
        , startBit(0)
        , byteLength(0)
        , bitLength(0)
    {
    }

    // 自定义数据列构造：按“单位 + 起始字节 + 起始位 + 字节长/位长”取值。
    //
    // 位置说明：
    //   起始字节是“该行数据段内的第几个字节”，从 0 开始，
    //   0 表示该包数据段的第 1 个字节；
    //   对应文件绝对偏移 = 包 offset + 6 字节包头 + 起始字节。
    //
    // @param columnTitle 列名。
    // @param dataUnit 取值单位：Byte=按字节，Bit=按位。
    // @param dataStartByte 数据段内的起始字节序号（从 0 开始）。
    // @param dataStartBit 起始字节内的起始位（0~7，0 表示最高位 bit7）；
    //        仅 Bit 模式生效，Byte 模式忽略。
    // @param dataByteLength 字节长（1~8）；仅 Byte 模式生效，Bit 模式忽略。
    // @param dataBitLength 位长（1~64）；仅 Bit 模式生效，Byte 模式忽略。
    ReviewColumn(const QString &columnTitle,
                 ReviewDataUnit dataUnit,
                 int dataStartByte,
                 int dataStartBit,
                 int dataByteLength,
                 int dataBitLength)
        : title(columnTitle)
        , field(ReviewColumnField::CustomData)
        , unit(dataUnit)
        , startByte(dataStartByte)
        , startBit(dataStartBit)
        , byteLength(dataByteLength)
        , bitLength(dataBitLength)
    {
    }
};

// 数据回看页面类。
//
// 主线程只处理 UI 和轻量元数据；Aurora 与 TCP 扫描分别由独立导入器在后台线程完成。
// 当前数据段最多加载 64 KB，避免单包过大时阻塞界面或占用过多内存。
class WidReview : public QWidget
{
    Q_OBJECT

public:
    // 构造数据回看页面。
    // @param parent Qt 父控件；作为 wid_stack 子页面时由调用方传入。
    explicit WidReview(QWidget *parent = nullptr);

    // 销毁页面；若后台导入仍在运行，会先请求取消并等待线程结束。
    ~WidReview() override;

    // 启动异步文件导入。
    //
    // 可被文件选择框、拖放和命令行启动参数复用。
    // 该方法只做快速路径校验并创建工作线程，真正的包头扫描在后台完成。
    //
    // @param filePath 用户选择的 BIN 文件路径。
    // @return 成功启动导入返回 true；正在导入或路径非法返回 false。
    bool loadFile(const QString &filePath);

    // 对数据浏览表中指定的当前页行列赋值。
    // @param value 需要写入单元格的文本。
    // @param row 当前页表格行索引，从 0 开始。
    // @param column 数据浏览表列索引。
    // @return 行列有效且赋值成功时返回 true。
    bool setDataValue(const QString &value, int row, int column);

    // ── 数据浏览表列定义接口（三条总线各自独立，供二次开发修改）──────
    // 每条总线一个接口，传入列表即该总线数据浏览表的完整列定义：
    // 列名的数量就是列数量，顺序就是表格列顺序。
    //
    // 列有两种写法：
    //   1) 内置列：只给列名和数据来源（序号/偏移/包头/数据块 n/长度/预览）；
    //   2) 自定义数据列：给列名 + 字节/位 + 起始字节 + 起始位 + 字节长/位长。
    //
    // 调用示例（FC 表：两列内置 + 两列从数据段取值）：
    //   review->setFcPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       // 数据段第 0 字节的第 4 位开始，取 4 位
    //       {QStringLiteral("低 4 位"), ReviewDataUnit::Bit, 0, 4, 0, 4},
    //       // 数据段第 0 字节开始，取 2 个字节
    //       {QStringLiteral("前 2 字节"), ReviewDataUnit::Byte, 0, 0, 2, 0}
    //   });
    //
    // 自定义数据列的取值规则：
    //   - 起始字节是“该行数据段内的第几个字节”，从 0 开始；
    //     对应文件绝对偏移 = 包 offset + 6 字节包头 + 起始字节；
    //   - Byte 模式读“字节长”个字节，按 BIN_HEADER_BIG_ENDIAN 组合成整数；
    //   - Bit 模式从起始字节的起始位开始连续读“位长”个二进制位，
    //     起始位 0 表示该字节最高位（bit7），组合后统一按 10 进制显示；
    //   - 起始字节超出该包数据段长度时，该单元格显示空字符串。
    //
    // 修改后若当前正在显示该总线，表头和当前页数据会立即刷新；
    // 传入空列表表示该总线不显示任何列。

    // 设置 FC 数据浏览表的列定义（总线索引 0）。
    //
    // 只影响“总线类型选择 FC”时显示的数据浏览表，Aurora / 以太网不受影响。
    // 传入列表即完整列定义：列表长度 = 列数，顺序 = 表格从左到右的列顺序，
    // 每列的写法见上方 ReviewColumn 的两种构造方式。
    //
    // 调用示例（保留序号/偏移，再追加两列自定义数据）：
    //   review->setFcPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       {QStringLiteral("帧头标志"), ReviewDataUnit::Byte, 0, 0, 1, 0},
    //       {QStringLiteral("通道号"),   ReviewDataUnit::Bit,  1, 0, 0, 4}
    //   });
    //
    // 执行流程（与另外两个接口完全一致，见 applyPacketTableColumns()）：
    //   1. 保存该总线的列定义到 m_packetColumns[0]；
    //   2. 若当前正在显示 FC → 立即重建表头（列名与列数）；
    //   3. 已有数据时重新填充当前页并选中第一行，新列马上可见。
    //
    // @param columns 列定义列表；传空列表表示 FC 表不显示任何列。
    void setFcPacketTableColumns(const QList<ReviewColumn> &columns);

    // 设置 Aurora 数据浏览表的列定义（总线索引 1）。
    //
    // 只影响“总线类型选择 Aurora”时显示的数据浏览表。
    // Aurora 包的数据段长度 = n × 1024，自定义列的起始字节同样从 0 开始，
    // 同样可用 Byte / Bit 两种方式取值。
    //
    // 调用示例（在内置列基础上追加一列“第 0 字节的低 4 位”）：
    //   review->setAuroraPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       {QStringLiteral("6 字节包头"), ReviewColumnField::Header},
    //       {QStringLiteral("数据块 n"), ReviewColumnField::DataCount},
    //       {QStringLiteral("低 4 位"), ReviewDataUnit::Bit, 0, 4, 0, 4}
    //   });
    //
    // 执行流程同上：保存定义 → 正在显示 Aurora 时重建表头 → 重填当前页。
    // @param columns 列定义列表；传空列表表示 Aurora 表不显示任何列。
    void setAuroraPacketTableColumns(const QList<ReviewColumn> &columns);

    // 设置以太网数据浏览表的列定义（总线索引 2）。
    //
    // 只影响“总线类型选择以太网”时显示的数据浏览表。
    // 以太网包的数据段长度 = n（n ≤ 1024），自定义列的位置同样以数据段为基准。
    //
    // 调用示例（把内置“数据预览”替换成按字节取值的列）：
    //   review->setTcpPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       {QStringLiteral("数据长度"), ReviewColumnField::PayloadLength},
    //       {QStringLiteral("前 4 字节"), ReviewDataUnit::Byte, 0, 0, 4, 0}
    //   });
    //
    // 执行流程同上：保存定义 → 正在显示以太网时重建表头 → 重填当前页。
    // @param columns 列定义列表；传空列表表示以太网表不显示任何列。
    void setTcpPacketTableColumns(const QList<ReviewColumn> &columns);

    // 读取指定总线当前的列定义，返回顺序即表格列顺序。
    // 常用场景：在 setXxxPacketTableColumns() 之前先取出默认定义，
    // 追加几列后再整体传回去，避免重复书写内置列。
    //   QList<ReviewColumn> columns = review->packetTableColumns(0);
    //   columns.append({QStringLiteral("通道号"), ReviewDataUnit::Bit,
    //                   0, 0, 0, 4});
    //   review->setFcPacketTableColumns(columns);
    // @param busIndex 总线索引：0=FC，1=Aurora，2=以太网。
    // @return 该总线的列定义；索引非法时返回空列表。
    QList<ReviewColumn> packetTableColumns(int busIndex) const;

    // 计算某总线数据浏览表“第 column 列、第 row 行”需要显示的文本。
    // 所有“列 → 数据”的取值映射都集中在本函数中：
    // 修改列含义、增加新字段或调整显示格式时，只需改这一处，
    // populateCurrentPage() 和各总线的列定义都不需要跟着改。
    // @param busIndex 总线索引：0=FC，1=Aurora，2=以太网。
    // @param column 列索引，按该总线当前的列定义顺序，从 0 开始。
    // @param row 行号（当前页内的行索引，从 0 开始）。
    // @param value 该行对应的数据（解包得到的 BinPacket）。
    // @return 该单元格应显示的文本；参数非法时返回空字符串。
    QString packetCellText(int busIndex,
                           int column,
                           int row,
                           const BinPacket &value) const;

protected:
    // 处理拖动进入事件。
    // @param event Qt 拖动事件对象，用于判断 MIME 数据和接受/拒绝拖动。
    void dragEnterEvent(QDragEnterEvent *event) override;

    // 处理拖动离开事件。
    // @param event Qt 拖动离开事件对象，用于结束拖入高亮状态。
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    // 处理文件放下事件。
    // @param event Qt 放下事件对象，用于读取拖入的本地文件 URL。
    void dropEvent(QDropEvent *event) override;

    // 处理页面尺寸变化。
    // @param event Qt 尺寸变化事件对象，用于触发字节列等宽重算。
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 打开文件选择对话框；无参数。
    void chooseFile();

    // 清空当前文件；导入期间执行取消操作；无参数。
    void clearFile();

    // 切换到上一页；无参数。
    void showPreviousPage();

    // 切换到下一页；无参数。
    void showNextPage();

    // 根据页码输入框切换到指定页；无参数。
    void jumpToPage();

    // 响应总线类型下拉框变化。
    // @param index 当前下拉项索引：0=未选择，1=FC，2=Aurora，3=以太网。
    void handleBusTypeChanged(int index);

    // 根据数据包列表当前选中行加载数据段详情；无参数。
    void updatePacketDetails();

    // 响应可编辑字节单元格内容变化。
    // @param item 内容发生变化的表格项。
    void handleHexItemChanged(QTableWidgetItem *item);

    // 切换数据段显示和编辑进制。
    // @param index 下拉项索引：0=16进制，1=10进制。
    void changeNumberBase(int index);

    // 将当前数据段修改原位写回 BIN 文件；无参数。
    void saveCurrentPacket();

    // 更新后台导入进度。
    // @param processedBytes 已扫描字节数。
    // @param totalBytes 文件总字节数。
    // @param packetCount 已识别数据包数量。
    void handleImportProgress(qint64 processedBytes, qint64 totalBytes, int packetCount);

    // 接收后台导入成功结果。
    // @param packets 数据包元数据列表。
    // @param filePath 导入文件绝对路径。
    // @param fileSize 导入文件字节大小。
    void handleImportFinished(const QVector<BinPacket> &packets,
                              const QString &filePath,
                              qint64 fileSize);

    // 处理后台导入失败。
    // @param message 详细错误信息。
    void handleImportFailed(const QString &message);

    // 处理后台导入取消；无参数。
    void handleImportCancelled();

private:
    // 数据段显示和编辑使用的数字进制。
    enum class NumberBase {
        Hexadecimal,
        Decimal
    };

    // 当前文件所属的总线协议。
    enum class BusType {
        None,
        FC,
        Aurora,
        Ethernet
    };

    // 初始化数据包元数据列表；无参数。
    void configureTable();

    // 数据浏览表使用的列结构。
    enum class PacketTableSchema {
        Empty,
        Aurora,
        FC,
        Ethernet
    };

    // 根据总线类型切换数据浏览表列名和隐藏列。
    // @param schema Empty 清空列名，Aurora 显示 6 列，FC/Ethernet 显示 4 列。
    void setPacketTableSchema(PacketTableSchema schema);

    // 初始化 FC / Aurora / 以太网三套默认列定义。
    // 默认值同样通过三条总线的公开接口
    // setFcPacketTableColumns() / setAuroraPacketTableColumns() /
    // setTcpPacketTableColumns() 设置，改默认列名只改本函数即可。
    void initializePacketTableColumns();

    // 三条总线的列定义设置的统一实现。
    // 三个公开接口都调用本函数，保证行为一致。
    // @param busIndex 总线索引：0=FC，1=Aurora，2=以太网。
    // @param columns 新的列定义；空列表表示该总线不显示列。
    void applyPacketTableColumns(int busIndex,
                                 const QList<ReviewColumn> &columns);

    // 计算自定义数据列（ReviewColumnField::CustomData）的单元格文本。
    // 按列定义中的“字节/位 + 起始字节 + 起始位 + 字节长/位长”
    // 从该行数据的数据段中取值，统一返回 10 进制文本。
    // @param column 列定义。
    // @param value 该行对应的数据包元数据。
    // @return 取值结果；位置越界或读取失败时返回空字符串。
    QString customColumnText(const ReviewColumn &column,
                             const BinPacket &value) const;

    // 初始化可编辑字节表格；无参数。
    void configureHexTable();

    // 根据表格实际可视宽度重新分配 16 个字节列，保证每列等宽；无参数。
    void updateHexColumnWidths();

    // 将已选择的文件信息显示到界面。
    // @param filePath 已选择文件的路径。
    void applySelectedFileInfo(const QString &filePath);

    // 按当前进制格式化一个字节。
    // @param value 0-255 范围内的单字节数值。
    // @return 16进制返回两位大写 HEX，10进制返回十进制文本。
    QString formatDisplayByte(quint8 value) const;

    // 仅刷新单元格显示文本，不改变底层字节内容；无参数。
    void refreshHexTableValues();

    // 从 BIN 文件按需读取指定数据包的数据段。
    // @param packetIndex 数据包在 m_packets 中的全局索引；-1 表示清空。
    void loadHexPayload(int packetIndex);

    // 保存成功后同步刷新数据包列表中的前 12 字节预览；无参数。
    void updatePacketPreviewRow();

    // 创建并启动后台导入线程。
    // @param filePath 已确认、准备扫描的 BIN 文件路径。
    // @param protocol 数据段解析协议；FC 使用独立类但当前规则与 TCP 相同。
    // @param busType 用户选择的总线类型，用于界面列结构和总线状态。
    void beginImport(const QString &filePath,
                     BinFormat::Protocol protocol,
                     BusType busType);

    // 统一切换导入期间控件状态。
    // @param importing true 表示进入导入状态，false 表示恢复交互。
    void setImportingUi(bool importing);

    // 将界面和数据状态恢复到未载入文件状态；无参数。
    void resetView();

    // 只渲染当前页，避免一次性创建全部数据包行；无参数。
    void populateCurrentPage();

    // 选中当前页第一包并只加载一次详情。
    // setCurrentCell + selectRow 会触发多次 selectionChanged，
    // 这里用信号屏蔽器合并为一次，减少翻页时重复构建十六进制表格。
    void selectFirstPacketOnPage();

    // 同步页码、范围、跳转输入框和翻页按钮状态；无参数。
    void updatePagination();

    // 设置拖入区域高亮。
    // @param highlighted true 表示显示拖入高亮，false 表示恢复普通边框。
    void setDropHighlighted(bool highlighted);

    // 设置顶部状态标签。
    // @param text 需要显示的状态文本。
    // @param state QSS 状态名，通常为 idle、success 或 error。
    void setStatus(const QString &text, const QString &state);

    // 设置文件状态提示。
    // @param text 提示文本。
    // @param state QSS 状态名，通常为 idle、success 或 error。
    void setFileState(const QString &text, const QString &state);

    // 显示导入错误并将界面恢复到安全状态。
    // @param message 面向用户的详细错误信息。
    void showLoadError(const QString &message);

    // 更新数据回看页面底部状态栏文本。
    // @param text 需要显示在底部状态栏的文本。
    void setReviewStatus(const QString &text);

    // Qt Designer 生成的界面对象。
    Ui::WidReview *ui;

    // 当前文件的全部数据包元数据，不包含完整数据段。
    QVector<BinPacket> m_packets;

    // 三条总线的数据浏览表列定义：
    // [0]=FC，[1]=Aurora，[2]=以太网。
    // 通过 setFcPacketTableColumns() / setAuroraPacketTableColumns() /
    // setTcpPacketTableColumns() 修改后，填充和显示都会按新定义执行。
    QList<ReviewColumn> m_packetColumns[3];

    // 当前文件绝对路径，用于按需读取和原位保存。
    QString m_currentFilePath;

    // 下次打开文件对话框时使用的目录。
    QString m_lastDirectory;

    // 用户已选择、等待总线类型或已解析的文件。
    QString m_selectedFilePath;
    qint64 m_selectedFileSize = 0;
    BusType m_busType = BusType::None;

    // 从 0 开始的当前分页索引，界面显示页码时加 1。
    int m_currentPage = 0;

    // 后台导入线程及其工作对象。
    QThread *m_importThread = nullptr;
    BinFileImporterBase *m_importer = nullptr;

    // 状态栏中的导入进度条。
    QProgressBar *m_importProgress = nullptr;

    // 页面内部底部状态栏中的状态文本。
    QLabel *m_reviewStatusLabel = nullptr;

    // 是否正在执行后台导入。
    bool m_isImporting = false;

    // 导入代次，用于忽略已经过期的线程信号。
    quint64 m_importGeneration = 0;

    // 当前数据段加载出来时的原始内容和编辑中的内容。
    QByteArray m_originalPayload;
    QByteArray m_editablePayload;

    // 当前可编辑数据段所属的数据包全局索引。
    int m_editablePacketIndex = -1;

    // 当前数据段是否有尚未保存的修改。
    bool m_hexDirty = false;

    // 批量填充表格时阻止 itemChanged 信号触发业务逻辑。
    bool m_populatingHexTable = false;

    // 十六进制表格分块填充器（封装了定时器、重绘开关和信号屏蔽），
    // 避免 Aurora 大包一次性创建数万单元格造成界面卡顿。
    ChunkedTableFiller *m_hexFiller = nullptr;

    // 当前数据段的显示和编辑进制。
    NumberBase m_numberBase = NumberBase::Hexadecimal;
};
